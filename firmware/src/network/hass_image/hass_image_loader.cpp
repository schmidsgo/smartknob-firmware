#include "hass_image_loader.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <logging.h>

HassImageLoader &HassImageLoader::instance()
{
    static HassImageLoader loader;
    return loader;
}

HassImageLoader::HassImageLoader()
{
    jobs_ = xQueueCreate(1, sizeof(Job));
    result_mutex_ = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(taskEntry, "HassImage", 6144, this, 1, NULL, 0);
}

void HassImageLoader::taskEntry(void *arg)
{
    static_cast<HassImageLoader *>(arg)->run();
}

void HassImageLoader::request(const char *owner, const char *url)
{
    Job job = {};
    snprintf(job.owner, sizeof(job.owner), "%s", owner);
    snprintf(job.url, sizeof(job.url), "%s", url);
    xQueueOverwrite(jobs_, &job);
}

lv_img_dsc_t *HassImageLoader::take(const char *owner, const char *url, bool *failed)
{
    lv_img_dsc_t *image = nullptr;
    *failed = false;

    xSemaphoreTake(result_mutex_, portMAX_DELAY);
    if (result_ready_ && strcmp(result_job_.owner, owner) == 0 && strcmp(result_job_.url, url) == 0)
    {
        image = result_image_;
        *failed = result_failed_;
        result_image_ = nullptr;
        result_ready_ = false;
    }
    xSemaphoreGive(result_mutex_);

    return image;
}

void HassImageLoader::free(lv_img_dsc_t *image)
{
    if (image == nullptr)
    {
        return;
    }
    heap_caps_free((void *)image->data);
    heap_caps_free(image);
}

void HassImageLoader::run()
{
    Job job;
    while (true)
    {
        if (xQueueReceive(jobs_, &job, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        lv_img_dsc_t *image = download(job.url);

        xSemaphoreTake(result_mutex_, portMAX_DELAY);
        // Drop an image nobody picked up.
        HassImageLoader::free(result_image_);
        result_job_ = job;
        result_image_ = image;
        result_failed_ = image == nullptr;
        result_ready_ = true;
        xSemaphoreGive(result_mutex_);
    }
}

lv_img_dsc_t *HassImageLoader::download(const char *url)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return nullptr;
    }
    if (strncmp(url, "http://", 7) != 0)
    {
        LOGW("Cover: only http:// URLs are supported, got %s", url);
        return nullptr;
    }

    const size_t expected = IMAGE_SIZE * IMAGE_SIZE * 2;

    HTTPClient http;
    http.setTimeout(8000);
    http.setConnectTimeout(4000);
    if (!http.begin(url))
    {
        LOGW("Cover: invalid URL");
        return nullptr;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        LOGW("Cover: HTTP %d", code);
        http.end();
        return nullptr;
    }

    int length = http.getSize();
    if (length >= 0 && (size_t)length != expected)
    {
        LOGW("Cover: unexpected size %d", length);
        http.end();
        return nullptr;
    }

    uint8_t *pixels = (uint8_t *)heap_caps_malloc(expected, MALLOC_CAP_SPIRAM);
    if (pixels == nullptr)
    {
        LOGE("Cover: out of memory");
        http.end();
        return nullptr;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t received = 0;
    unsigned long last_data = millis();
    while (received < expected && http.connected() && millis() - last_data < 5000)
    {
        size_t available = stream->available();
        if (available == 0)
        {
            delay(2);
            continue;
        }
        int read = stream->readBytes(pixels + received, min(available, expected - received));
        if (read > 0)
        {
            received += read;
            last_data = millis();
        }
    }
    // The last bytes can arrive together with the connection close.
    while (received < expected && stream->available())
    {
        int read = stream->readBytes(pixels + received, expected - received);
        if (read <= 0)
            break;
        received += read;
    }
    http.end();

    if (received != expected)
    {
        LOGW("Cover: got %u of %u bytes", (unsigned)received, (unsigned)expected);
        heap_caps_free(pixels);
        return nullptr;
    }

    lv_img_dsc_t *image = (lv_img_dsc_t *)heap_caps_calloc(1, sizeof(lv_img_dsc_t), MALLOC_CAP_SPIRAM);
    if (image == nullptr)
    {
        heap_caps_free(pixels);
        return nullptr;
    }
    image->header.always_zero = 0;
    image->header.w = IMAGE_SIZE;
    image->header.h = IMAGE_SIZE;
    image->header.cf = LV_IMG_CF_TRUE_COLOR;
    image->data_size = expected;
    image->data = pixels;

    LOGI("Cover loaded");
    return image;
}
