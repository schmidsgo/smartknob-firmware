#pragma once
#include <WiFi.h>

#include "util.h"
#include "app_config.h"
#include "semaphore_guard.h"
#include "display/page_manager.h"
#include "navigation/navigation.h"
#include "notify/motor_notifier/motor_notifier.h"
#include "notify/wifi_notifier/wifi_notifier.h"

#include "./pages/error.h"
#include "./pages/reset.h"

// Motor config id used while the WiFi retry limit choice is on screen, so rotation is only routed to the error flow then.
static const char *ERROR_CHOICE_CONFIG_ID = "ERROR_CHOICE";

enum WiFiErrorOption
{
    WIFI_ERROR_OPTION_RETRY = 0,
    WIFI_ERROR_OPTION_SETUP,
    WIFI_ERROR_OPTION_COUNT
};

enum ErrorPages
{
    ERROR_PAGE,
    RESET_PAGE,
    COUNT
};

class ErrorHandlingPageManager : public PageManager<ErrorPages>
{
public:
    ErrorHandlingPageManager(lv_obj_t *parent, SemaphoreHandle_t mutex) : PageManager<ErrorPages>(parent, mutex)
    {
        add(ErrorPages::ERROR_PAGE, new ErrorPage(parent));
        add(ErrorPages::RESET_PAGE, new ResetPage(parent));
    }

    void render(ErrorPages page_enum)
    {
        PageManager::show(page_enum);
        lv_scr_load(parent_);
    }
};

class ErrorHandlingFlow
{
public:
    ErrorHandlingFlow(SemaphoreHandle_t mutex);

    void handleNavigationEvent(NavigationEvent event);
    void handleEvent(WiFiEvent event);
    void update(AppState state);
    void setMotorNotifier(MotorNotifier *motor_notifier);
    void setWiFiNotifier(WiFiNotifier *wifi_notifier);

    void setSharedEventsQueue(QueueHandle_t shared_event_queue);
    void publishEvent(WiFiEvent event);

    ErrorType getErrorType();

private:
    SemaphoreHandle_t mutex_;

    ErrorHandlingPageManager *page_manager = nullptr;

    lv_timer_t *timer;

    char buf_[64];

    int32_t current_position = 0;
    ErrorType error_type = NO_ERROR;
    uint8_t selected_option = WIFI_ERROR_OPTION_RETRY;

    bool isWiFiChoiceActive();

    ErrorState error_state = {
        NO_ERROR,
        SK_NO_EVENT,
        1,
    };

    PB_SmartKnobConfig root_level_motor_config;
    PB_SmartKnobConfig blocked_motor_config = PB_SmartKnobConfig{
        0,
        0,
        0,
        0,
        0,
        PI,
        0,
        1,
        0.5,
        "",
        0,
        {},
        0,
        0,
    };

    PB_SmartKnobConfig choice_motor_config = PB_SmartKnobConfig{
        WIFI_ERROR_OPTION_RETRY,
        0,
        WIFI_ERROR_OPTION_RETRY,
        0,
        WIFI_ERROR_OPTION_COUNT - 1,
        35 * PI / 180,
        2,
        1,
        0.55,
        "ERROR_CHOICE",
        0,
        {},
        0,
        20,
    };

    MotorNotifier *motor_notifier;
    WiFiNotifier *wifi_notifier;

    QueueHandle_t shared_events_queue;

    char ap_data[64];
    char ip_data[64];
};