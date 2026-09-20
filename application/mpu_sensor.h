#ifndef APPLICATION_MPU_SENSOR_H_
#define APPLICATION_MPU_SENSOR_H_

#include "main.h"
#include <stdint.h>

/**
 * @brief Data mentah accelerometer.
 */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;

} MPU_AccelRaw_t;

/**
 * @brief Offset kalibrasi accelerometer.
 */
typedef struct
{
    int32_t x_offset;
    int32_t y_offset;
    int32_t z_offset;

} MPU_AccelCalibration_t;

/**
 * @brief Mendeteksi dan menginisialisasi sensor.
 */
HAL_StatusTypeDef MPU_Sensor_Init(
    uint8_t *who_am_i
);

/**
 * @brief Membaca accelerometer mentah X/Y/Z.
 */
HAL_StatusTypeDef MPU_Sensor_ReadRaw(
    MPU_AccelRaw_t *data
);

/**
 * @brief Kalibrasi accelerometer saat sensor diam dan rata.
 */
HAL_StatusTypeDef MPU_Sensor_Calibrate(
    MPU_AccelCalibration_t *calibration
);

/**
 * @brief Membaca accelerometer yang sudah dikalibrasi dalam mg.
 */
HAL_StatusTypeDef MPU_Sensor_ReadMg(
    const MPU_AccelCalibration_t *calibration,
    int16_t *x_mg,
    int16_t *y_mg,
    int16_t *z_mg
);

#endif /* APPLICATION_MPU_SENSOR_H_ */
