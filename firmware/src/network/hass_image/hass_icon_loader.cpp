#include "hass_icon_loader.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <logging.h>

static const size_t BIG_BYTES = (HassIconLoader::BIG_SIZE + 1) / 2 * HassIconLoader::BIG_SIZE;
static const size_t SMALL_BYTES = (HassIconLoader::SMALL_SIZE + 1) / 2 * HassIconLoader::SMALL_SIZE;
static const size_t ICON_BYTES = BIG_BYTES + SMALL_BYTES;

static const uint8_t MAX_TRIES = 3;
static const uint8_t QUEUE_LENGTH = 16;

HassIconLoader &HassIconLoader::instance()
{
    static HassIconLoader loader;
    return loader;
}

HassIconLoader::HassIconLoader()
{
    jobs_ = xQueueCreate(QUEUE_LENGTH, sizeof(Job));
    results_ = xQueueCreate(QUEUE_LENGTH, sizeof(Result));
    xTaskCreatePinnedToCore(taskEntry, "HassIcon", 4096, this, 1, NULL, 0);
}

void HassIconLoader::taskEntry(void *arg)
{
    static_cast<HassIconLoader *>(arg)->run();
}

bool HassIconLoader::request(const char *url)
{
    Job job = {};
    snprintf(job.url, sizeof(job.url), "%s", url);
    return xQueueSendToBack(jobs_, &job, 0) == pdTRUE;
}

bool HassIconLoader::poll(Result *result)
{
    return xQueueReceive(results_, result, 0) == pdTRUE;
}

void HassIconLoader::run()
{
    Job job;
    while (true)
    {
        if (xQueueReceive(jobs_, &job, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        Result result = {};
        snprintf(result.url, sizeof(result.url), "%s", job.url);
        for (uint8_t attempt = 0; attempt < MAX_TRIES; attempt++)
        {
            if (download(job.url, &result))
            {
                break;
            }
            delay(2000 * (attempt + 1));
        }
        xQueueSendToBack(results_, &result, portMAX_DELAY);
    }
}

static lv_img_dsc_t *makeImage(uint8_t *data, uint16_t size, size_t bytes)
{
    lv_img_dsc_t *image = (lv_img_dsc_t *)heap_caps_calloc(1, sizeof(lv_img_dsc_t), MALLOC_CAP_SPIRAM);
    if (image == nullptr)
    {
        return nullptr;
    }
    image->header.always_zero = 0;
    image->header.w = size;
    image->header.h = size;
    image->header.cf = LV_IMG_CF_ALPHA_4BIT;
    image->data_size = bytes;
    image->data = data;
    return image;
}

bool HassIconLoader::download(const char *url, Result *result)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }
    if (strncmp(url, "http://", 7) != 0)
    {
        LOGW("Icon: only http:// URLs are supported");
        return false;
    }

    HTTPClient http;
    http.setTimeout(6000);
    http.setConnectTimeout(4000);
    if (!http.begin(url))
    {
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK)
    {
        LOGW("Icon: HTTP %d for %s", code, url);
        http.end();
        return false;
    }

    int length = http.getSize();
    if (length >= 0 && (size_t)length != ICON_BYTES)
    {
        LOGW("Icon: unexpected size %d", length);
        http.end();
        return false;
    }

    uint8_t *pixels = (uint8_t *)heap_caps_malloc(ICON_BYTES, MALLOC_CAP_SPIRAM);
    if (pixels == nullptr)
    {
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t received = 0;
    unsigned long last_data = millis();
    while (received < ICON_BYTES && millis() - last_data < 4000)
    {
        size_t available = stream->available();
        if (available == 0)
        {
            if (!http.connected())
                break;
            delay(2);
            continue;
        }
        int read = stream->readBytes(pixels + received, min(available, ICON_BYTES - received));
        if (read > 0)
        {
            received += read;
            last_data = millis();
        }
    }
    http.end();

    if (received != ICON_BYTES)
    {
        LOGW("Icon: got %u of %u bytes", (unsigned)received, (unsigned)ICON_BYTES);
        heap_caps_free(pixels);
        return false;
    }

    result->big = makeImage(pixels, BIG_SIZE, BIG_BYTES);
    result->small = makeImage(pixels + BIG_BYTES, SMALL_SIZE, SMALL_BYTES);
    if (result->big == nullptr || result->small == nullptr)
    {
        heap_caps_free(result->big);
        heap_caps_free(result->small);
        heap_caps_free(pixels);
        result->big = nullptr;
        result->small = nullptr;
        return false;
    }
    return true;
}
