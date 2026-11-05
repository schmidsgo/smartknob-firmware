#include "media_player.h"
#include "cJSON.h"
#include <cstring>

static const uint32_t MEDIA_PLAYER_COLOR = 0x4FC3F7;
static const unsigned long VOLUME_ECHO_IGNORE_MS = 2000;
static const unsigned long CONFIG_WAIT_MS = 500;

MediaPlayerApp::MediaPlayerApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_) : App(mutex)
{
    snprintf(app_id, sizeof(app_id), "%s", app_id_);
    snprintf(friendly_name, sizeof(friendly_name), "%s", friendly_name_);
    snprintf(entity_id, sizeof(entity_id), "%s", entity_id_);

    strncpy(volume_config_.id, app_id, sizeof(volume_config_.id) - 1);
    strncpy(track_config_.id, app_id, sizeof(track_config_.id) - 1);

    motor_config = volume_config_;

    LV_IMG_DECLARE(x80_media_player);
    LV_IMG_DECLARE(x40_media_player);

    big_icon = x80_media_player;
    small_icon = x40_media_player;

    initScreen();
}

static lv_obj_t *createPage(lv_obj_t *parent)
{
    lv_obj_t *page = lv_obj_create(parent);
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, LV_HOR_RES, LV_VER_RES);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    return page;
}

static lv_obj_t *createTextLabel(lv_obj_t *parent, lv_coord_t width, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "");
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, &aktivgrotesk_regular_10pt, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, y);
    return label;
}

