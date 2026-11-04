#if SK_LEDS

#include <FastLED.h>

#include "led_ring_task.h"
#include "../semaphore_guard.h"
#include "../util.h"

static CRGB leds[NUM_LEDS];    // what is currently shown
static CRGB targets[NUM_LEDS]; // what the current scene wants to show

// The display draws its value arcs from 8 o'clock clockwise over 12 o'clock to
// 4 o'clock (240 degrees, gap at the bottom). The ring bar uses the same arc.
static const float ARC_START_CLOCK_DEG = 240.0f;
static const float ARC_SPAN_DEG = 240.0f;

static const uint32_t FRAME_MS = 15;

LedRingTask::LedRingTask(const uint8_t task_core) : Task{"Led_Ring", 1024 * 3, 0, task_core}
{
    mutex_ = xSemaphoreCreateMutex();
    assert(mutex_ != NULL);
}

LedRingTask::~LedRingTask()
{
    vSemaphoreDelete(mutex_);
}

void LedRingTask::setScene(const LedRingScene &scene)
{
    SemaphoreGuard lock(mutex_);
    scene_ = scene;
}

static float ledClockAngle(uint8_t i)
{
    float angle = SK_LED_RING_OFFSET_DEG + SK_LED_RING_DIRECTION * (360.0f * i / NUM_LEDS);
    angle = fmodf(angle, 360.0f);
    if (angle < 0)
    {
        angle += 360.0f;
    }
    return angle;
}

// Color at the given brightness. The color is first normalized so its strongest
// channel is 255, so that dark lamp colors still light the ring at "level".
static CRGB colorAtLevel(uint32_t color, uint8_t level)
{
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint8_t max_channel = max(r, max(g, b));
    if (max_channel == 0 || level == 0)
    {
        return CRGB::Black;
    }
    return CRGB(
        (uint16_t)r * level / max_channel,
        (uint16_t)g * level / max_channel,
        (uint16_t)b * level / max_channel);
}

void LedRingTask::computeTargets(const LedRingScene &scene)
{
    const float led_span_deg = 360.0f / NUM_LEDS;

    // LED closest to 6 o'clock, used for the beacon.
    uint8_t beacon_led = 0;
    float best = 360.0f;

    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        float angle = ledClockAngle(i);
        float to_bottom = fabsf(angle - 180.0f);
        if (to_bottom < best)
        {
            best = to_bottom;
            beacon_led = i;
        }

        switch (scene.mode)
        {
        case LED_RING_AMBIENT:
            targets[i] = colorAtLevel(scene.color, scene.ambient);
            break;
        case LED_RING_FULL:
            targets[i] = colorAtLevel(scene.color, scene.level);
            break;
        case LED_RING_BAR:
        {
            float rel = fmodf(angle - ARC_START_CLOCK_DEG + 360.0f, 360.0f);
            float coverage = 0.0f;
            if (rel <= ARC_SPAN_DEG + led_span_deg / 2)
            {
                // How much of this LED lies below the fill edge, for a smooth edge.
                float fill_deg = scene.fill * ARC_SPAN_DEG;
                coverage = (fill_deg - (rel - led_span_deg / 2)) / led_span_deg;
                coverage = constrain(coverage, 0.0f, 1.0f);
                if (scene.fill <= 0.0f)
                {
                    coverage = 0.0f;
                }
            }
            uint8_t level = scene.ambient + (uint8_t)roundf((scene.level - scene.ambient) * coverage);
            if (scene.level < scene.ambient)
            {
                level = scene.ambient;
            }
            targets[i] = colorAtLevel(scene.color, level);
            break;
        }
        case LED_RING_BEACON:
        case LED_RING_OFF:
        default:
            targets[i] = CRGB::Black;
            break;
        }
    }

    if (scene.mode == LED_RING_BEACON)
    {
        targets[beacon_led] = colorAtLevel(scene.color, scene.level);
    }
}

static uint8_t approach(uint8_t current, uint8_t target)
{
    if (current == target)
    {
        return current;
    }
    int16_t diff = (int16_t)target - current;
    int16_t step = diff / 5;
    if (step == 0)
    {
        step = diff > 0 ? 1 : -1;
    }
    return current + step;
}

bool LedRingTask::stepTowardsTargets()
{
    bool changed = false;
    for (uint8_t i = 0; i < NUM_LEDS; i++)
    {
        if (leds[i] != targets[i])
        {
            leds[i].r = approach(leds[i].r, targets[i].r);
            leds[i].g = approach(leds[i].g, targets[i].g);
            leds[i].b = approach(leds[i].b, targets[i].b);
            changed = true;
        }
    }
    return changed;
}

void LedRingTask::playBootAnimation()
{
    // Warm comet once around the ring.
    for (int i = 0; i < NUM_LEDS; i++)
    {
        for (int j = 0; j < NUM_LEDS; j++)
        {
            int index = (i - j + NUM_LEDS) % NUM_LEDS;
            int value = j < 20 ? 255 - j * 12 : 0;
            leds[index] = CHSV(39, 255, value);
        }
        FastLED.show();
        delay(5);
    }
    FastLED.clear();
    FastLED.show();
}

void LedRingTask::run()
{
    FastLED.addLeds<WS2812B, PIN_LED_DATA, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(255);
    playBootAnimation();

    LedRingScene scene;
    while (1)
    {
        {
            SemaphoreGuard lock(mutex_);
            scene = scene_;
        }

        computeTargets(scene);
        if (stepTowardsTargets())
        {
            FastLED.show();
        }

        delay(FRAME_MS);
    }
}

#endif
