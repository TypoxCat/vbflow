#include "mpu_sensor.h"
#include "i2c.h"

#define MPU_ADDR                         (0x68U << 1)

#define MPU_REG_SMPLRT_DIV               0x19U
#define MPU_REG_CONFIG                   0x1AU
#define MPU_REG_ACCEL_CONFIG             0x1CU
#define MPU_REG_ACCEL_XOUT_H             0x3BU
#define MPU_REG_PWR_MGMT_1               0x6BU
#define MPU_REG_PWR_MGMT_2               0x6CU
#define MPU_REG_WHO_AM_I                 0x75U

#define MPU_ACCEL_LSB_PER_G              16384L
#define MPU_CALIBRATION_SAMPLE_COUNT     1000U
#define MPU_CALIBRATION_PERIOD_MS        10U
#define MPU_I2C_TIMEOUT_MS               20U

static HAL_StatusTypeDef MPU_WriteRegister(
    uint8_t register_address,
    uint8_t value
);

static HAL_StatusTypeDef MPU_ReadRegister(
    uint8_t register_address,
    uint8_t *value
);

static int16_t MPU_CombineBytes(
    uint8_t high_byte,
    uint8_t low_byte
);

static int16_t MPU_ClampInt16(
    int32_t value
);

static HAL_StatusTypeDef MPU_WriteRegister(
    uint8_t register_address,
    uint8_t value
)
{
    return HAL_I2C_Mem_Write(
        &hi2c1,
        MPU_ADDR,
        register_address,
        I2C_MEMADD_SIZE_8BIT,
        &value,
        1U,
        MPU_I2C_TIMEOUT_MS
    );
}