void MediaPlayerApp::initScreen()
{
    SemaphoreGuard lock(mutex_);

    // --- Volume page ---
    volume_page_ = createPage(screen);

    volume_arc_ = lv_arc_create(volume_page_);
    lv_obj_set_size(volume_arc_, 220, 220);
    lv_arc_set_rotation(volume_arc_, 150);
    lv_arc_set_bg_angles(volume_arc_, 0, 240);
    lv_arc_set_range(volume_arc_, 0, 100);
    lv_arc_set_value(volume_arc_, 0);
    lv_obj_remove_style(volume_arc_, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(volume_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(volume_arc_, dark_arc_bg, LV_PART_MAIN);
    lv_obj_set_style_arc_color(volume_arc_, lv_color_hex(MEDIA_PLAYER_COLOR), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(volume_arc_, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_width(volume_arc_, 14, LV_PART_INDICATOR);
    lv_obj_center(volume_arc_);

    lv_obj_t *name_label = createTextLabel(volume_page_, 150, -62);
    lv_label_set_text(name_label, friendly_name);

    volume_label_ = lv_label_create(volume_page_);
    lv_obj_set_style_text_font(volume_label_, &roboto_light_mono_48pt, 0);
    lv_label_set_text(volume_label_, "--");
    lv_obj_align(volume_label_, LV_ALIGN_CENTER, 0, -18);

    play_label_ = lv_label_create(volume_page_);
    lv_obj_set_style_text_font(play_label_, &lv_font_montserrat_22, 0);
    lv_label_set_text(play_label_, LV_SYMBOL_PLAY);
    lv_obj_align(play_label_, LV_ALIGN_CENTER, 0, 22);

    title_label_ = createTextLabel(volume_page_, 150, 50);
    artist_label_ = createTextLabel(volume_page_, 120, 66);
    lv_obj_set_style_text_color(artist_label_, LV_COLOR_MAKE(0x99, 0x99, 0x99), 0);

    // --- Track page ---
    track_page_ = createPage(screen);
    lv_obj_add_flag(track_page_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *prev_label = lv_label_create(track_page_);
    lv_obj_set_style_text_font(prev_label, &lv_font_montserrat_22, 0);
    lv_label_set_text(prev_label, LV_SYMBOL_PREV);
    lv_obj_align(prev_label, LV_ALIGN_LEFT_MID, 22, 0);

    lv_obj_t *next_label = lv_label_create(track_page_);
    lv_obj_set_style_text_font(next_label, &lv_font_montserrat_22, 0);
    lv_label_set_text(next_label, LV_SYMBOL_NEXT);
    lv_obj_align(next_label, LV_ALIGN_RIGHT_MID, -22, 0);

    track_play_label_ = lv_label_create(track_page_);
    lv_obj_set_style_text_font(track_play_label_, &lv_font_montserrat_22, 0);
    lv_label_set_text(track_play_label_, LV_SYMBOL_PLAY);
    lv_obj_align(track_play_label_, LV_ALIGN_CENTER, 0, -40);

    track_title_label_ = createTextLabel(track_page_, 140, -4);
    track_artist_label_ = createTextLabel(track_page_, 130, 14);
    lv_obj_set_style_text_color(track_artist_label_, LV_COLOR_MAKE(0x99, 0x99, 0x99), 0);

    lv_obj_t *hint_label = createTextLabel(track_page_, 150, 58);
    lv_label_set_text(hint_label, "Turn to skip\nHold for menu");
    lv_obj_set_style_text_color(hint_label, LV_COLOR_MAKE(0x77, 0x77, 0x77), 0);

    renderTrack();
}

void MediaPlayerApp::renderVolume()
{
    lv_arc_set_value(volume_arc_, volume_);
    if (!volume_known_)
    {
        lv_label_set_text(volume_label_, "--");
    }
    else if (is_muted_)
    {
        lv_label_set_text(volume_label_, "Mute");
    }
    else
    {
        lv_label_set_text_fmt(volume_label_, "%d", (int)volume_);
    }
    lv_obj_align(volume_label_, LV_ALIGN_CENTER, 0, -18);
    lv_obj_set_style_arc_color(volume_arc_, is_muted_ ? LV_COLOR_MAKE(0x60, 0x60, 0x60) : lv_color_hex(MEDIA_PLAYER_COLOR), LV_PART_INDICATOR);
    lv_obj_invalidate(volume_arc_);
}

void MediaPlayerApp::renderPlaying()
{
    const char *symbol = is_playing_ ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    lv_label_set_text(play_label_, symbol);
    lv_label_set_text(track_play_label_, symbol);
}

void MediaPlayerApp::renderTrack()
{
    const char *title = title_;
    const char *artist = artist_;

    if (strcmp(player_state_, "off") == 0)
    {
        title = "Off";
        artist = "";
    }
    else if (strcmp(player_state_, "unavailable") == 0)
    {
        title = "Unavailable";
        artist = "";
    }
    else if (title_[0] == '\0')
    {
        title = "Nothing playing";
    }

    lv_label_set_text(title_label_, title);
    lv_label_set_text(artist_label_, artist);
    lv_label_set_text(track_title_label_, title);
    lv_label_set_text(track_artist_label_, artist);
}

void MediaPlayerApp::applyMotorConfig(PB_SmartKnobConfig config, int32_t position)
{
    uint8_t nonce = motor_config.position_nonce + 1;
    motor_config = config;
    motor_config.position = position;
    motor_config.position_nonce = nonce;

    last_position_ = position;
    awaiting_config_ = true;
    config_sent_at_ = millis();
}

void MediaPlayerApp::showPage(Page page)
{
    page_ = page;
    {
        SemaphoreGuard lock(mutex_);
        if (page == VOLUME_PAGE)
        {
            lv_obj_clear_flag(volume_page_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(track_page_, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(volume_page_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(track_page_, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (page == VOLUME_PAGE)
    {
        applyMotorConfig(volume_config_, volume_);
    }
    else
    {
        applyMotorConfig(track_config_, 0);
    }
    // Apps::handleNavigationEvent pushes motor_config to the motor after this.
}

EntityStateUpdate MediaPlayerApp::makeUpdate(const char *key, const char *action, int32_t volume)
{
    EntityStateUpdate new_state;
    snprintf(new_state.app_id, sizeof(new_state.app_id), "%s", app_id);
    snprintf(new_state.entity_id, sizeof(new_state.entity_id), "%s", entity_id);
    snprintf(new_state.app_slug, sizeof(new_state.app_slug), "%s", APP_SLUG_MEDIA_PLAYER);

    cJSON *json = cJSON_CreateObject();
    if (action != nullptr)
    {
        cJSON_AddStringToObject(json, key, action);
    }
    else
    {
        cJSON_AddNumberToObject(json, key, volume);
    }
    char *json_str = cJSON_PrintUnformatted(json);
    snprintf(new_state.state, sizeof(new_state.state), "%s", json_str);
    cJSON_free(json_str);
    cJSON_Delete(json);

    new_state.changed = true;
    return new_state;
}

EntityStateUpdate MediaPlayerApp::updateStateFromKnob(PB_SmartKnobState state)
{
    EntityStateUpdate new_state;

    if (pending_action_ != nullptr)
    {
        const char *action = pending_action_;
        pending_action_ = nullptr;
        return makeUpdate("action", action, 0);
    }

    if (awaiting_config_)
    {
        if (state.config.position_nonce != motor_config.position_nonce && millis() - config_sent_at_ < CONFIG_WAIT_MS)
        {
            return new_state;
        }
        awaiting_config_ = false;
        last_position_ = state.current_position;
        if (page_ == VOLUME_PAGE)
        {
            motor_config.position = state.current_position;
        }
        return new_state;
    }

    int32_t position = state.current_position;
    if (position == last_position_)
    {
        return new_state;
    }
    last_position_ = position;

    if (page_ == VOLUME_PAGE)
    {
        volume_ = constrain(position, 0, 100);
        volume_known_ = true;
        is_muted_ = false;
        motor_config.position = position;
        volume_config_.position = position;
        last_local_volume_ms_ = millis();
        {
            SemaphoreGuard lock(mutex_);
            renderVolume();
        }
        return makeUpdate("volume", nullptr, volume_);
    }

    // Track page: one detent to either side skips, then the knob springs back to the middle.
    if (position == 0)
    {
        return new_state;
    }
    const char *action = position > 0 ? "next" : "previous";
    applyMotorConfig(track_config_, 0);
    if (motor_notifier != nullptr)
    {
        motor_notifier->requestUpdate(motor_config);
    }
    return makeUpdate("action", action, 0);
}

void MediaPlayerApp::updateStateFromHASS(MQTTStateUpdate mqtt_state_update)
{
    cJSON *json = cJSON_Parse(mqtt_state_update.state);
    if (json == NULL)
    {
        LOGW("Media player: invalid state from Home Assistant");
        return;
    }

    cJSON *state = cJSON_GetObjectItem(json, "state");
    cJSON *volume = cJSON_GetObjectItem(json, "volume");
    cJSON *muted = cJSON_GetObjectItem(json, "muted");
    cJSON *title = cJSON_GetObjectItem(json, "title");
    cJSON *artist = cJSON_GetObjectItem(json, "artist");

    if (cJSON_IsString(state))
    {
        snprintf(player_state_, sizeof(player_state_), "%s", state->valuestring);
        is_playing_ = strcmp(player_state_, "playing") == 0;
    }
    if (cJSON_IsBool(muted))
    {
        is_muted_ = cJSON_IsTrue(muted);
    }
    if (cJSON_IsString(title))
    {
        snprintf(title_, sizeof(title_), "%s", title->valuestring);
    }
    if (cJSON_IsString(artist))
    {
        snprintf(artist_, sizeof(artist_), "%s", artist->valuestring);
    }

    bool move_knob = false;
    if (cJSON_IsNumber(volume))
    {
        int32_t new_volume = constrain(volume->valueint, 0, 100);
        bool recently_turned = millis() - last_local_volume_ms_ < VOLUME_ECHO_IGNORE_MS;
        if (!recently_turned && (!volume_known_ || new_volume != volume_))
        {
            volume_ = new_volume;
            volume_config_.position = new_volume;
            move_knob = page_ == VOLUME_PAGE;
        }
        volume_known_ = true;
    }

    cJSON_Delete(json);

    if (move_knob)
    {
        // HassApps::handleEvent sends the active app's motor config right after this.
        applyMotorConfig(volume_config_, volume_);
    }

    SemaphoreGuard lock(mutex_);
    renderVolume();
    renderPlaying();
    renderTrack();
}

void MediaPlayerApp::handleNavigation(NavigationEvent event)
{
    switch (event)
    {
    case SHORT:
    {
        // Send play/pause instead of a toggle, so a repeated message cannot flip it back.
        const char *action = is_playing_ ? "pause" : "play";
        is_playing_ = !is_playing_;
        {
            SemaphoreGuard lock(mutex_);
            renderPlaying();
        }
        // Navigation events cannot return a state, so the next knob state carries it.
        pending_action_ = action;
        break;
    }
    case LONG:
        // navigationBack() already decided: on the track page the knob goes back to the menu.
        // Either way the next visit (or this one) continues on the other page.
        showPage(page_ == VOLUME_PAGE ? TRACK_PAGE : VOLUME_PAGE);
        break;
    default:
        break;
    }
}

int8_t MediaPlayerApp::navigationNext()
{
    return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
}

int8_t MediaPlayerApp::navigationBack()
{
    // Called twice per long press by Apps::handleNavigationEvent, so no side effects here.
    if (page_ == VOLUME_PAGE)
    {
        // Long press on the volume page opens the track page instead of leaving the app.
        return DONT_NAVIGATE_UPDATE_MOTOR_CONFIG;
    }
    return App::navigationBack();
}

LedRingHint MediaPlayerApp::getLedRingHint()
{
    LedRingHint hint;
    hint.color = is_muted_ ? 0x606060 : MEDIA_PLAYER_COLOR;
    if (page_ == VOLUME_PAGE)
    {
        hint.mode = LED_RING_BAR;
        hint.fill = constrain(volume_ / 100.0f, 0.0f, 1.0f);
    }
    return hint;
}
