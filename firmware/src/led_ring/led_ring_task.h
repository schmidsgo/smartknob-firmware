#pragma once

#include "led_ring_scene.h"

#if SK_LEDS

#include "../task.h"
#include "../app_config.h"

// Where LED 0 sits on the knob, as a clock angle in degrees (0 = 12 o'clock,
// 90 = 3 o'clock, 180 = 6 o'clock), and in which direction the LED indices run
// (1 = clockwise, -1 = counter-clockwise, seen from above).
#ifndef SK_LED_RING_OFFSET_DEG
#define SK_LED_RING_OFFSET_DEG 180
#endif
#ifndef SK_LED_RING_DIRECTION
#define SK_LED_RING_DIRECTION 1
#endif

// Renders a LedRingScene continuously and fades smoothly between scenes.
class LedRingTask : public Task<LedRingTask>
{
    friend class Task<LedRingTask>; // Allow base Task to invoke protected run()

public:
    LedRingTask(const uint8_t task_core);
    ~LedRingTask();

    void setScene(const LedRingScene &scene);

protected:
    void run();

private:
    SemaphoreHandle_t mutex_;
    LedRingScene scene_;

    void playBootAnimation();
    void computeTargets(const LedRingScene &scene);
    bool stepTowardsTargets();
};

#else

class LedRingTask
{
public:
    void setScene(const LedRingScene &scene) {}
};

#endif