static HAL_StatusTypeDef MPU_ReadRegister(
    uint8_t register_address,
    uint8_t *value
)
{
    if (value == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(
        &hi2c1,
        MPU_ADDR,
        register_address,
        I2C_MEMADD_SIZE_8BIT,
        value,
        1U,
        MPU_I2C_TIMEOUT_MS
    );
}

static int16_t MPU_CombineBytes(
    uint8_t high_byte,
    uint8_t low_byte
)
{
    uint16_t combined_value;

    combined_value =
        ((uint16_t)high_byte << 8U) |
        (uint16_t)low_byte;

    return (int16_t)combined_value;
}

static int16_t MPU_ClampInt16(
    int32_t value
)
{
    if (value > INT16_MAX)
    {
        return INT16_MAX;
    }

    if (value < INT16_MIN)
    {
        return INT16_MIN;
    }

    return (int16_t)value;
}

HAL_StatusTypeDef MPU_Sensor_Init(
    uint8_t *who_am_i
)
{
    HAL_StatusTypeDef status;

    if (who_am_i == NULL)
    {
        return HAL_ERROR;
    }

    status = HAL_I2C_IsDeviceReady(
        &hi2c1,
        MPU_ADDR,
        3U,
        100U
    );

    if (status != HAL_OK)
    {
        return status;
    }

    status = MPU_ReadRegister(
        MPU_REG_WHO_AM_I,
        who_am_i
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /*
     * 0x68: identitas MPU6050 standar.
     * 0x72: identitas yang terbaca pada modul pengguna.
     */
    if ((*who_am_i != 0x68U) &&
        (*who_am_i != 0x72U))
    {
        return HAL_ERROR;
    }

    status = MPU_WriteRegister(
        MPU_REG_PWR_MGMT_1,
        0x00U
    );

    if (status != HAL_OK)
    {
        return status;
    }

    HAL_Delay(100U);

    status = MPU_WriteRegister(
        MPU_REG_CONFIG,
        0x03U
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /*
     * Sample rate: 1000 / (1 + 9) = 100 Hz.
     */
    status = MPU_WriteRegister(
        MPU_REG_SMPLRT_DIV,
        0x09U
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /*
     * Accelerometer +/-2 g.
     */
    status = MPU_WriteRegister(
        MPU_REG_ACCEL_CONFIG,
        0x00U
    );

    if (status != HAL_OK)
    {
        return status;
    }

    /*
     * Gyroscope X/Y/Z standby.
     * Accelerometer tetap aktif.
     */
    status = MPU_WriteRegister(
        MPU_REG_PWR_MGMT_2,
        0x07U
    );

    if (status != HAL_OK)
    {
        return status;
    }

    HAL_Delay(100U);

    return HAL_OK;
}

HAL_StatusTypeDef MPU_Sensor_ReadRaw(
    MPU_AccelRaw_t *data
)
{
    uint8_t buffer[6];
    HAL_StatusTypeDef status;

    if (data == NULL)
    {
        return HAL_ERROR;
    }

    status = HAL_I2C_Mem_Read(
        &hi2c1,
        MPU_ADDR,
        MPU_REG_ACCEL_XOUT_H,
        I2C_MEMADD_SIZE_8BIT,
        buffer,
        sizeof(buffer),
        MPU_I2C_TIMEOUT_MS
    );

    if (status != HAL_OK)
    {
        return status;
    }

    data->x =
        MPU_CombineBytes(
            buffer[0],
            buffer[1]
        );

    data->y =
        MPU_CombineBytes(
            buffer[2],
            buffer[3]
        );

    data->z =
        MPU_CombineBytes(
            buffer[4],
            buffer[5]
        );

    return HAL_OK;
}

HAL_StatusTypeDef MPU_Sensor_Calibrate(
    MPU_AccelCalibration_t *calibration
)
{
    int64_t x_sum = 0;
    int64_t y_sum = 0;
    int64_t z_sum = 0;

    int32_t z_average;
    int32_t expected_z;

    MPU_AccelRaw_t sample;
    HAL_StatusTypeDef status;

    if (calibration == NULL)
    {
        return HAL_ERROR;
    }

    HAL_Delay(500U);

    for (uint32_t i = 0U;
         i < MPU_CALIBRATION_SAMPLE_COUNT;
         i++)
    {
        status = MPU_Sensor_ReadRaw(
            &sample
        );

        if (status != HAL_OK)
        {
            return status;
        }

        x_sum += sample.x;
        y_sum += sample.y;
        z_sum += sample.z;

        HAL_Delay(
            MPU_CALIBRATION_PERIOD_MS
        );
    }

    calibration->x_offset =
        (int32_t)(
            x_sum /
            MPU_CALIBRATION_SAMPLE_COUNT
        );

    calibration->y_offset =
        (int32_t)(
            y_sum /
            MPU_CALIBRATION_SAMPLE_COUNT
        );

    z_average =
        (int32_t)(
            z_sum /
            MPU_CALIBRATION_SAMPLE_COUNT
        );

    if (z_average >= 0)
    {
        expected_z =
            MPU_ACCEL_LSB_PER_G;
    }
    else
    {
        expected_z =
            -MPU_ACCEL_LSB_PER_G;
    }

    calibration->z_offset =
        z_average - expected_z;

    return HAL_OK;
}

HAL_StatusTypeDef MPU_Sensor_ReadMg(
    const MPU_AccelCalibration_t *calibration,
    int16_t *x_mg,
    int16_t *y_mg,
    int16_t *z_mg
)
{
    MPU_AccelRaw_t raw_data;
    HAL_StatusTypeDef status;

    int32_t x_calibrated;
    int32_t y_calibrated;
    int32_t z_calibrated;

    int32_t x_value_mg;
    int32_t y_value_mg;
    int32_t z_value_mg;

    if ((calibration == NULL) ||
        (x_mg == NULL) ||
        (y_mg == NULL) ||
        (z_mg == NULL))
    {
        return HAL_ERROR;
    }

    status = MPU_Sensor_ReadRaw(
        &raw_data
    );

    if (status != HAL_OK)
    {
        return status;
    }

    x_calibrated =
        (int32_t)raw_data.x -
        calibration->x_offset;

    y_calibrated =
        (int32_t)raw_data.y -
        calibration->y_offset;

    z_calibrated =
        (int32_t)raw_data.z -
        calibration->z_offset;

    x_value_mg =
        (x_calibrated * 1000L) /
        MPU_ACCEL_LSB_PER_G;

    y_value_mg =
        (y_calibrated * 1000L) /
        MPU_ACCEL_LSB_PER_G;

    z_value_mg =
        (z_calibrated * 1000L) /
        MPU_ACCEL_LSB_PER_G;

    *x_mg = MPU_ClampInt16(x_value_mg);
    *y_mg = MPU_ClampInt16(y_value_mg);
    *z_mg = MPU_ClampInt16(z_value_mg);

    return HAL_OK;
}
