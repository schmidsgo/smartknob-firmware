
#include "spotify_api.h"

AsyncClient *cover_art_client = nullptr;
AsyncClient *cover_art_colors_client = nullptr;

AsyncHttp *cover_art_http = nullptr;
AsyncHttp *cover_art_colors_http = nullptr;

#define HOST "configurator.seedlabs.it"
#define PORT 80

SpotifyApi::SpotifyApi(Configuration &configuration)
    : configuration_(configuration)
{
    auth_header_ =
        (char *)heap_caps_calloc(1, AUTH_HEADER_SIZE, MALLOC_CAP_SPIRAM);
    assert(auth_header_ != nullptr);

    refresh_token_body_ =
        (char *)heap_caps_calloc(1, REFRESH_TOKEN_SIZE, MALLOC_CAP_SPIRAM);
    assert(refresh_token_body_ != nullptr);

    base64_basic_auth_header_ = (char *)heap_caps_calloc(
        1, BASE64_BASIC_AUTH_HEADER_SIZE, MALLOC_CAP_SPIRAM);
    assert(base64_basic_auth_header_ != nullptr);

    cover_art_client = new AsyncClient();
    cover_art_colors_client = new AsyncClient();

    cover_art_http = new AsyncHttp(cover_art_client, HOST, PORT);
    cover_art_colors_http = new AsyncHttp(cover_art_colors_client, HOST, PORT);

    // http_client_.setReuse(true); // TODO, check if this is needed
}

SpotifyApi::~SpotifyApi()
{
    if (auth_header_ != nullptr)
    {
        heap_caps_free(auth_header_);
    }

    if (refresh_token_body_ != nullptr)
    {
        heap_caps_free(refresh_token_body_);
    }

    if (base64_basic_auth_header_ != nullptr)
    {
        heap_caps_free(base64_basic_auth_header_);
    }
}

bool SpotifyApi::isPlaying()
{
    return getCurrentPlaybackState().is_playing;
    // return false;
}

void processImage(char *body, size_t length)
{
    LOGD("Processing image of size: %zu bytes", length);

    // Example: Save to a file or decode the image
    // Here you can decode the image (e.g., using a JPEG/PNG decoder)
}

// Null-safe JSON accessors: Spotify returns null for several fields
// (device.id, item, ...). cJSON_GetObjectItem(...)->valuestring on those used
// to dereference NULL and crash the knob in a boot loop.
static bool jsonCopyString(const cJSON *obj, const char *key, char *dst, size_t dst_size)
{
    dst[0] = '\0';
    const char *value = cJSON_GetStringValue(cJSON_GetObjectItem(obj, key));
    if (value == NULL)
    {
        return false;
    }
    strlcpy(dst, value, dst_size);
    return true;
}

static double jsonNumber(const cJSON *obj, const char *key)
{
    const cJSON *value = cJSON_GetObjectItem(obj, key);
    return cJSON_IsNumber(value) ? value->valuedouble : 0;
}

