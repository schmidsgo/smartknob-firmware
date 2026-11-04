#pragma once

#include <stdint.h>

// What an app wants the LED ring to show. Apps only describe the content; the
// root task adds brightness levels from the settings and hands a LedRingScene to
// the LED ring task.
enum LedRingMode : uint8_t
{
    LED_RING_OFF = 0,
    LED_RING_AMBIENT, // faint background light on every LED
    LED_RING_BAR,     // fills along the same 240 degree arc the display draws
    LED_RING_FULL,    // every LED at medium brightness, e.g. while picking a color
    LED_RING_BEACON,  // a single LED, used when the room and the screen are dark
};

// Color value meaning "use the ring color from the settings" (warm white by default).
#define LED_RING_DEFAULT_COLOR 0xFFFFFFFFu

struct LedRingHint
{
    LedRingMode mode = LED_RING_AMBIENT;
    float fill = 0.0f; // 0..1, only used in LED_RING_BAR
    uint32_t color = LED_RING_DEFAULT_COLOR;
};

struct LedRingScene
{
    LedRingMode mode = LED_RING_AMBIENT;
    float fill = 0.0f;
    uint32_t color = 0;     // 0xRRGGBB
    uint8_t level = 0;      // brightness of lit LEDs (bar, full, beacon), 0..255
    uint8_t ambient = 0;    // brightness of the background light, 0..255
};
