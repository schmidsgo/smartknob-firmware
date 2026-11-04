# 4. Anleitung: App oder UI ändern bzw. hinzufügen

[← 3. Apps und UI](03-apps-und-ui.md) · [Übersicht](../ARCHITEKTUR.md)

Diese Seite zeigt konkret, wie man Apps und Oberfläche ändert, ohne den Kern (Motor, Sensoren, RootTask) anzufassen.

## 4.1 Was man anfasst und was nicht

| Vorhaben | Dateien | Kern betroffen? |
|---|---|---|
| Aussehen einer App ändern | `apps/<app>/<app>.cpp` (`initScreen`, `updateStateFrom*`) | nein |
| Haptik einer App ändern | `motor_config` im Konstruktor der App | nein |
| Neue App | neuer Ordner `apps/<app>/`, plus je eine Zeile in `apps/app.h`, `apps/apps.h`, `apps/apps.cpp` | nein |
| Neue App in der Demo zeigen | JSON in `apps/demo_apps.cpp` | nein |
| Neue App in Home Assistant nutzen | wie „Neue App“, plus Slug in der HA-Integration (separates Projekt) | nein |
| Neue Settings-Seite | `apps/settings/pages/`, `apps/settings/settings.h` | nein |
| App braucht einen neuen Netzwerkdienst | neue Task, neue Events in `events/events.h`, Routing in `root_task.cpp` | **ja** |
| Neuer Betriebsmodus | `configuration.h`, `display_task.*`, `root_task.cpp` | **ja** |

## 4.2 Bestehende UI ändern

Jede App baut ihre Oberfläche in `initScreen()` auf dem eigenen `screen` auf und aktualisiert sie in `updateStateFromKnob()` / `updateStateFromHASS()`. Beispiel: Die Farbe, die `SwitchApp` im eingeschalteten Zustand zeigt, steht in `apps/switch/switch.cpp`:

```cpp
// eingeschaltet, kein Lichtschalter → grün
lv_obj_set_style_bg_color(screen, LV_COLOR_MAKE(0x00, 0x80, 0x00), 0);
```

Diese Farbe kommt zweimal vor (Änderung per Knopf und per HA). Beim Ändern beide Stellen anpassen oder in eine gemeinsame Hilfsmethode ziehen.

Regeln:

- **Jeder LVGL-Aufruf außerhalb des Konstruktors braucht den Mutex**: `SemaphoreGuard lock(mutex_);` in einem eigenen `{ … }`-Block, damit er nicht länger als nötig gehalten wird.
- `updateStateFromKnob()` läuft bis zu 200-mal pro Sekunde. Nur neu zeichnen, wenn sich etwas geändert hat (z. B. `if (current_position != last_position)`), sonst wird die Anzeige träge.
- Schriften: `LV_FONT_DECLARE(roboto_light_mono_24pt);` und dann `lv_obj_set_style_text_font(label, &roboto_light_mono_24pt, 0);`.
- Icons: `LV_IMG_DECLARE(x80_lightbulb_filled);` und `lv_img_set_src(img, &x80_lightbulb_filled);`.

## 4.3 Haptik einer App anpassen

