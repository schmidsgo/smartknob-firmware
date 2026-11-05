#include "press_threshold.h"

#include <atomic>
#include <Preferences.h>
#include <logging.h>

static const char *PREFS_NAMESPACE = "press";
static const char *PREFS_KEY_WEIGHT = "weight";

static std::atomic<uint16_t> press_weight(PRESS_WEIGHT_DEFAULT);

static uint16_t clampPressWeight(uint16_t grams)
{
    if (grams < PRESS_WEIGHT_MIN)
        return PRESS_WEIGHT_MIN;
    if (grams > PRESS_WEIGHT_MAX)
        return PRESS_WEIGHT_MAX;
    return grams;
}

void loadPressWeight()
{
    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, true))
    {
        // Namespace does not exist yet, nothing saved so far.
        press_weight = clampPressWeight(PRESS_WEIGHT_DEFAULT);
        return;
    }
    press_weight = clampPressWeight(prefs.getUShort(PREFS_KEY_WEIGHT, PRESS_WEIGHT_DEFAULT));
    prefs.end();
    LOGI("Press threshold: %ug", (unsigned)press_weight.load());
}

uint16_t getPressWeight()
{
    return press_weight.load();
}

void setPressWeight(uint16_t grams)
{
    grams = clampPressWeight(grams);
    press_weight = grams;

    Preferences prefs;
    if (!prefs.begin(PREFS_NAMESPACE, false))
    {
        LOGE("Could not open preferences to save press threshold");
        return;
    }
    prefs.putUShort(PREFS_KEY_WEIGHT, grams);
    prefs.end();
    LOGI("Press threshold saved: %ug", (unsigned)grams);
}
