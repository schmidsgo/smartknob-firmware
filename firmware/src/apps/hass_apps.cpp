#include "hass_apps.h"

HassApps::HassApps(SemaphoreHandle_t mutex) : Apps(mutex)
{
    lv_obj_t *label = lv_label_create(waiting_for_hass);
    lv_label_set_text(label, "Waiting for Home Assistant");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
};
void HassApps::sync(cJSON *json_apps)
{
    clear();
    {
        SemaphoreGuard lock(app_mutex_);
        app_icon_urls_.clear();
    }
    uint16_t app_position = 0;

    cJSON *json_app_ = NULL;
    cJSON_ArrayForEach(json_app_, json_apps)
    {

        cJSON *json_app_slug = cJSON_GetObjectItemCaseSensitive(json_app_, "app_slug");
        cJSON *json_app_id = cJSON_GetObjectItemCaseSensitive(json_app_, "app_id");
        cJSON *json_entity_id = cJSON_GetObjectItemCaseSensitive(json_app_, "entity_id");
        cJSON *json_friendly_name = cJSON_GetObjectItemCaseSensitive(json_app_, "friendly_name");

        if (json_app_slug == NULL || json_app_id == NULL || json_entity_id == NULL || json_friendly_name == NULL)
        {
            LOGE("Invalid data.");
            continue;
        }

        App *app = loadApp(app_position, json_app_slug->valuestring, json_app_id->valuestring, json_friendly_name->valuestring, json_entity_id->valuestring);
        if (app == nullptr)
        {
            continue;
        }

        // The stopwatch ignores the name it is given; use the one from Home Assistant.
        if (json_friendly_name->valuestring[0] != '\0')
        {
            snprintf(app->friendly_name, sizeof(app->friendly_name), "%s", json_friendly_name->valuestring);
        }

        cJSON *json_icon_url = cJSON_GetObjectItemCaseSensitive(json_app_, "icon_url");
        if (cJSON_IsString(json_icon_url) && json_icon_url->valuestring[0] != '\0' &&
            strlen(json_icon_url->valuestring) < HassIconLoader::URL_SIZE)
        {
            app_icon_urls_[app_position] = json_icon_url->valuestring;
        }

        app_position++;
    }

    SettingsApp *settings_app = new SettingsApp(screen_mutex_);
    settings_app->setOSConfigNotifier(os_config_notifier_);
    add(app_position, settings_app);

    updateMenu();
    setMotorNotifier(motor_notifier);

    {
        SemaphoreGuard lock(app_mutex_);
        for (auto &entry : app_icon_urls_)
        {
            requestIcon(entry.first, entry.second.c_str());
        }
    }
    cJSON_Delete(json_apps); // DELETING DELETES POINTERS NEEDED TO DISPLAY FRIENDLY NAME ON APPS HMMMM
}
void HassApps::handleEvent(WiFiEvent event)
{
    SemaphoreGuard lock(app_mutex_);
    std::shared_ptr<App> app;

    switch (event.type)
    {
    case SK_MQTT_STATE_UPDATE:
        if (event.body.mqtt_state_update.all == true)
        {
            for (auto &app : apps)
            {

                if (strcmp(app.second->app_id, event.body.mqtt_state_update.app_id) != 0 && strcmp(app.second->entity_id, event.body.mqtt_state_update.entity_id) == 0)
                {
                    app.second->updateStateFromHASS(event.body.mqtt_state_update);
                }
            }
        }
        else
        {
            app = find(event.body.mqtt_state_update.app_id);
            if (app != nullptr)
            {
                app->updateStateFromHASS(event.body.mqtt_state_update);
                motor_notifier->requestUpdate(active_app->getMotorConfig());
            }
            else
            {
                LOGW("App not found");
            }
        }
        break;
    default:
        break;
    }
}
void HassApps::handleNavigationEvent(NavigationEvent event)
{
    if (active_app == nullptr || apps.size() <= 1) // 1 is menu which doesnt get removed when sync = 0 apps
    {
        return;
    }
    return Apps::handleNavigationEvent(event);
}

void HassApps::render()
{
    if (active_app == nullptr || apps.size() <= 1) // 1 is menu which doesnt get removed when sync = 0 apps
    {
        return lv_scr_load(waiting_for_hass);
    }
    return Apps::render();
}

EntityStateUpdate HassApps::update(AppState state)
{
    pollIcons();
    return Apps::update(state);
}

// Call with app_mutex_ held.
void HassApps::requestIcon(uint8_t app_position, const char *url)
{
    auto it = icons_.find(url);
    if (it != icons_.end())
    {
        if (it->second.big != nullptr)
        {
            applyIcon(app_position, it->second);
        }
        // Still loading: applied when it arrives. Failed earlier: try again below.
        if (it->second.loading || it->second.big != nullptr)
        {
            return;
        }
    }

    icons_[url].loading = true;
    if (!HassIconLoader::instance().request(url))
    {
        icon_backlog_.push_back(url);
    }
}

void HassApps::pollIcons()
{
    SemaphoreGuard lock(app_mutex_);

    while (!icon_backlog_.empty() && HassIconLoader::instance().request(icon_backlog_.back().c_str()))
    {
        icon_backlog_.pop_back();
    }

    HassIconLoader::Result result;
    while (HassIconLoader::instance().poll(&result))
    {
        Icon &icon = icons_[result.url];
        icon.loading = false;
        if (result.big == nullptr)
        {
            LOGW("Icon not loaded, keeping built-in icon");
            continue;
        }
        icon.big = result.big;
        icon.small = result.small;

        for (auto &entry : app_icon_urls_)
        {
            if (entry.second == result.url)
            {
                applyIcon(entry.first, icon);
            }
        }
    }
}

// Call with app_mutex_ held.
void HassApps::applyIcon(uint8_t app_position, const Icon &icon)
{
    auto app = apps.find(app_position);
    if (app == apps.end() || app->second == nullptr)
    {
        return;
    }
    // Only the menu changes; apps keep their own icons on their screens (e.g. the
    // switch's on/off pair).
    if (menu != nullptr)
    {
        menu->setAppIcons(app_position, icon.big, icon.small);
    }
}