Die Haptik ist komplett in der `motor_config` der App beschrieben. Die Felder stehen in [1.2](01-kern-framework.md#das-haptik-profil-pb_smartknobconfig). Die bestehenden Apps initialisieren die Struktur **positionsbasiert**. Kommentare je Feld helfen, sich nicht zu verzählen:

```cpp
motor_config = PB_SmartKnobConfig{
    0,              // position
    0,              // sub_position_unit
    0,              // position_nonce
    0,              // min_position
    20,             // max_position   (21 Positionen)
    8 * PI / 180,   // position_width_radians (8° pro Rastung)
    1.5,            // detent_strength_unit  (kräftiger)
    1,              // endstop_strength_unit
    1.1,            // snap_point
    "",             // id → unten auf app_id setzen
    0,              // detent_positions_count
    {},             // detent_positions
    0,              // snap_point_bias
    27,             // led_hue
};
strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);
```

Rezepte:

| Gefühl | Einstellung |
|---|---|
| Ein/Aus-Schalter mit „Kippen“ | `min 0`, `max 1`, Breite 60°, `snap_point 0.55` |
| Feine Rasterung (z. B. Lautstärke) | Breite 2–5°, `detent_strength_unit 0.5–1` |
| Frei drehend ohne Rastung | `detent_strength_unit 0` |
| Endlos drehbar (Menü, Encoder) | `max_position < min_position`, z. B. `0` / `-1` |
| Nur an bestimmten Stellen rasten | `detent_positions_count = 3`, `detent_positions = {0, 10, 20}` |
| Weicher Anschlag | `endstop_strength_unit 0.3` |

Haptik zur Laufzeit wechseln (z. B. bei einem Seitenwechsel innerhalb der App):

```cpp
motor_config = andere_config;
motor_config.position_nonce++;          // erzwingt, dass die Position übernommen wird
motor_notifier->requestUpdate(motor_config);
```

Gibt die App bei Navigation `DONT_NAVIGATE_UPDATE_MOTOR_CONFIG` zurück (Standard), schickt `Apps` nach `handleNavigation()` ohnehin die aktuelle `motor_config` an den Motor. Dann reicht es, `motor_config` in `handleNavigation()` zu setzen.

## 4.4 Neue App: Schritt für Schritt

Beispiel: eine **Zähler-App**, die Werte von 0 bis 10 einstellt und den Wert an Home Assistant meldet.

### Schritt 1: Header `apps/counter/counter.h`

```cpp
#pragma once
#include "../app.h"

class CounterApp : public App
{
public:
    CounterApp(SemaphoreHandle_t mutex, char *app_id, char *friendly_name, char *entity_id);

    EntityStateUpdate updateStateFromKnob(PB_SmartKnobState state) override;
    void updateStateFromHASS(MQTTStateUpdate mqtt_state_update) override;

protected:
    void initScreen() override;

private:
    lv_obj_t *value_label_ = nullptr;
    int32_t current_position_ = 0;
    int32_t last_position_ = -1;
    char buf_[16];
};
```

### Schritt 2: Implementierung `apps/counter/counter.cpp`

```cpp
#include "counter.h"

CounterApp::CounterApp(SemaphoreHandle_t mutex, char *app_id_, char *friendly_name_, char *entity_id_)
    : App(mutex)
{
    sprintf(app_id, "%s", app_id_);
    sprintf(friendly_name, "%s", friendly_name_);
    sprintf(entity_id, "%s", entity_id_);

    motor_config = PB_SmartKnobConfig{
        0,             // position
        0,             // sub_position_unit
        0,             // position_nonce
        0,             // min_position
        10,            // max_position
        15 * PI / 180, // position_width_radians
        1,             // detent_strength_unit
        1,             // endstop_strength_unit
        1.1,           // snap_point
        "",            // id
        0,             // detent_positions_count
        {},            // detent_positions
        0,             // snap_point_bias
        200,           // led_hue
    };
    // Pflicht: ohne passende id bekommt die App keine Knopf-Updates
    strncpy(motor_config.id, app_id, sizeof(motor_config.id) - 1);

    LV_IMG_DECLARE(x80_timer);
    LV_IMG_DECLARE(x40_timer);
    big_icon = x80_timer;
    small_icon = x40_timer;

    initScreen();
}

void CounterApp::initScreen()
{
    SemaphoreGuard lock(mutex_);

    value_label_ = lv_label_create(screen);
    lv_label_set_text(value_label_, "0");
    lv_obj_center(value_label_);

    lv_obj_t *name = lv_label_create(screen);
    lv_label_set_text(name, friendly_name);
    lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, -48);
}

EntityStateUpdate CounterApp::updateStateFromKnob(PB_SmartKnobState state)
{
    EntityStateUpdate new_state;

    // Nach einem Update von HA das nächste Knopf-Update nicht zurückschicken
    if (state_sent_from_hass)
    {
        state_sent_from_hass = false;
        return new_state;
    }

    current_position_ = state.current_position;

    // Position merken, damit der Knopf beim Zurückkehren aus dem Menü dort steht
    motor_config.position = current_position_;
    motor_config.position_nonce = current_position_;

    if (current_position_ == last_position_)
    {
        return new_state; // nichts geändert, nichts zeichnen
    }

    {
        SemaphoreGuard lock(mutex_);
        snprintf(buf_, sizeof(buf_), "%d", (int)current_position_);
        lv_label_set_text(value_label_, buf_);
    }

    // Zustand für Home Assistant (muss gültiges JSON sein)
    sprintf(new_state.app_id, "%s", app_id);
    sprintf(new_state.entity_id, "%s", entity_id);
    sprintf(new_state.app_slug, "%s", APP_SLUG_COUNTER);
    snprintf(new_state.state, sizeof(new_state.state), "{\"value\": %d}", (int)current_position_);
    new_state.changed = true;

    last_position_ = current_position_;
    return new_state;
}

void CounterApp::updateStateFromHASS(MQTTStateUpdate mqtt_state_update)
{
    cJSON *json = cJSON_Parse(mqtt_state_update.state);
    cJSON *value = cJSON_GetObjectItem(json, "value");
    if (value != NULL)
    {
        current_position_ = value->valueint;
        last_position_ = current_position_;
        motor_config.position = current_position_;
        motor_config.position_nonce = current_position_ + 1; // erzwingt Übernahme
        state_sent_from_hass = true;

        SemaphoreGuard lock(mutex_);
        snprintf(buf_, sizeof(buf_), "%d", (int)current_position_);
        lv_label_set_text(value_label_, buf_);
    }
    cJSON_Delete(json);
}
```

### Schritt 3: Slug registrieren (`apps/app.h`)

Bei den anderen `APP_SLUG_*`-Konstanten ergänzen:

```cpp
const char APP_SLUG_COUNTER[48] = "counter";
```

### Schritt 4: In die App-Fabrik eintragen

In `apps/apps.h` den Header einbinden:

```cpp
#include "apps/counter/counter.h"
```

In `Apps::loadApp()` in `apps/apps.cpp` einen Zweig vor dem abschließenden `else` ergänzen:

```cpp
else if (app_slug.compare(APP_SLUG_COUNTER) == 0)
{
    CounterApp *app = new CounterApp(screen_mutex_, app_id, friendly_name, entity_id);
    add(position, app);
    return app;
}
```

PlatformIO kompiliert alle Dateien unter `firmware/src/` automatisch, eine Build-Datei muss man nicht anpassen.

### Schritt 5: App sichtbar machen

**a) In der Demo** (ohne Netzwerk testen, am einfachsten): In `apps/demo_apps.cpp` einen Eintrag ins JSON-Array einfügen:

