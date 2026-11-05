#pragma once
#include "../app.h"
#include "../../util.h"

// Controls a Home Assistant media_player entity (Sonos, Chromecast, Spotify Connect via HA, ...).
//
// Volume page (start):  turn = volume, short press = play/pause, long press = track page
// Track page:           turn one detent = next / previous track, short press = play/pause,
//                       long press = back to the menu
//
// From the knob it sends {"volume": 0..100} or {"action": "play"|"pause"|"next"|"previous"}.
// From Home Assistant it expects {"state", "volume", "muted", "title", "artist", "cover"}; every field
// is optional. "cover" is a URL to a 240x240 RGB565 image prepared by the integration.
class MediaPlayerApp : public App
{
public:
    MediaPlayerApp(SemaphoreHandle_t mutex, char *app_id, char *friendly_name, char *entity_id);
    ~MediaPlayerApp() override;

    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    void updateStateFromHASS(MQTTStateUpdate mqtt_state_update) override;
    void handleNavigation(NavigationEvent event) override;
    int8_t navigationNext() override;
    int8_t navigationBack() override;
    LedRingHint getLedRingHint() override;

private:
    enum Page
    {
        VOLUME_PAGE = 0,
        TRACK_PAGE,
    };

    void initScreen() override;
    void showPage(Page page);
    void applyMotorConfig(PB_SmartKnobConfig config, int32_t position);
    EntityStateUpdate makeUpdate(const char *key, const char *action, int32_t volume);

    void requestCover();
    void pollCover();
    void setCover(lv_img_dsc_t *image); // takes the screen mutex itself

    // UI helpers, call with mutex_ held.
    void renderSkipProgress(int16_t progress);
    void renderVolume();
    void renderPlaying();
    void renderTrack();

    PB_SmartKnobConfig volume_config_ = PB_SmartKnobConfig{
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 0,
        .min_position = 0,
        .max_position = 100,
        .position_width_radians = 2.4 * PI / 180,
        .detent_strength_unit = 1,
        .endstop_strength_unit = 1,
        .snap_point = 1.1,
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
        .led_hue = 200,
    };

    // Three positions: -1 = previous, 0 = rest, 1 = next. The knob is pushed back to 0 after each skip.
    PB_SmartKnobConfig track_config_ = PB_SmartKnobConfig{
        .position = 0,
        .sub_position_unit = 0,
        .position_nonce = 0,
        .min_position = -1,
        .max_position = 1,
        .position_width_radians = 35 * PI / 180,
        .detent_strength_unit = 2.5,
        .endstop_strength_unit = 1,
        .snap_point = 0.7,
        .detent_positions_count = 0,
        .detent_positions = {},
        .snap_point_bias = 0,
        .led_hue = 200,
    };

    Page page_ = VOLUME_PAGE;

    // Play/pause chosen by a press, sent with the next knob state.
    const char *volatile pending_action_ = nullptr;

    int32_t volume_ = 0; // 0..100
    int32_t last_position_ = 0;
    bool is_playing_ = false;
    bool is_muted_ = false;
    bool volume_known_ = false;
    char player_state_[24] = "";
    char title_[96] = "";
    char artist_[64] = "";

    char cover_url_[384] = "";
    lv_img_dsc_t *cover_ = nullptr;
    bool cover_pending_ = false;
    uint8_t cover_tries_ = 0;
    unsigned long cover_requested_at_ = 0;

    int16_t skip_progress_ = 0;
    unsigned long skip_flash_until_ = 0;

    // Ignore knob states that still belong to an older motor config (after a page switch,
    // a skip or a volume change from Home Assistant).
    bool awaiting_config_ = false;
    unsigned long config_sent_at_ = 0;

    // Home Assistant echoes our own volume changes with a delay. While the knob is being
    // turned those echoes would yank the knob back, so they are ignored for a moment.
    unsigned long last_local_volume_ms_ = 0;

    lv_obj_t *volume_page_ = nullptr;
    lv_obj_t *volume_arc_ = nullptr;
    lv_obj_t *volume_label_ = nullptr;
    lv_obj_t *play_label_ = nullptr;
    lv_obj_t *title_label_ = nullptr;
    lv_obj_t *artist_label_ = nullptr;

    lv_obj_t *cover_img_ = nullptr;
    lv_obj_t *cover_shade_ = nullptr;

    lv_obj_t *track_page_ = nullptr;
    lv_obj_t *skip_arc_ = nullptr;
    lv_obj_t *prev_label_ = nullptr;
    lv_obj_t *next_label_ = nullptr;
    lv_obj_t *track_play_label_ = nullptr;
    lv_obj_t *track_title_label_ = nullptr;
    lv_obj_t *track_artist_label_ = nullptr;
};
