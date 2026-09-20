/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : MPU accelerometer-only acquisition at 100 Hz
  ******************************************************************************
  * @attention
  *
  * Baseline sebelum integrasi micro T-Kernel:
  * - HAL timebase menggunakan TIM6
  * - Accelerometer X/Y/Z saja
  * - Gyroscope dinonaktifkan melalui PWR_MGMT_2
  * - Akuisisi 100 Hz
  * - UART CSV 10 Hz
  * - Tidak menggunakan fungsi formatted-output
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "icache.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/**
  * @brief Data mentah accelerometer 3-sumbu.
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

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Alamat I2C HAL: alamat 7-bit digeser satu bit ke kiri. */
#define MPU_ADDR                         (0x68U << 1)

/* Register sensor kompatibel MPU. */
#define MPU_REG_SMPLRT_DIV               0x19U
#define MPU_REG_CONFIG                   0x1AU
#define MPU_REG_ACCEL_CONFIG             0x1CU
#define MPU_REG_ACCEL_XOUT_H             0x3BU
#define MPU_REG_PWR_MGMT_1               0x6BU
#define MPU_REG_PWR_MGMT_2               0x6CU
#define MPU_REG_WHO_AM_I                 0x75U

/* Sensitivitas accelerometer pada rentang +/-2 g. */
#define MPU_ACCEL_LSB_PER_G              16384L

/* Kalibrasi 1000 sampel pada 100 Hz: sekitar 10-11 detik. */
#define MPU_CALIBRATION_SAMPLE_COUNT     1000U

/* Sampling utama 100 Hz. */
#define MPU_SAMPLE_PERIOD_MS             10U

/* UART dicetak setiap 10 sampel: sekitar 10 Hz. */
#define MPU_UART_PRINT_DIVIDER           10U

/* Timeout transaksi I2C. */
#define MPU_I2C_TIMEOUT_MS               20U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */

void knl_start_mtkernel(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief Mengirim string melalui USART2.
  */
static void UART_Print(const char *text)
{
  uint16_t length = 0U;

  if (text == NULL)
  {
    return;
  }

  while ((text[length] != '\0') &&
         (length < 1000U))
  {
    length++;
  }

  if (length > 0U)
  {
    (void)HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        length,
        200U
    );
  }
}

/**
  * @brief Mengirim satu karakter melalui USART2.
  */
static void UART_PrintChar(char character)
{
  (void)HAL_UART_Transmit(
      &huart2,
      (uint8_t *)&character,
      1U,
      50U
  );
}

/**
  * @brief Mengirim bilangan unsigned 32-bit dalam format desimal.
  */
static void UART_PrintUInt32(uint32_t value)
{
  char digits[10];
  uint32_t index = 0U;

  if (value == 0U)
  {
    UART_PrintChar('0');
    return;
  }

  while ((value > 0U) &&
         (index < sizeof(digits)))
  {
    digits[index] =
        (char)('0' + (value % 10U));

    value /= 10U;
    index++;
  }

  while (index > 0U)
  {
    index--;
    UART_PrintChar(digits[index]);
  }
}

/**
  * @brief Mengirim bilangan signed 32-bit dalam format desimal.
  */
static void UART_PrintInt32(int32_t value)
{
  uint32_t magnitude;

  if (value < 0)
  {
    UART_PrintChar('-');

    /*
     * Konversi ini tetap aman untuk INT32_MIN.
     */
    magnitude =
        (uint32_t)(-(value + 1)) + 1U;
  }
  else
  {
    magnitude = (uint32_t)value;
  }

  UART_PrintUInt32(magnitude);
}

/**
  * @brief Mengirim satu byte sebagai dua digit heksadesimal.
  */
static void UART_PrintHex8(uint8_t value)
{
  static const char hex_digits[] =
      "0123456789ABCDEF";

  UART_Print("0x");
  UART_PrintChar(
      hex_digits[(value >> 4U) & 0x0FU]
  );
  UART_PrintChar(
      hex_digits[value & 0x0FU]
  );
}

/**
  * @brief Mengirim nama status HAL.
  */
static void UART_PrintHALStatus(
    HAL_StatusTypeDef status
)
{
  switch (status)
  {
    case HAL_OK:
      UART_Print("HAL_OK");
      break;

    case HAL_ERROR:
      UART_Print("HAL_ERROR");
      break;

    case HAL_BUSY:
      UART_Print("HAL_BUSY");
      break;

    case HAL_TIMEOUT:
      UART_Print("HAL_TIMEOUT");
      break;

    default:
      UART_Print("HAL_STATUS_UNKNOWN");
      break;
  }
}

