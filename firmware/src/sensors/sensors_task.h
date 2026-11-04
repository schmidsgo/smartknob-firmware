#pragma once

#include "task.h"
#include "app_config.h"
#include <vector>
#include <Adafruit_VL53L0X.h>

#if SK_STRAIN
#include <HX711.h>
#endif

#include "driver/temp_sensor.h"

const uint16_t PROXIMITY_SENSOR_OFFSET_MM = 10;

// Presence detection. The case reflects a lot of light back into the
// VL53L0X (crosstalk), so the reported distance is not reliable. Instead we
// learn the idle return signal rate and treat a clear rise above it, or a
// run of valid readings, as "someone is there".
const float PROXIMITY_SIGNAL_SMOOTHING = 0.3f;        // EMA factor for the signal rate
const float PROXIMITY_BASELINE_ALPHA = 0.01f;         // how fast the idle baseline follows drift
const float PROXIMITY_PRESENT_DELTA_MCPS = 0.35f;     // rise above baseline that counts as presence
const float PROXIMITY_ABSENT_DELTA_MCPS = 0.2f;       // rise below which presence ends
const uint8_t PROXIMITY_PRESENT_SAMPLES = 3;          // consecutive samples to enter presence (~150 ms)
const uint8_t PROXIMITY_ABSENT_SAMPLES = 10;          // consecutive samples to leave presence (~500 ms)
const uint8_t PROXIMITY_BASELINE_INIT_SAMPLES = 20;   // samples averaged at boot for the first baseline

class SensorsTask : public Task<SensorsTask>
{
    friend class Task<SensorsTask>; // Allow base Task to invoke protected run()

public:
    SensorsTask(const uint8_t task_core, Configuration *configuration);
    ~SensorsTask();

    void addStateListener(QueueHandle_t queue);
    void factoryStrainCalibrationCallback(float calibration_weight);
    void weightMeasurementCallback();

    void setSharedEventsQueue(QueueHandle_t shared_event_queue);
    void publishEvent(WiFiEvent event);

    bool powerDownAllowed();

    void strainPowerDown();
    void strainPowerUp();

    bool isStrainPowered() { return strain_powered; }

protected:
    void run();

private:
    SensorsState sensors_state = {};
    QueueHandle_t sensors_state_queue_;

    bool do_strain = false;
    bool strain_powered = false;

    QueueHandle_t shared_events_queue;

    std::vector<QueueHandle_t> state_listeners_;

    SemaphoreHandle_t mutex_;
    void publishState(const SensorsState &state);
#if SK_STRAIN
    HX711 strain;
#endif

    Configuration *configuration_;

    uint8_t factory_strain_calibration_step_ = 0;
    uint8_t weight_measurement_step_ = 0;

    float last_press_value_ = 0;
    float last_strain_reading_raw_ = 0;

    float raw_initial_value_ = 0;

    float calibration_scale_ = 0;
};
