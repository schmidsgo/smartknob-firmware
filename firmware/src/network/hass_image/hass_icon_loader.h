#pragma once

#include <Arduino.h>
#include <lvgl.h>

// Downloads the menu icons that the Home Assistant integration draws for each app
// (see app_icons.py in the integration): an 80x80 and a 40x40 LV_IMG_CF_ALPHA_4BIT
// image back to back, 4000 bytes in total.
//
// request() queues a URL; poll() hands back finished icons from the caller's task.
// Icons live in PSRAM and are never freed: the caller caches them by URL, and the
// handful of distinct icons a knob uses is small.
class HassIconLoader
{
public:
    static const uint16_t BIG_SIZE = 80;
    static const uint16_t SMALL_SIZE = 40;
    static const size_t URL_SIZE = 192;

    struct Result
    {
        char url[URL_SIZE];
        lv_img_dsc_t *big;   // nullptr if the download failed
        lv_img_dsc_t *small; // nullptr if the download failed
    };

    static HassIconLoader &instance();

    // Returns false if the queue is full; the caller can try again later.
    bool request(const char *url);

    // Returns true and fills *result when an icon (or a failure) is ready.
    bool poll(Result *result);

private:
    HassIconLoader();
    static void taskEntry(void *arg);
    void run();
    bool download(const char *url, Result *result);

    struct Job
    {
        char url[URL_SIZE];
    };

    QueueHandle_t jobs_;
    QueueHandle_t results_;
};