/**
  * @brief Menulis satu register sensor.
  */
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

/**
  * @brief Membaca satu register sensor.
  */
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

/**
  * @brief Menggabungkan dua byte menjadi signed 16-bit.
  */
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

/**
  * @brief Membaca enam byte accelerometer X/Y/Z.
  */
static HAL_StatusTypeDef MPU_ReadAccelRaw(
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

/**
  * @brief Inisialisasi sensor untuk accelerometer-only.
  */
static HAL_StatusTypeDef MPU_InitAccelOnly(
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
   * 0x68 adalah identitas MPU6050 standar.
   * 0x72 tetap diterima karena modul pengguna
   * sudah terbukti mendukung register accelerometer.
   */
  if ((*who_am_i != 0x68U) &&
      (*who_am_i != 0x72U))
  {
    return HAL_ERROR;
  }

  /*
   * Keluar dari sleep mode.
   */
  status = MPU_WriteRegister(
      MPU_REG_PWR_MGMT_1,
      0x00U
  );

  if (status != HAL_OK)
  {
    return status;
  }

  HAL_Delay(100U);

  /*
   * DLPF configuration 3.
   */
  status = MPU_WriteRegister(
      MPU_REG_CONFIG,
      0x03U
  );

  if (status != HAL_OK)
  {
    return status;
  }

  /*
   * Dengan keluaran dasar 1 kHz dan divider 9:
   * 1000 / (1 + 9) = 100 Hz.
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
   * Nonaktifkan gyroscope X/Y/Z.
   * STBY_XG, STBY_YG, STBY_ZG = 1.
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

/**
  * @brief Menghitung offset accelerometer saat startup.
  *
  * Sensor harus diletakkan rata dan tidak digerakkan.
  * Gravitasi +/-1 g pada sumbu Z tetap dipertahankan.
  */
static HAL_StatusTypeDef MPU_CalibrateAccel(
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
    status = MPU_ReadAccelRaw(&sample);

    if (status != HAL_OK)
    {
      UART_Print(
          "Kalibrasi gagal pada sampel "
      );
      UART_PrintUInt32(i + 1U);
      UART_Print(": ");
      UART_PrintHALStatus(status);
      UART_Print("\r\n");

      return status;
    }

    x_sum += sample.x;
    y_sum += sample.y;
    z_sum += sample.z;

    HAL_Delay(MPU_SAMPLE_PERIOD_MS);

    /*
     * Tampilkan progres setiap 100 sampel.
     */
    if (((i + 1U) % 100U) == 0U)
    {
      UART_Print("Kalibrasi ");
      UART_PrintUInt32(
          ((i + 1U) * 100U) /
          MPU_CALIBRATION_SAMPLE_COUNT
      );
      UART_Print("%\r\n");
    }
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

/**
  * @brief Mencetak satu baris CSV accelerometer.
  */
static void UART_PrintAccelCsv(
    uint32_t time_ms,
    int32_t accel_x_mg,
    int32_t accel_y_mg,
    int32_t accel_z_mg,
    uint32_t sample_counter,
    uint32_t read_error_counter,
    uint32_t overrun_counter
)
{
  UART_PrintUInt32(time_ms);
  UART_PrintChar(',');

  UART_PrintInt32(accel_x_mg);
  UART_PrintChar(',');

  UART_PrintInt32(accel_y_mg);
  UART_PrintChar(',');

  UART_PrintInt32(accel_z_mg);
  UART_PrintChar(',');

  UART_PrintUInt32(sample_counter);
  UART_PrintChar(',');

  UART_PrintUInt32(read_error_counter);
  UART_PrintChar(',');

  UART_PrintUInt32(overrun_counter);
  UART_Print("\r\n");
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

	/* Belum ada variabel aplikasi.
	 * Aplikasi akan dijalankan dari usermain.c.
	 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_ICACHE_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */

  knl_start_mtkernel();

  /*
   * Normalnya fungsi kernel tidak kembali.
   */
  Error_Handler();

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* Tidak akan dicapai selama kernel berjalan. */

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV2;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x08FFF000;
  MPU_InitStruct.LimitAddress = 0x08FFFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
