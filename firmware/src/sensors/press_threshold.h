#pragma once

#include <stdint.h>

// Strain weight (in calibrated grams) at which a push on the knob counts as a press.
// The default comes from the build flag PRESS_WEIGHT, a value saved from the settings
// app overrides it and survives reboots.
#ifndef PRESS_WEIGHT
#define PRESS_WEIGHT 250
#endif

const uint16_t PRESS_WEIGHT_DEFAULT = PRESS_WEIGHT;
const uint16_t PRESS_WEIGHT_MIN = 100;
const uint16_t PRESS_WEIGHT_MAX = 800;
const uint16_t PRESS_WEIGHT_STEP = 10;

// Reads the saved threshold from flash, call once at boot.
void loadPressWeight();

// Current threshold, safe to call from any task.
uint16_t getPressWeight();

// Sets the threshold (clamped to the allowed range) and saves it to flash.
void setPressWeight(uint16_t grams);
