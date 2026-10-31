# 2. Abläufe: was passiert wann?

[← 1. Kern-Framework](01-kern-framework.md) · [Übersicht](../ARCHITEKTUR.md) · [3. Apps und UI →](03-apps-und-ui.md)

Diese Seite zeigt die wichtigsten Aufrufketten als Sequenzdiagramme. Funktionsnamen entsprechen dem Code, damit man direkt hineinspringen kann.

## 2.1 Systemstart

`setup()` in `main.cpp` startet die Tasks in dieser Reihenfolge und löscht sich am Ende selbst (`vTaskDelete(NULL)`). `loop()` ist leer.

```mermaid
sequenceDiagram
    autonumber
    participant Main as setup() (main.cpp)
    participant Disp as DisplayTask
    participant Root as RootTask
    participant Cfg as Configuration
    participant Mot as MotorTask
    participant Wifi as WifiTask
    participant Mqtt as MqttTask
    participant Sens as SensorsTask

    Main->>Main: Logging, Temperatursensor, EEPROM
    Main->>Disp: begin()
    Note over Disp: lv_init, Treiber,<br/>App-Sätze anlegen,<br/>wartet auf OS-Modus
    Main->>Root: addListener(display queue)
    Main->>Main: LedRingTask.begin()
    Main->>Root: begin()
    Note over Root: run() startet,<br/>wartet auf Konfiguration
    Main->>Cfg: loadPersistantConfig / loadSettings / loadSpotifyConfig
    opt Spotify-Token vorhanden
        Main->>Main: spotify_task.begin()
    end
    Main->>Root: loadConfiguration()
    Root->>Wifi: requestSTA(gespeichertes WLAN)<br/>(nur im Modus HASS/SPOTIFY)
    Main->>Mot: begin()
    Note over Mot: FOC init,<br/>Kalibrierung laden
    Main->>Wifi: addStateListener, begin()
    Main->>Mqtt: addAppSyncListener, begin()
    Main->>Sens: addStateListener, begin()
    Main->>Main: ResetTask.begin(), vTaskDelete(NULL)

    Root->>Root: Event-Bus verteilen,<br/>Notifier setzen
    Root->>Cfg: loadOSConfiguration()
    Root->>Disp: enableOnboarding() / enableHass() / enableSpotify()
    Disp->>Mot: (über MotorNotifier) Haptik-Profil der Start-App
```

Hinweis: Steht im EEPROM der Modus `DEMO`, startet die Firmware trotzdem im Onboarding (Demo wird nicht dauerhaft gespeichert).

## 2.2 Drehen am Knopf → Anzeige und Home Assistant

Das ist der häufigste Ablauf, etwa 200-mal pro Sekunde meldet der Motor seinen Zustand.

```mermaid
sequenceDiagram
    autonumber
    participant Mot as MotorTask
    participant Root as RootTask
    participant Set as App-Satz (z. B. HassApps)
    participant App as Aktive App
    participant LVGL as LVGL / DisplayTask
    participant Mqtt as MqttTask
    participant HA as Home Assistant

    loop ~1 kHz
        Mot->>Mot: loopFOC, Rastung berechnen,<br/>Drehmoment setzen
    end
    Mot-)Root: publish(PB_SmartKnobState) alle 5 ms<br/>(Queue, nur neuester Wert)
    Root->>Root: Bewegung erkannt → Bildschirm wach halten
    Root->>Set: update(app_state)
    Set->>Set: config.id == active_app->app_id ?
    Set->>App: updateStateFromKnob(motor_state)
    App->>LVGL: Labels/Bögen ändern (mit Mutex)
    App-->>Set: EntityStateUpdate
    Set->>App: updateStateFromSystem(app_state)
    Set-->>Root: EntityStateUpdate
    Root-)Mqtt: enqueueEntityStateToSend()
    opt play_haptic
        Root-)Mot: playHaptic()
    end
    Note over LVGL: lv_task_handler() zeichnet<br/>beim nächsten Durchlauf
    Mqtt->>Mqtt: Nur wenn changed == true,<br/>gebündelt pro app_id
    Mqtt-)HA: smartknob/<MAC>/from_knob<br/>{type: state_update, app_id, state}
    HA-)Mqtt: acknowledgement
```