PlaybackState SpotifyApi::getCurrentPlaybackState()
{
    PlaybackState playback_state{.available = false,
                                 .spotify_available = false,
                                 .timestamp = 0,
                                 .progress_ms = 0};
    if (!checkAndRefreshToken())
    {
        LOGE("Failed to refresh token");
        playback_state.available = false;
        playback_state.spotify_available = false;
        return playback_state;
    }

    String url = "https://api.spotify.com/v1/me/player";
    http_client_.begin(url);
    http_client_.addHeader("Authorization", auth_header_);

    int httpCode = http_client_.GET();

    if (httpCode == HTTP_CODE_OK)
    {
        int content_length = http_client_.getSize();
        String payload;
        if (content_length > 0)
        {
            payload.reserve(content_length + 1);
        }
        payload = http_client_.getString();
        http_client_.end();

        LOGV(LOG_LEVEL_DEBUG, "Spotify playback state: %d bytes (content-length %d)", payload.length(), content_length);

        cJSON *json = cJSON_Parse(payload.c_str());
        if (json == NULL)
        {
            LOGE("Spotify: error parsing playback state JSON (%d bytes)", payload.length());
            playback_state.available = false;
            return playback_state;
        }

        // Spotify marks many fields as nullable (e.g. device.id for some
        // Connect devices, item for podcasts/ads). Every field is read via the
        // null-safe helpers below so a missing or null value can't crash us.
        jsonCopyString(json, "repeat_state", playback_state.repeat_state, sizeof(playback_state.repeat_state));
        playback_state.shuffle_state = cJSON_IsTrue(cJSON_GetObjectItem(json, "shuffle_state"));
        playback_state.timestamp = (uint64_t)jsonNumber(json, "timestamp");
        playback_state.progress_ms = (int)jsonNumber(json, "progress_ms");
        playback_state.is_playing = cJSON_IsTrue(cJSON_GetObjectItem(json, "is_playing"));
        jsonCopyString(json, "currently_playing_type", playback_state.currently_playing_type, sizeof(playback_state.currently_playing_type));

        cJSON *device = cJSON_GetObjectItem(json, "device");
        if (cJSON_IsObject(device))
        {
            if (!jsonCopyString(device, "id", playback_state.device.id, sizeof(playback_state.device.id)))
            {
                LOGW("Spotify: active device has no id (null)");
            }
            playback_state.device.is_active = cJSON_IsTrue(cJSON_GetObjectItem(device, "is_active"));
            playback_state.device.is_private_session = cJSON_IsTrue(cJSON_GetObjectItem(device, "is_private_session"));
            playback_state.device.is_restricted = cJSON_IsTrue(cJSON_GetObjectItem(device, "is_restricted"));
            jsonCopyString(device, "name", playback_state.device.name, sizeof(playback_state.device.name));
            jsonCopyString(device, "type", playback_state.device.type, sizeof(playback_state.device.type));
            playback_state.device.volume_percent = (int)jsonNumber(device, "volume_percent");
            playback_state.device.supports_volume = cJSON_IsTrue(cJSON_GetObjectItem(device, "supports_volume"));
            LOGV(LOG_LEVEL_DEBUG, "Spotify device: name='%s' type='%s' id='%s' vol=%d playing=%d",
                 playback_state.device.name, playback_state.device.type, playback_state.device.id,
                 playback_state.device.volume_percent, playback_state.is_playing);
        }
        else
        {
            LOGW("Spotify: no device object in playback state");
        }

        cJSON *item = cJSON_GetObjectItem(json, "item");
        if (cJSON_IsObject(item))
        {
            cJSON *first_artist = cJSON_GetArrayItem(cJSON_GetObjectItem(item, "artists"), 0);
            jsonCopyString(first_artist, "name", playback_state.item.artist, sizeof(playback_state.item.artist));

            cJSON *album = cJSON_GetObjectItem(item, "album");
            if (cJSON_IsObject(album))
            {
                cJSON *images = cJSON_GetObjectItem(album, "images");
                int image_count = cJSON_IsArray(images) ? cJSON_GetArraySize(images) : 0;
                for (int i = 0; i < image_count && i < MAX_IMAGES; i++)
                {
                    cJSON *image = cJSON_GetArrayItem(images, i);
                    jsonCopyString(image, "url", playback_state.item.album.images[i].url, sizeof(playback_state.item.album.images[i].url));
                    playback_state.item.album.images[i].width = (int)jsonNumber(image, "width");
                    playback_state.item.album.images[i].height = (int)jsonNumber(image, "height");
                }
                jsonCopyString(album, "name", playback_state.item.album.name, sizeof(playback_state.item.album.name));
            }

            playback_state.item.duration_ms = (int)jsonNumber(item, "duration_ms");
            jsonCopyString(item, "name", playback_state.item.name, sizeof(playback_state.item.name));
            LOGV(LOG_LEVEL_DEBUG, "Spotify item: '%s' by '%s' (%d ms)", playback_state.item.name,
                 playback_state.item.artist, playback_state.item.duration_ms);
        }
        else
        {
            LOGW("Spotify: no item in playback state (type '%s')", playback_state.currently_playing_type);
        }

        cJSON_Delete(json);
    }
    else if (httpCode == HTTP_CODE_NO_CONTENT)
    {
        http_client_.end();
        playback_state.available = false;
        playback_state.spotify_available = true;
        return playback_state;
    }
    else
    {
        LOGE("Error in HTTP request: %d", httpCode);
        http_client_.end();
        return playback_state;
    }
    playback_state.available = true;
    playback_state.spotify_available = true;
    return playback_state;
}

