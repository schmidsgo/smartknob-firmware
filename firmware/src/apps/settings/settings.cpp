#include "settings.h"
#include "sensors/press_threshold.h"

SettingsApp::SettingsApp(SemaphoreHandle_t mutex) : App(mutex)
{
    sprintf(app_id, "%s", "settings");
    sprintf(entity_id, "%s", "settings");
    sprintf(friendly_name, "%s", "Settings");

    motor_config = PB_SmartKnobConfig{
        0,
        0,
        0,
        0,
        SETTINGS_PAGE_COUNT - 1,
        35 * PI / 180,
        2,
        1,
        0.55, // Note the snap point is slightly past the midpoint (0.5); compare to normal detents which use a snap point *past* the next value (i.e. > 1)
        "",
        0,
        {},
        0,
        20,
    };
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    LV_IMG_DECLARE(x80_settings);
    LV_IMG_DECLARE(x40_settings);

    big_icon = x80_settings;
    small_icon = x40_settings;
}

SettingsPages getSettingsPageEnum(uint8_t screen)
{
    if (screen >= 0 && screen <= SETTINGS_PAGE_COUNT - 1)
    {
        return static_cast<SettingsPages>(screen);
    }
    return WIFI_PAGE_SETTINGS; // TODO handle error here instead of returning WIFI_PAGE_SETTINGS
}

EntityStateUpdate SettingsApp::updateStateFromKnob(PB_SmartKnobState state)
{
    EntityStateUpdate new_state;

    // After switching between page selection and editing, ignore knob states until the
    // motor runs the new config, otherwise an old position would be read in the new mode.
    if (awaiting_config)
    {
        if (state.config.position_nonce != motor_config.position_nonce)
        {
            return new_state;
        }
        awaiting_config = false;
    }

    if (editing_press)
    {
        int32_t steps = state.current_position < 0 ? 0 : state.current_position;
        uint16_t value = PRESS_WEIGHT_MIN + steps * PRESS_WEIGHT_STEP;
        if (value > PRESS_WEIGHT_MAX)
            value = PRESS_WEIGHT_MAX;
        if (value != press_edit_value)
        {
            press_edit_value = value;
            pressPage()->setValue(press_edit_value);
        }
        motor_config.position = state.current_position;
        return new_state;
    }

    if (current_position != state.current_position)
    {
        current_position = state.current_position;
        page_mgr->show(getSettingsPageEnum(current_position));
    }

    return new_state;
}

PressSettingsPage *SettingsApp::pressPage()
{
    return static_cast<PressSettingsPage *>(page_mgr->getPage(PRESS_PAGE_SETTINGS));
}

void SettingsApp::startPressEdit()
{
    uint8_t prev_nonce = motor_config.position_nonce;
    page_selector_config = motor_config;
    page_selector_config.position = current_position;

    press_edit_value = getPressWeight();
    motor_config = PB_SmartKnobConfig{
        .position = (press_edit_value - PRESS_WEIGHT_MIN) / PRESS_WEIGHT_STEP,
        .sub_position_unit = 0,
        .position_nonce = (uint8_t)(prev_nonce + 1),
        .min_position = 0,
        .max_position = (PRESS_WEIGHT_MAX - PRESS_WEIGHT_MIN) / PRESS_WEIGHT_STEP,
        .position_width_radians = 4 * PI / 180,
        .detent_strength_unit = 1,
        .endstop_strength_unit = 1,
        .snap_point = 1.1,
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
        .led_hue = 27,
    };
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    editing_press = true;
    awaiting_config = true;
    pressPage()->setValue(press_edit_value);
    pressPage()->setEditing(true);
}

void SettingsApp::endPressEdit(bool save)
{
    uint8_t prev_nonce = motor_config.position_nonce;

    if (save)
    {
        setPressWeight(press_edit_value);
    }

    editing_press = false;
    pressPage()->setValue(getPressWeight());
    pressPage()->setEditing(false);

    motor_config = page_selector_config;
    motor_config.position = PRESS_PAGE_SETTINGS;
    motor_config.position_nonce = prev_nonce + 1;
    current_position = PRESS_PAGE_SETTINGS;
    awaiting_config = true;
}

int8_t SettingsApp::navigationBack()
{
    // A long press while editing cancels the edit instead of leaving the app.
    if (editing_press)
    {
        return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
    }
    return App::navigationBack();
}

void SettingsApp::updateStateFromSystem(AppState state)
{
    page_mgr->getCurrentPage()->updateFromSystem(state);
}

void SettingsApp::handleNavigation(NavigationEvent event)
{
    if (page_mgr->getCurrentPageNum() == PRESS_PAGE_SETTINGS)
    {
        if (editing_press)
        {
            // Short press saves, long press cancels.
            endPressEdit(event == NavigationEvent::SHORT);
        }
        else if (event == NavigationEvent::SHORT)
        {
            startPressEdit();
        }
        return;
    }

    page_mgr->getCurrentPage()->handleNavigation(event);
}

void SettingsApp::setOSConfigNotifier(OSConfigNotifier *os_config_notifier)
{
    os_config_notifier_ = os_config_notifier;
    page_mgr = new SettingsPageManager(screen, mutex_, os_config_notifier_); // Should be created in the constructor moved here for fix of os_config_notifier_ being nullptr, UGLY
}