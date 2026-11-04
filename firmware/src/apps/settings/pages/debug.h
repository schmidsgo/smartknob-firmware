#pragma once
#include "./display/page_manager.h"

// Shows live sensor readings, currently the VL53L0X proximity sensor.
class DebugSettingsPage : public BasePage
{
public:
    DebugSettingsPage(lv_obj_t *parent);
    void updateFromSystem(AppState state) override;

private:
    lv_obj_t *distance_label;
    lv_obj_t *status_label;
    lv_obj_t *engaged_label;
    lv_obj_t *raw_label;
    lv_obj_t *baseline_label;

    uint16_t last_range_mm = UINT16_MAX;
    uint8_t last_status = UINT8_MAX;
    float last_signal_rate = -1;
    bool last_present = false;
    unsigned long last_update_ms = 0;
};
