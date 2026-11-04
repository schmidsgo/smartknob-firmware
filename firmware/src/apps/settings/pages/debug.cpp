#include "debug.h"

// Refresh the labels at most this often, the sensor itself runs at 20 Hz.
static const unsigned long DEBUG_REFRESH_MS = 100;

static const char *rangeStatusText(uint8_t status)
{
    // VL53L0X RangeStatus codes as reported by the Adafruit driver.
    switch (status)
    {
    case 0:
        return "0 VALID";
    case 1:
        return "1 SIGMA FAIL";
    case 2:
        return "2 SIGNAL FAIL";
    case 3:
        return "3 MIN RANGE";
    case 4:
        return "4 OUT OF RANGE";
    default:
        return "UNKNOWN";
    }
}

DebugSettingsPage::DebugSettingsPage(lv_obj_t *parent) : BasePage(parent)
{
    lv_obj_t *title = lv_label_create(page);
    lv_obj_set_style_text_font(title, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_label_set_text(title, "PROXIMITY");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -72);

    distance_label = lv_label_create(page);
    lv_obj_set_style_text_font(distance_label, &roboto_light_mono_48pt, LV_PART_MAIN);
    lv_label_set_text(distance_label, "--");
    lv_obj_align(distance_label, LV_ALIGN_CENTER, 0, -24);

    status_label = lv_label_create(page);
    lv_obj_set_style_text_font(status_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_label_set_text(status_label, "");
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 16);

    engaged_label = lv_label_create(page);
    lv_obj_set_style_text_font(engaged_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_label_set_text(engaged_label, "");
    lv_obj_align(engaged_label, LV_ALIGN_CENTER, 0, 38);

    raw_label = lv_label_create(page);
    lv_obj_set_style_text_font(raw_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_obj_set_style_text_color(raw_label, lv_color_make(0x72, 0x72, 0x72), LV_PART_MAIN);
    lv_label_set_text(raw_label, "");
    lv_obj_align(raw_label, LV_ALIGN_CENTER, 0, 62);

    baseline_label = lv_label_create(page);
    lv_obj_set_style_text_font(baseline_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_obj_set_style_text_color(baseline_label, lv_color_make(0x72, 0x72, 0x72), LV_PART_MAIN);
    lv_label_set_text(baseline_label, "");
    lv_obj_align(baseline_label, LV_ALIGN_CENTER, 0, 84);
}

void DebugSettingsPage::updateFromSystem(AppState state)
{
    uint16_t range_mm = state.proximiti_state.RangeMilliMeter;
    uint8_t status = state.proximiti_state.RangeStatus;

    float signal_rate = state.proximiti_state.SignalRateMcps;

    bool present = state.proximiti_state.Present;

    if (range_mm == last_range_mm && status == last_status && signal_rate == last_signal_rate && present == last_present)
    {
        return;
    }
    if (millis() - last_update_ms < DEBUG_REFRESH_MS)
    {
        return;
    }
    last_update_ms = millis();
    last_range_mm = range_mm;
    last_status = status;
    last_signal_rate = signal_rate;
    last_present = present;

    // The sensors task subtracts a 10 mm offset from an unsigned value,
    // so raw readings below 10 mm wrap around to values near 65535.
    if (range_mm > 60000)
    {
        lv_label_set_text_fmt(distance_label, "-%umm", (unsigned)(UINT16_MAX + 1 - range_mm));
    }
    else
    {
        lv_label_set_text_fmt(distance_label, "%umm", (unsigned)range_mm);
    }
    lv_obj_align(distance_label, LV_ALIGN_CENTER, 0, -24);

    lv_label_set_text(status_label, rangeStatusText(status));
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 16);

    lv_label_set_text(engaged_label, present ? "PRESENT - WAKES" : "ABSENT");
    lv_obj_set_style_text_color(engaged_label, present ? lv_color_make(0x80, 0xFF, 0x50) : lv_color_make(0x72, 0x72, 0x72), LV_PART_MAIN);
    lv_obj_align(engaged_label, LV_ALIGN_CENTER, 0, 38);

    // Raw distance and return signal rate: a high signal rate with nothing
    // in front of the knob means the case reflects light back (crosstalk).
    // LVGL's printf has no float support here, so format with snprintf.
    char raw_text[40];
    snprintf(raw_text, sizeof(raw_text), "RAW %umm  SIG %.2f", (unsigned)state.proximiti_state.RawRangeMilliMeter, signal_rate);
    lv_label_set_text(raw_label, raw_text);
    lv_obj_align(raw_label, LV_ALIGN_CENTER, 0, 62);

    // Learned idle baseline and how far the filtered signal is above it.
    snprintf(raw_text, sizeof(raw_text), "BASE %.2f  %+.2f", state.proximiti_state.SignalBaselineMcps,
             state.proximiti_state.SignalSmoothedMcps - state.proximiti_state.SignalBaselineMcps);
    lv_label_set_text(baseline_label, raw_text);
    lv_obj_align(baseline_label, LV_ALIGN_CENTER, 0, 84);
}