Wichtig für App-Entwickler: Die App bekommt nur dann Updates, wenn die `id` in der gerade aktiven Motor-Config gleich ihrer `app_id` ist. Direkt nach einem App-Wechsel kann es ein paar Millisekunden dauern, bis der Motor die neue Config übernommen hat. In dieser Zeit bekommt die neue App keine Updates. Das ist gewollt und verhindert, dass sie Positionswerte der vorherigen App sieht.

## 2.3 Drücken → Klick und Navigation

```mermaid
sequenceDiagram
    autonumber
    participant Sens as SensorsTask
    participant Root as RootTask
    participant Mot as MotorTask
    participant Set as App-Satz (Apps)
    participant App as Aktive App

    Sens-)Root: SensorsState (virtual_button_code = SHORT_PRESSED)
    Root->>Mot: playHaptic(press=true)
    Note over Sens: Nutzer lässt los
    Sens-)Root: SensorsState (SHORT_RELEASED)
    Root->>Mot: playHaptic(press=false)
    Root->>Root: Fehler aktiv?<br/>ja → ErrorHandlingFlow
    Root->>Set: handleNavigationEvent(SHORT)
    Set->>App: navigationNext()
    alt liefert App-ID (z. B. aus dem Menü)
        Set->>App: handleNavigation(SHORT)
        Set->>Set: setActive(neue ID) → render()
    else DONT_NAVIGATE_UPDATE_MOTOR_CONFIG (Standard)
        Set->>App: handleNavigation(SHORT)
    else DONT_NAVIGATE
        Set-->>Root: nichts tun
    end
    Set->>Root: motor_notifier.requestUpdate(active_app->getMotorConfig())
    Root->>Mot: setConfig() (in der nächsten Runde)
```

Ein **langer Druck** läuft genauso, nur mit `NavigationEvent::LONG` und `navigationBack()`. Standardmäßig liefert `navigationBack()` den Wert `MENU`, ein langer Druck führt also aus jeder App zurück ins Menü. Im Menü selbst bewirkt ein langer Druck je nach Modus etwas anderes (Demo: zurück ins Onboarding).

## 2.4 Home Assistant: Apps synchronisieren

Welche Apps im HASS-Modus erscheinen, bestimmt allein Home Assistant. Die Firmware hat keine feste App-Liste.

```mermaid
sequenceDiagram
    autonumber
    participant HA as Home Assistant
    participant Mqtt as MqttTask
    participant Root as RootTask
    participant Hass as HassApps
    participant Mot as MotorTask

    Mqtt-)HA: smartknob/init {mac, model, firmware}
    HA-)Mqtt: acknowledgement
    HA-)Mqtt: from_hass {type: "sync", apps: [...]}
    Mqtt-)HA: acknowledgement (sync)
    Mqtt-)Root: app_sync_queue
    Root->>Hass: sync(mqtt_task->getApps())
    loop für jeden Eintrag
        Hass->>Hass: loadApp(position, app_slug, app_id,<br/>friendly_name, entity_id)
        Note right of Hass: Slug → Klasse,<br/>z. B. "switch" → SwitchApp
    end
    Hass->>Hass: SettingsApp anhängen
    Hass->>Hass: updateMenu() → setActive(MENU)
    Root->>Root: 0,5 s warten
    Root->>Hass: triggerMotorConfigUpdate()
    Hass->>Mot: Haptik-Profil des Menüs
```

Ein Eintrag im `apps`-Array sieht so aus (siehe auch `demo_apps.cpp`):

```json
{
  "app_slug": "light_dimmer",
  "app_id": "light.workbench",
  "friendly_name": "Workbench",
  "entity_id": "light.workbench",
  "area": "Kitchen",
  "menu_color": "#ffffff"
}
```

`app_slug`, `app_id`, `entity_id` und `friendly_name` sind Pflicht, sonst wird der Eintrag übersprungen.

## 2.5 Home Assistant: Zustand ändert sich in HA

