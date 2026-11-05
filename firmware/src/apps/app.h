#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "semaphore_guard.h"
#include "../proto/proto_gen/smartknob.pb.h"
#include "../app_config.h"
#include "assets/images/icons.h"
#include "../events/events.h"
#include "../notify/motor_notifier/motor_notifier.h"
#include "navigation/navigation.h"
#include "../util.h"
#include "../led_ring/led_ring_scene.h"

const char APP_SLUG_CLIMATE[48] = "climate";
const char APP_SLUG_BLINDS[48] = "blinds";
const char APP_SLUG_MUSIC[48] = "music";
const char APP_SLUG_3D_PRINTER[48] = "3d_printer";
const char APP_SLUG_LIGHT_DIMMER[48] = "light_dimmer";
const char APP_SLUG_LIGHT_SWITCH[48] = "light_switch";
const char APP_SLUG_SWITCH[48] = "switch";
const char APP_SLUG_STOPWATCH[48] = "stopwatch";
const char APP_SLUG_SPOTIFY[48] = "spotify";
const char APP_SLUG_MEDIA_PLAYER[48] = "media_player";

enum SharedAppIds : int8_t
{
    MENU = -2,
    DONT_NAVIGATE = -1,
    DONT_NAVIGATE_UPDATE_MOTOR_CONFIG = -3,
    USE_NEW_IMPLEMENTATION = -4,
};

class App
{
public:
    App(SemaphoreHandle_t mutex);

    App(SemaphoreHandle_t mutex, int8_t next, int8_t back);
    // Virtual so apps held as std::shared_ptr<App> can release their own resources.
    virtual ~App() {}
    void render();

    virtual EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) { return EntityStateUpdate(); };
    virtual void updateStateFromHASS(MQTTStateUpdate mqtt_state_update) {};
    virtual void updateStateFromSystem(AppState state) {};

    // What the LED ring should show while this app is open. Default: faint background light.
    virtual LedRingHint getLedRingHint() { return LedRingHint(); };

    virtual void handleNavigation(NavigationEvent event) {
        // DO NOTHING BY DEFAULT
    };

    void setMotorNotifier(MotorNotifier *motor_notifier);

    void triggerMotorConfigUpdate();

    virtual int8_t navigationNext()
    {
        return next_;
    }
    void setNext(int8_t next);
    virtual int8_t navigationBack()
    {
        return back_;
    }
    void setBack(int8_t back);

    PB_SmartKnobConfig getMotorConfig();

    std::string getClassName();

    lv_img_dsc_t small_icon;
    lv_img_dsc_t big_icon;
    char friendly_name[256] = "";
    char app_id[256] = "";
    char entity_id[256] = "";

protected:
    virtual void initScreen() {};

    SemaphoreHandle_t mutex_;
    int8_t next_ = DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
    int8_t back_ = MENU;
    PB_SmartKnobConfig motor_config;

    PB_SmartKnobConfig root_level_motor_config;
    PB_SmartKnobConfig blocked_motor_config = {
        .endstop_strength_unit = 1,
        .snap_point = 0.5,
    };

    bool state_sent_from_hass = false;

    MotorNotifier *motor_notifier;

    lv_obj_t *screen = lv_obj_create(NULL);
};