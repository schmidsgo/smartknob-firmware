#pragma once

#include <Arduino.h>
#include <lvgl.h>

// Downloads album covers that the Home Assistant integration prepares for the knob
// (240x240 raw RGB565, see cover_art.py in the integration) in a background task.
//
// Apps call request() with a URL and later poll take() from their own task. The
// returned image and its pixel buffer live in PSRAM and belong to the caller, who
// frees them with HassImageLoader::free().
class HassImageLoader
{
public:
    static const uint16_t IMAGE_SIZE = 240;
    static const size_t URL_SIZE = 384;

    static HassImageLoader &instance();

    // Replaces any request that has not started yet.
    void request(const char *owner, const char *url);

    // Returns the finished image for owner and url, or nullptr if there is none (yet).
    // A failed download is reported through *failed so the caller can retry later.
    lv_img_dsc_t *take(const char *owner, const char *url, bool *failed);

    static void free(lv_img_dsc_t *image);

private:
    HassImageLoader();
    static void taskEntry(void *arg);
    void run();
    lv_img_dsc_t *download(const char *url);

    struct Job
    {
        char owner[64];
        char url[URL_SIZE];
    };

    QueueHandle_t jobs_;
    SemaphoreHandle_t result_mutex_;

    // Last finished job; only one at a time, a newer one replaces an unclaimed image.
    Job result_job_ = {};
    lv_img_dsc_t *result_image_ = nullptr;
    bool result_ready_ = false;
    bool result_failed_ = false;
};