```mermaid
sequenceDiagram
    autonumber
    participant HA as Home Assistant
    participant Mqtt as MqttTask
    participant Bus as shared_events_queue
    participant Root as RootTask
    participant Hass as HassApps
    participant App as App mit dieser app_id
    participant Mot as MotorTask

    HA-)Mqtt: from_hass {type: "state_update",<br/>app_id, entity_id, new_state}
    Mqtt-)Bus: WiFiEvent SK_MQTT_STATE_UPDATE
    Bus-)Root: (nächste Runde)
    Root->>Hass: handleEvent(event)
    Hass->>App: updateStateFromHASS(mqtt_state_update)
    App->>App: JSON parsen, Anzeige und<br/>motor_config.position anpassen
    Hass->>Mot: requestUpdate(active_app->getMotorConfig())
```

Damit springt bei einem Lichtschalter, der in HA umgeschaltet wurde, auch die physische Rastung auf die neue Position.

## 2.6 Spotify: Play/Pause und Lautstärke

Die Spotify-App kommuniziert nicht über MQTT, sondern direkt mit der SpotifyTask über den Event-Bus.

```mermaid
sequenceDiagram
    autonumber
    participant App as SpotifyApp
    participant Bus as shared_events_queue
    participant Root as RootTask
    participant Sp as SpotifyTask
    participant API as Spotify-Web-API

    Note over App: Kurzer Druck
    App-)Bus: SK_SPOTIFY_PLAY / SK_SPOTIFY_PAUSE
    Note over App: Drehen
    App-)Bus: SK_SPOTIFY_VOLUME (0–100)
    Bus-)Root: nächste Runde
    Root->>Sp: handleEvent()
    Sp->>API: HTTP-Aufruf
    loop regelmäßig
        Sp->>API: Wiedergabestatus, Cover
        Sp-)Bus: SK_SPOTIFY_PLAYBACK_STATE,<br/>SK_SPOTIFY_NEW_COVER_ART(_COLORS)
        Bus-)Root: nächste Runde
        Root->>Root: in app_state übernehmen
        Root->>App: updateStateFromSystem(app_state)
    end
```

## 2.7 Betriebsmodus wechseln

```mermaid
sequenceDiagram
    autonumber
    participant UI as Onboarding / Settings / Demo
    participant OSN as OSConfigNotifier
    participant Root as RootTask
    participant Cfg as Configuration
    participant Disp as DisplayTask

    UI->>OSN: setOSMode(HASS)
    Root->>OSN: loopTick()
    OSN->>Root: Callback(os_mode)
    Root->>Cfg: saveOSConfigurationInMemory()
    alt HASS / ONBOARDING
        Root->>Cfg: saveOSConfiguration() (EEPROM)
        Root->>Disp: enableHass() / enableOnboarding()
    else DEMO
        Root->>Disp: enableDemo() (nicht gespeichert)
    else SPOTIFY
        Root->>Cfg: saveOSConfiguration()
        Root->>Root: esp_restart()
    end
    Disp->>Disp: render() des App-Satzes
    Disp->>Root: triggerMotorConfigUpdate()
```

## 2.8 Bildschirm wecken und dimmen

Die RootTask führt `screen_state.awake_until` und `has_been_engaged`:

- **Wecken**: Drehbewegung, Druck, Näherung < 200 mm, Fehler oder Kalibrierung setzen `awake_until` in die Zukunft (bei physischer Bedienung meist `settings.screen.timeout`, Standard 30 s; bei bloßer Annäherung `KNOB_ENGAGED_TIMEOUT_NONE_PHYSICAL` = 8 s, gesetzt in `platformio.ini`).
- **Wach**: volle Helligkeit (`max_bright`), Dehnungssensor an, LED-Ring auf voller Helligkeit.
- **Nach Ablauf**: Mit Lichtsensor (`SK_ALS`) wird die Helligkeit an die Umgebung angepasst (zwischen 0 und `min_bright`). Der LED-Ring geht auf gedimmt oder in den „Leuchtturm“-Modus mit einer LED. Der Dehnungssensor wird abgeschaltet.
