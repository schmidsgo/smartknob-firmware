#include "press.h"
#include "sensors/press_threshold.h"

// The strain sensor runs at 120 Hz, the labels do not need to follow that fast.
static const unsigned long PRESS_REFRESH_MS = 80;

static const int16_t BAR_WIDTH = 150;
static const int16_t BAR_HEIGHT = 10;
static const int16_t BAR_Y = 58;

static const lv_color_t COLOR_IDLE = LV_COLOR_MAKE(0x80, 0xFF, 0x50);
static const lv_color_t COLOR_EDIT = LV_COLOR_MAKE(0xFF, 0xB4, 0x50);
static const lv_color_t COLOR_GRAY = LV_COLOR_MAKE(0x72, 0x72, 0x72);
static const lv_color_t COLOR_TRACK = LV_COLOR_MAKE(0x30, 0x30, 0x30);

PressSettingsPage::PressSettingsPage(lv_obj_t *parent, SemaphoreHandle_t mutex) : BasePage(parent), mutex_(mutex)
{
    title_label = lv_label_create(page);
    lv_obj_set_style_text_font(title_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_label_set_text(title_label, "Press threshold");
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -64);

    value_label = lv_label_create(page);
    lv_obj_set_style_text_font(value_label, &roboto_light_mono_48pt, LV_PART_MAIN);
    lv_label_set_text(value_label, "");
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -22);

    prompt_label = lv_label_create(page);
    lv_obj_set_style_text_font(prompt_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_obj_align(prompt_label, LV_ALIGN_CENTER, 0, 22);

    // Live force of the current push, with a marker at the threshold.
    force_bar = lv_bar_create(page);
    lv_obj_set_size(force_bar, BAR_WIDTH, BAR_HEIGHT);
    lv_bar_set_range(force_bar, 0, PRESS_WEIGHT_MAX);
    lv_bar_set_value(force_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(force_bar, COLOR_TRACK, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(force_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(force_bar, COLOR_GRAY, LV_PART_INDICATOR);
    lv_obj_align(force_bar, LV_ALIGN_CENTER, 0, BAR_Y);

    threshold_marker = lv_obj_create(page);
    lv_obj_remove_style_all(threshold_marker);
    lv_obj_set_size(threshold_marker, 2, BAR_HEIGHT + 10);
    lv_obj_set_style_bg_opa(threshold_marker, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(threshold_marker, COLOR_IDLE, LV_PART_MAIN);

    force_label = lv_label_create(page);
    lv_obj_set_style_text_font(force_label, &aktivgrotesk_regular_12pt_8bpp_subpixel, LV_PART_MAIN);
    lv_obj_set_style_text_color(force_label, COLOR_GRAY, LV_PART_MAIN);
    lv_label_set_text(force_label, "push to test");
    lv_obj_align(force_label, LV_ALIGN_CENTER, 0, BAR_Y + 24);

    value_ = getPressWeight();
    lv_label_set_text_fmt(value_label, "%ug", (unsigned)value_);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -22);
    lv_label_set_text(prompt_label, "PRESS TO EDIT");
    lv_obj_set_style_text_color(prompt_label, COLOR_IDLE, LV_PART_MAIN);
    lv_obj_align(prompt_label, LV_ALIGN_CENTER, 0, 22);
    layoutMarker();
}

void PressSettingsPage::layoutMarker()
{
    int16_t x = (int32_t)value_ * BAR_WIDTH / PRESS_WEIGHT_MAX - BAR_WIDTH / 2;
    lv_obj_align(threshold_marker, LV_ALIGN_CENTER, x, BAR_Y);
}

void PressSettingsPage::setEditing(bool editing)
{
    SemaphoreGuard lock(mutex_);
    editing_ = editing;
    lv_color_t color = editing ? COLOR_EDIT : COLOR_IDLE;
    lv_label_set_text(prompt_label, editing ? "TURN, PRESS TO SAVE" : "PRESS TO EDIT");
    lv_obj_set_style_text_color(prompt_label, color, LV_PART_MAIN);
    lv_obj_align(prompt_label, LV_ALIGN_CENTER, 0, 22);
    lv_obj_set_style_text_color(value_label, editing ? COLOR_EDIT : lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(threshold_marker, color, LV_PART_MAIN);
}

void PressSettingsPage::setValue(uint16_t grams)
{
    SemaphoreGuard lock(mutex_);
    value_ = grams;
    lv_label_set_text_fmt(value_label, "%ug", (unsigned)grams);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -22);
    layoutMarker();
}

void PressSettingsPage::updateFromSystem(AppState state)
{
    if (millis() - last_update_ms < PRESS_REFRESH_MS)
    {
        return;
    }

    float raw = state.strain_state.raw_value;
    int16_t force = raw < 0 ? 0 : (raw > PRESS_WEIGHT_MAX ? PRESS_WEIGHT_MAX : (int16_t)raw);
    if (force == last_force_)
    {
        return;
    }
    last_update_ms = millis();
    last_force_ = force;

    SemaphoreGuard lock(mutex_);
    lv_bar_set_value(force_bar, force, LV_ANIM_OFF);
    // Green once the push would reach the shown threshold.
    lv_obj_set_style_bg_color(force_bar, force >= value_ ? COLOR_IDLE : COLOR_GRAY, LV_PART_INDICATOR);
    lv_label_set_text_fmt(force_label, "now %dg", (int)force);
    lv_obj_align(force_label, LV_ALIGN_CENTER, 0, BAR_Y + 24);
}
