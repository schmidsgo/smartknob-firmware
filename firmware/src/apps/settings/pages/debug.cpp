#include "debug.h"

// Same thresholds as the screen wake-up logic in RootTask.
static const uint16_t ENGAGE_DISTANCE_MM = 200;
static const uint8_t ENGAGE_MAX_STATUS = 3;
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
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -56);

    distance_label = lv_label_create(page);
    lv_obj_set_style_text_font(distance_label, &roboto_light_mono_48pt, LV_PART_MAIN);
    lv_label_set_text(distance_label, "--");
    lv_obj_align(distance_label, LV_ALIGN_CENTER, 0, -8);

    status_label = lv_label_create(page);
    lv_obj_set_style_text_font(status_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_label_set_text(status_label, "");
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 36);

    engaged_label = lv_label_create(page);
    lv_obj_set_style_text_font(engaged_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_label_set_text(engaged_label, "");
    lv_obj_align(engaged_label, LV_ALIGN_CENTER, 0, 60);
}

void DebugSettingsPage::updateFromSystem(AppState state)
{
    uint16_t range_mm = state.proximiti_state.RangeMilliMeter;
    uint8_t status = state.proximiti_state.RangeStatus;

    if (range_mm == last_range_mm && status == last_status)
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
    lv_obj_align(distance_label, LV_ALIGN_CENTER, 0, -8);

    lv_label_set_text(status_label, rangeStatusText(status));
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 36);

    bool engaged = status < ENGAGE_MAX_STATUS && range_mm < ENGAGE_DISTANCE_MM;
    lv_label_set_text(engaged_label, engaged ? "WAKES SCREEN" : "NO WAKE");
    lv_obj_set_style_text_color(engaged_label, engaged ? LV_COLOR_MAKE(0x80, 0xFF, 0x50) : LV_COLOR_MAKE(0x72, 0x72, 0x72), LV_PART_MAIN);
    lv_obj_align(engaged_label, LV_ALIGN_CENTER, 0, 60);
}