#define MIN_TLS_HEAP_SIZE 18000

bool SpotifyApi::refreshToken()
{
    http_client_.begin("https://accounts.spotify.com/api/token");
    http_client_.addHeader("Authorization", base64_basic_auth_header_);
    http_client_.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http_client_.addHeader("Content-Length", String(strlen(refresh_token_body_)));

    int httpCode = http_client_.POST(refresh_token_body_);

    // LOGE("Refresh token body: %s", refresh_token_body_);
    // LOGE("Refresh token header: %s", base64_basic_auth_header_);

    if (httpCode == HTTP_CODE_OK)
    {
        // String payload = http_client_.getString();
        int content_length = http_client_.getSize();
        char *buffer_ =
            (char *)heap_caps_calloc(1, content_length, MALLOC_CAP_SPIRAM);
        if (buffer_ == nullptr)
        {
            LOGE("Failed to allocate memory for buffer");
            http_client_.end();
            return false;
        }

        WiFiClient *stream = http_client_.getStreamPtr();
        uint16_t total_bytes_read = 0;
        while (http_client_.connected() && (total_bytes_read < content_length))
        {
            // Read data in chunks and process
            size_t available_data = stream->available();
            if (available_data)
            {
                char buffer[128]; // Example buffer size
                uint16_t bytes_read =
                    stream->readBytes(buffer, min(available_data, sizeof(buffer)));
                memcpy(buffer_ + total_bytes_read, buffer, bytes_read);
                assert(total_bytes_read + bytes_read <= content_length);
                total_bytes_read += bytes_read;
            }
        }

        cJSON *json = cJSON_Parse(buffer_);

        if (json == NULL)
        {
            LOGE("Error parsing JSON"); // TODO handle
            http_client_.end();
            return false;
        }

        PB_SpotifyConfig *spotify_config_ = (PB_SpotifyConfig *)heap_caps_calloc(
            1, sizeof(PB_SpotifyConfig), MALLOC_CAP_SPIRAM);

        strcpy(spotify_config_->access_token,
               cJSON_GetStringValue(cJSON_GetObjectItem(json, "access_token")));
        strcpy(spotify_config_->token_type,
               cJSON_GetStringValue(cJSON_GetObjectItem(json, "token_type")));
        spotify_config_->expires_in =
            cJSON_GetNumberValue(cJSON_GetObjectItem(json, "expires_in")) * 1000;
        if (cJSON_HasObjectItem(json, "refresh_token"))
            strcpy(spotify_config_->refresh_token,
                   cJSON_GetStringValue(cJSON_GetObjectItem(json, "refresh_token")));
        strcpy(spotify_config_->scope,
               cJSON_GetStringValue(cJSON_GetObjectItem(json, "scope")));
        cJSON_Delete(json);

        expires_in_ = spotify_config_->expires_in;

        configuration_.setSpotifyConfig(*spotify_config_);

        setConfig(*spotify_config_);
        http_client_.end();
        return true;
    }
    else
    {
        LOGE("Error in HTTP request: %d", httpCode);
    }
    http_client_.end();
    return false;
}

