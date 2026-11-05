#pragma once
#include "./display/page_manager.h"

// Shows and edits the strain weight at which a push counts as a press.
// The SettingsApp drives editing: it calls setEditing() and setValue() and
// changes the motor config so that turning the knob changes the value.
class PressSettingsPage : public BasePage
{
public:
    PressSettingsPage(lv_obj_t *parent, SemaphoreHandle_t mutex);
    void updateFromSystem(AppState state) override;

    void setEditing(bool editing);
    void setValue(uint16_t grams);

private:
    void layoutMarker();

    SemaphoreHandle_t mutex_;

    lv_obj_t *title_label;
    lv_obj_t *value_label;
    lv_obj_t *prompt_label;
    lv_obj_t *force_bar;
    lv_obj_t *threshold_marker;
    lv_obj_t *force_label;

    bool editing_ = false;
    uint16_t value_ = 0;

    int16_t last_force_ = INT16_MIN;
    unsigned long last_update_ms = 0;
};
