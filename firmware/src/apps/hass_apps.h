#pragma once
#include "apps.h"
#include "../util.h"
#include "../events/events.h"
#include "../network/hass_image/hass_icon_loader.h"

#include <string>
#include <vector>

class HassApps : public Apps
{
public:
    HassApps(SemaphoreHandle_t mutex);

    void sync(cJSON *json_apps);
    void handleEvent(WiFiEvent event);
    void handleNavigationEvent(NavigationEvent event);
    void render();

    // Hides Apps::update to apply menu icons that finished loading first.
    EntityStateUpdate update(AppState state);

private:
    struct Icon
    {
        lv_img_dsc_t *big = nullptr;
        lv_img_dsc_t *small = nullptr;
        bool loading = false;
    };

    void requestIcon(uint8_t app_position, const char *url);
    void pollIcons();
    void applyIcon(uint8_t app_position, const Icon &icon);

    // Icons by URL, kept across syncs so a re-sync doesn't download them again.
    std::map<std::string, Icon> icons_;
    // Icon URL of each app position from the last sync.
    std::map<uint8_t, std::string> app_icon_urls_;
    // URLs the loader couldn't take yet because its queue was full.
    std::vector<std::string> icon_backlog_;

    lv_obj_t *waiting_for_hass = lv_obj_create(NULL);
};