```json
{"app_slug": "counter", "app_id": "counter.demo", "friendly_name": "Zähler", "area": "Test", "menu_color": "#ffffff", "entity_id": "counter_demo"}
```

Dann im Onboarding „Demo“ wählen. Die neue App erscheint im Menü.

> Achtung: Die Standardschrift (`LV_FONT_DEFAULT`, AktivGrotesk) enthält laut `assets/fonts/lvgl_fonts.md` die Zeichen `0x20–0x7E` und `0xC0–0xFF`. Das „ä“ in „Zähler“ ist enthalten, Sonderzeichen wie „€“ oder Emojis nicht.

**b) In Home Assistant**: Die HA-Integration schickt die App-Liste per `sync` (siehe [2.4](02-ablaeufe.md#24-home-assistant-apps-synchronisieren)). Sie muss den Slug `counter` kennen, die Entität zuordnen und `{"value": n}` verstehen. Das passiert in der Integration, nicht in der Firmware.

**c) Als eigener Modus** (wie `SpotifyStandalone`): Das braucht einen neuen `OSMode` und berührt den Kern, siehe [3.1](03-apps-und-ui.md#was-ein-modus-kostet).

## 4.5 App mit mehreren Seiten

Vorbild: `apps/light_dimmer/` oder `apps/settings/`.

1. Ein `enum` mit den Seiten anlegen.
2. Jede Seite als Klasse von `BasePage` ableiten (`display/page_manager.h`), UI im Konstruktor auf `page` aufbauen, optional `update()`, `updateFromSystem()`, `handleNavigation()` überschreiben.
3. Einen `PageManager<DeinEnum>` im App-Konstruktor mit `screen` als Parent anlegen und die Seiten mit `add()` registrieren.
4. In `handleNavigation()` der App mit `page_mgr->show(SEITE)` umschalten und ggf. eine andere `motor_config` setzen.
5. `navigationBack()` so überschreiben, dass ein langer Druck auf einer Unterseite zurück zur Hauptseite führt statt ins Menü (siehe `LightDimmerApp::navigationBack`).

## 4.6 Eine neue Settings-Seite

1. Seite in `apps/settings/pages/` als `BasePage`-Unterklasse anlegen.
2. In `apps/settings/settings.h`: Wert im `enum SettingsPages` vor `SETTINGS_PAGE_COUNT` einfügen, in `SettingsPageManager` per `add()` registrieren und in `show()` den Seitennamen ergänzen.
3. Die Anzahl der Rastungen passt sich automatisch an, weil `SettingsApp` `max_position = SETTINGS_PAGE_COUNT - 1` setzt.

## 4.7 Wenn eine App einen neuen Dienst braucht

Manche Apps brauchen mehr als HA-Zustände, z. B. eine eigene Web-API wie Spotify. Dann führt kein Weg am Kern vorbei, aber das Muster ist immer gleich:

1. Neue Task unter `network/` (Vorbild: `SpotifyTask`), in `main.cpp` anlegen und starten.
2. Neue `EventType`-Werte und ggf. eine Struktur in `WiFiEventBody` (`events/events.h`).
3. Die Task bekommt den Event-Bus per `setSharedEventsQueue()` (in `RootTask::run`).
4. In `RootTask::run` die neuen Events routen: Befehle der App an die Task, Ergebnisse der Task in `AppState`.
5. Die App liest die Ergebnisse in `updateStateFromSystem(app_state)` und schickt Befehle über `app_state.shared_events_queue` (siehe `SpotifyApp::publishEvent`).

Mit dieser Trennung bleibt die App selbst frei von Netzwerkcode.

## 4.8 Fallstricke

- **`motor_config.id` ≠ `app_id`**: Die App bekommt keine Knopf-Updates, der Bildschirm reagiert nicht aufs Drehen.
- **LVGL ohne Mutex**: sporadische Abstürze oder Grafikfehler.
- **`position_nonce` ≥ 256**: Das Feld ist 8 Bit breit und läuft über. Bei Positionen > 255 einen eigenen Zähler verwenden.
- **`snap_point < 0.5`** oder negative Stärken: Die MotorTask verwirft die Config stillschweigend (nur Debug-Log).
- **`EntityStateUpdate.state` kein gültiges JSON**: HA verwirft die Nachricht.
- **Bei jedem Knopf-Update `changed = true` setzen**: MQTT wird mit Nachrichten überschwemmt. Nur bei echter Änderung setzen.
- **Speicher**: Jede App hält einen eigenen LVGL-Screen. Große Bilder (z. B. `hue_wheel.c`, 1 MB Quelltext) kosten Flash. Bei vielen Apps im Blick behalten.
- **`cJSON`-Objekte**: Immer `cJSON_Delete` bzw. `cJSON_free` aufrufen, sonst gibt es Speicherlecks.

## 4.9 Bauen und Flashen

Mit PlatformIO (in VS Code oder auf der Kommandozeile):

```bash
pio run -e seedlabs_devkit              # bauen
pio run -e seedlabs_devkit -t upload    # flashen
pio device monitor                      # Logs, Taste 'c' startet Motorkalibrierung
```

Unter Windows liegt `pio` oft nicht im `PATH`, sondern unter `%USERPROFILE%\.platformio\penv\Scripts\pio.exe`.

Umgebungen in `platformio.ini`: `seedlabs_devkit` (Standard), `seedlabs_devkit_inverted_display`, `seedlabs_devkit_github_action_release`, `seedlabs_legacy`.