bool SpotifyApi::checkAndRefreshToken()
{
    // LOGE("Last refreshed: %lu", last_refreshed_ms_);
    // LOGE("Expires in: %d", expires_in_);
    if (last_refreshed_ms_ == 0 || millis() >= (last_refreshed_ms_ - 1000) + 1800000)
    {
        if (refreshToken())
        {
            last_refreshed_ms_ = millis();
            LOGV(LOG_LEVEL_INFO, "Spotify token refreshed");
            return true;
        }
        else
        {
            LOGE("Failed to refresh Spotify token");
            return false;
        }
    }
    return true;
}

// Spotify returns no id for some Connect devices (e.g. grouped speakers).
// An empty "device_id=" makes the API answer 404, so only add the parameter
// when we actually have an id; otherwise the request targets the active device.
static String withDeviceId(String url, const char *device_id, const char *fallback_device_id)
{
    const char *id = (device_id != nullptr && device_id[0] != '\0') ? device_id : fallback_device_id;
    if (id != nullptr && id[0] != '\0')
    {
        url += (url.indexOf('?') >= 0) ? "&device_id=" : "?device_id=";
        url += id;
    }
    return url;
}

bool SpotifyApi::play(const char *device_id)
{
    int16_t result = sendPutRequest(withDeviceId("https://api.spotify.com/v1/me/player/play", device_id, device_id_));
    if (result == HTTP_CODE_OK || result == HTTP_CODE_NO_CONTENT)
    {
        return true;
    }
    LOGE("Error in HTTP request: %d", result);
    return false;
}

bool SpotifyApi::pause(const char *device_id)
{
    int16_t result = sendPutRequest(withDeviceId("https://api.spotify.com/v1/me/player/pause", device_id, device_id_));
    if (result == HTTP_CODE_OK || result == HTTP_CODE_NO_CONTENT)
    {
        return true;
    }
    LOGE("Error in HTTP request: %d", result);
    return false;
}

bool SpotifyApi::setVolume(uint8_t volume, const char *device_id)
{
    String url = withDeviceId("https://api.spotify.com/v1/me/player/volume?volume_percent=" + String(volume), device_id, device_id_);
    LOGV(LOG_LEVEL_DEBUG, "Spotify volume URL: %s", url.c_str());
    int result = sendPutRequest(url);
    if (result == HTTP_CODE_OK || result == HTTP_CODE_NO_CONTENT)
    {
        return true;
    }
    LOGE("Error in HTTP request: %d", result);
    return false;
}

int SpotifyApi::sendPutRequest(const String &url, const String &body)
{
    if (!checkAndRefreshToken())
    {
        return -1;
    }

    http_client_.begin(url);
    http_client_.addHeader("Authorization", auth_header_);
    http_client_.addHeader("Content-Type", "application/json");
    http_client_.addHeader("Content-Length", String(body.length()));

    int httpCode = http_client_.PUT(body);

    if (httpCode == HTTP_CODE_OK)
    {
        LOGV(LOG_LEVEL_DEBUG, "HTTP_CODE_OK");
    }
    else if (httpCode == HTTP_CODE_NO_CONTENT)
    {
        LOGV(LOG_LEVEL_DEBUG, "HTTP_CODE_NO_CONTENT");
    }
    else
    {
        LOGE("Error in HTTP request code: %d", httpCode);
    }
    http_client_.end();
    return httpCode;
}

bool SpotifyApi::isAvailable() // TODO
{
    // if (spotify_config_.expires_in > 0)
    //     return checkAndRefreshToken();
    // return false;
    return true;
}

void SpotifyApi::downloadImage(char *path) // TODO
{
    if (WiFi.status() == WL_CONNECTED)
    {
        cover_art_http->get(path, "image/png", [this, path](const char *body, size_t body_size)
                            {
                imageSize = body_size;
                imageBuffer = (uint8_t *)heap_caps_aligned_alloc(16, body_size, MALLOC_CAP_SPIRAM);
                memcpy(imageBuffer, body, body_size);

                if (isValidPng(imageBuffer, body_size))
                {
                    LOGD("Valid PNG image");
                }
                else
                {
                    LOGE("Invalid PNG image");
                    heap_caps_free(imageBuffer);
                    imageBuffer = nullptr;
                    downloadImage(path);
                    return;
                }

                lv_img_dsc_t *image_dsc = (lv_img_dsc_t *)heap_caps_malloc(sizeof(lv_img_dsc_t), MALLOC_CAP_SPIRAM);
                image_dsc->header.always_zero = 0;
                image_dsc->header.w = 240;
                image_dsc->header.h = 240;
                image_dsc->header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
                image_dsc->data = imageBuffer;
                image_dsc->data_size = body_size;

                

                WiFiEvent cover_art_event;
                cover_art_event.type = SK_SPOTIFY_NEW_COVER_ART;
                cover_art_event.body.cover_art = image_dsc;
                publishEvent(cover_art_event);
                
                heap_caps_free(imageBuffer); });
    }
}

void SpotifyApi::fetchImageColors(char *path)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        cover_art_colors_http->get(path, "application/json", [this](const char *body, size_t body_size)
                                   {
            cJSON *json = cJSON_Parse(body);
            if (json == NULL)
            {
                LOGE("Error parsing JSON");
                return;
            }

            cJSON *color1 = cJSON_GetObjectItem(json, "color1"); 
            cJSON *color2 = cJSON_GetObjectItem(json, "color2");

            if (color1 == NULL || color2 == NULL)
            {
                LOGE("Error parsing JSON");
                return;
            }

            colors = (lv_color_t *)heap_caps_malloc(2 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
            colors[0] = lv_color_hex(color1->valueint);
            colors[1] = lv_color_hex(color2->valueint);

            WiFiEvent cover_art_colors_event;
            cover_art_colors_event.type = SK_SPOTIFY_NEW_COVER_ART_COLORS;
            cover_art_colors_event.body.cover_art_colors = colors;
            publishEvent(cover_art_colors_event); });
    }
}

String SpotifyApi::getCurrentTrackName()
{
    return getCurrentPlaybackState().item.name;
}

void SpotifyApi::setConfig(const PB_SpotifyConfig &spotify_config)
{
    strncpy(auth_header_, "Bearer ", AUTH_HEADER_SIZE);
    strncat(auth_header_, spotify_config.access_token, AUTH_HEADER_SIZE - 1);
    auth_header_[AUTH_HEADER_SIZE] = '\0';

    strncpy(refresh_token_body_,
            "grant_type=refresh_token&refresh_token=", REFRESH_TOKEN_SIZE);
    strncat(refresh_token_body_, spotify_config.refresh_token,
            REFRESH_TOKEN_SIZE - 1);
    refresh_token_body_[BASE64_BASIC_AUTH_HEADER_SIZE] = '\0';

    strncpy(base64_basic_auth_header_, "Basic ", BASE64_BASIC_AUTH_HEADER_SIZE);
    strncat(base64_basic_auth_header_, spotify_config.base64_id_and_secret,
            BASE64_BASIC_AUTH_HEADER_SIZE - 1);
    base64_basic_auth_header_[BASE64_BASIC_AUTH_HEADER_SIZE] = '\0';

    expires_in_ = spotify_config.expires_in;
    strcpy(device_id_, spotify_config.device_id);
}

void SpotifyApi::setSharedEventsQueue(QueueHandle_t shared_events_queue)
{
    shared_events_queue_ = shared_events_queue;
}

void SpotifyApi::publishEvent(WiFiEvent event)
{
    if (shared_events_queue_ == NULL)
    {
        LOGE("No shared events queue set");
        return; // TODO handle!!!!!!!!!
    }
    event.sent_at = millis();
    xQueueSendToBack(shared_events_queue_, &event, 0);
}
