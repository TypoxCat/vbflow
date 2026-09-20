#include "main.h"
#include "usart.h"

#include <tk/tkernel.h>

#include <stdbool.h>
#include <limits.h>

#ifndef ARM_MATH_CM33
#define ARM_MATH_CM33
#endif

#include "arm_math.h"

#include "mpu_sensor.h"
#include "ring_buffer.h"
#include "ai_inference.h"

/*
 * Prioritas lebih kecil berarti lebih tinggi.
 */
#define SENSOR_TASK_PRIORITY       5
#define DSP_TASK_PRIORITY          10
#define CONTROL_TASK_PRIORITY      15

#define SENSOR_TASK_STACK_SIZE     2048
#define DSP_TASK_STACK_SIZE        4096

#define SENSOR_PERIOD_MS           10U

/*
 * Frame DSP:
 * 256 sampel pada 100 Hz = 2,56 detik.
 * Hop 128 sampel = frame baru setiap 1,28 detik.
 */
#define DSP_FRAME_LENGTH           256U
#define DSP_HOP_LENGTH             128U

#define DSP_SAMPLE_RATE_HZ         100U
#define DSP_ACCEL_LSB_PER_G        16384.0f
#define DSP_FFT_MIN_BIN            1U
#define DSP_FFT_MAX_BIN            ((DSP_FRAME_LENGTH / 2U) - 1U)

#define DSP_TWO_PI                 6.28318530717958647692f

/*
 * Validasi FFT memakai sinus sintetis.
 * Ubah menjadi 0 setelah validasi selesai.
 */
#define DSP_USE_SYNTHETIC_TEST  0U
#define DSP_TEST_BIN            26U
#define DSP_TEST_AMPLITUDE_G    0.100f


/*
 * Konfigurasi pengumpulan dataset CSV berlabel.
 *
 * Pemilihan kelas dilakukan melalui UART saat program berjalan:
 * 0 = stationary
 * 1 = low_vibration
 * 2 = high_vibration
 * 3 = inference label
 *
 * The session ID is incremented for every new recording.
 */
#define DATASET_LABEL_STATIONARY        0U
#define DATASET_LABEL_LOW_VIBRATION     1U
#define DATASET_LABEL_HIGH_VIBRATION    2U

#define DATASET_SESSION_ID_START        0U
#define DATASET_LABEL_INFERENCE         3U

typedef enum
{
    APP_MODE_IDLE = 0,
    APP_MODE_RECORD = 1,
    APP_MODE_INFERENCE = 2,
    APP_MODE_RECORD_INFERENCE = 3
} AppMode_t;

LOCAL volatile AppMode_t app_mode = APP_MODE_IDLE;
LOCAL volatile uint8_t record_active = 0U;
LOCAL volatile uint8_t dsp_reset_requested = 0U;

/* Dipilih saat startup melalui input UART. */
LOCAL uint32_t dataset_active_label = DATASET_LABEL_STATIONARY;
LOCAL const char *dataset_active_class_name = "stationary";
LOCAL uint32_t dataset_session_id = DATASET_SESSION_ID_START;

/*0
 * Band frekuensi untuk feature extraction.
 * Batas atas bersifat eksklusif.
 */
#define DSP_BAND_LOW_MAX_MILLIHZ    5000U
#define DSP_BAND_MID_MAX_MILLIHZ   15000U
#define DSP_BAND_HIGH_MAX_MILLIHZ  30000U
#define DSP_BAND_VERY_HIGH_MAX_MILLIHZ 50000U

_Static_assert(
    DSP_TEST_BIN > 0U,
    "DSP_TEST_BIN tidak boleh DC"
);

_Static_assert(
    DSP_TEST_BIN < (DSP_FRAME_LENGTH / 2U),
    "DSP_TEST_BIN harus di bawah Nyquist"
);


#if (DSP_HOP_LENGTH == 0U)
#error "DSP_HOP_LENGTH tidak boleh nol"
#endif

#if (DSP_HOP_LENGTH >= DSP_FRAME_LENGTH)
#error "DSP_HOP_LENGTH harus lebih kecil dari DSP_FRAME_LENGTH"
#endif

typedef struct
{
    uint16_t peak_bin;
    uint32_t peak_frequency_millihz;
    uint32_t peak_amplitude_mg;
    uint32_t rms_mg;
    uint32_t band_low_rms_mg;
    uint32_t band_mid_rms_mg;
    uint32_t band_high_rms_mg;
    uint32_t band_very_high_rms_mg;
} DSP_Features_t;

LOCAL void Sensor_Task(
    INT stacd,
    void *exinf
);

LOCAL void DSP_Task(
    INT stacd,
    void *exinf
);

LOCAL void Control_Task(
    INT stacd,
    void *exinf
);

LOCAL void Sensor_CyclicHandler(
    void *exinf
);

LOCAL void UART_Print(
    const char *text
);

LOCAL void UART_PrintChar(
    char character
);

LOCAL void UART_PrintUInt32(
    uint32_t value
);

LOCAL void UART_PrintFixed3(
    uint32_t milli_value
);


LOCAL void UART_PrintHex8(
    uint8_t value
);

LOCAL void FatalError(
    const char *message
);

LOCAL bool DSP_InitFft(void);

LOCAL uint32_t DSP_GToMg(
    float32_t value_g
);

LOCAL bool DSP_ProcessZAxis(
    const AccelSample_t *frame,
    DSP_Features_t *features
);

LOCAL void Control_PrintModePrompt(void);

LOCAL void Control_StartRecord(uint8_t label);

LOCAL void Control_StopSession(void);

LOCAL void Dataset_PrintCsvHeader(void);

LOCAL void Dataset_PrintCsvRow(
    uint32_t frame_number,
    uint32_t frame_start_ms,
    uint32_t frame_end_ms,
    uint32_t frame_span_ms,
    uint32_t frame_hop_ms,
    const DSP_Features_t *features
);

LOCAL ID sensor_task_id;
LOCAL ID dsp_task_id;
LOCAL ID uart_output_mutex_id;
LOCAL ID sensor_cyclic_id;

LOCAL AccelRingBuffer_t accel_ring_buffer;
LOCAL MPU_AccelCalibration_t calibration;

/*
 * Buffer frame dibuat statik agar tidak memakai stack task.
 */
LOCAL AccelSample_t dsp_frame[
    DSP_FRAME_LENGTH
];

/*
 * Buffer FFT dibuat statik agar tidak menggunakan stack task.
 * FFT tahap pertama memproses sumbu Z.
 */
LOCAL arm_rfft_fast_instance_f32 fft_instance;

LOCAL float32_t fft_input[
    DSP_FRAME_LENGTH
];

LOCAL float32_t fft_output[
    DSP_FRAME_LENGTH
];

LOCAL float32_t hann_window[
    DSP_FRAME_LENGTH
];

LOCAL float32_t hann_window_sum;
LOCAL float32_t hann_window_power_sum;

LOCAL volatile uint32_t sensor_sample_count;
LOCAL volatile uint32_t sensor_read_error_count;
LOCAL volatile uint32_t sensor_push_error_count;
LOCAL volatile uint32_t sensor_wakeup_overflow_count;

LOCAL volatile uint32_t dsp_consumed_count;
LOCAL volatile uint32_t dsp_frame_count;

LOCAL T_CTSK sensor_task_config =
{
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = Sensor_Task,
    .itskpri = SENSOR_TASK_PRIORITY,
    .stksz   = SENSOR_TASK_STACK_SIZE
};

LOCAL T_CTSK dsp_task_config =
{
    .tskatr =
        TA_HLNG |
        TA_RNG3 |
        TA_FPU,

    .task    = DSP_Task,
    .itskpri = DSP_TASK_PRIORITY,
    .stksz   = DSP_TASK_STACK_SIZE
};

LOCAL T_CTSK control_task_config =
{
    .tskatr  = TA_HLNG | TA_RNG3,
    .task    = Control_Task,
    .itskpri = CONTROL_TASK_PRIORITY,
    .stksz   = 1024
};

LOCAL T_CMTX uart_output_mutex_config =
{
    .mtxatr  = TA_TFIFO,
    .ceilpri = 0
};

LOCAL T_CCYC sensor_cyclic_config =
{
    .cycatr = TA_HLNG | TA_STA,
    .cychdr = Sensor_CyclicHandler,
    .cyctim = SENSOR_PERIOD_MS,
    .cycphs = SENSOR_PERIOD_MS
};

LOCAL void Control_PrintModePrompt(void)
{
    UART_Print("\r\nMode selection:\r\n");
    UART_Print("  r = record dataset\r\n");
    UART_Print("  i = inference\r\n");
    UART_Print("Input [r/i]:\r\n");
    UART_Print("# mode_prompt\r\n");
}

LOCAL void Control_StartRecord(uint8_t label)
{
    if (label == DATASET_LABEL_INFERENCE)
    {
        dataset_active_label = DATASET_LABEL_STATIONARY;
        dataset_active_class_name = "inference";
        app_mode = APP_MODE_RECORD_INFERENCE;
    }
    else
    {
    switch (label)
    {
        case DATASET_LABEL_STATIONARY:
            dataset_active_class_name = "stationary";
            break;

        case DATASET_LABEL_LOW_VIBRATION:
            dataset_active_class_name = "low_vibration";
            break;

        case DATASET_LABEL_HIGH_VIBRATION:
            dataset_active_class_name = "high_vibration";
            break;

        default:
            UART_Print("# invalid_label\r\n");
            return;
    }

        dataset_active_label = label;
        app_mode = APP_MODE_RECORD;
    }

    dataset_session_id++;
    record_active = 1U;
    dsp_reset_requested = 1U;

    if (app_mode == APP_MODE_RECORD_INFERENCE)
    {
        if (!AiInference_Init())
        {
            FatalError("AiInference_Init failed");
        }
    }

    (void)tk_loc_mtx(uart_output_mutex_id, TMO_FEVR);
    UART_Print("# app_mode=record\r\n");
    UART_Print("# record_start,session_id=");
    UART_PrintUInt32(dataset_session_id);
    UART_Print(",class_name=");
    UART_Print(dataset_active_class_name);
    UART_Print("\r\n");
    Dataset_PrintCsvHeader();
    (void)tk_unl_mtx(uart_output_mutex_id);
}

LOCAL void Control_StopSession(void)
{
    if (record_active != 0U)
    {
        (void)tk_loc_mtx(uart_output_mutex_id, TMO_FEVR);
        record_active = 0U;
        app_mode = APP_MODE_IDLE;
        UART_Print("# record_stop,session_id=");
        UART_PrintUInt32(dataset_session_id);
        UART_Print("\r\n");
        (void)tk_unl_mtx(uart_output_mutex_id);

        if (app_mode == APP_MODE_RECORD_INFERENCE)
        {
            AiInference_Deinit();
        }
    }
    else if ((app_mode == APP_MODE_INFERENCE) ||
             (app_mode == APP_MODE_RECORD_INFERENCE))
    {
        AiInference_Deinit();
        app_mode = APP_MODE_IDLE;
        UART_Print("# inference_stop\r\n");
    }

    Control_PrintModePrompt();
}

LOCAL void Control_Task(
    INT stacd,
    void *exinf
)
{
    uint8_t command = 0U;

    (void)stacd;
    (void)exinf;

    Control_PrintModePrompt();

    while (1)
    {
        if (HAL_UART_Receive(&huart2, &command, 1U, 100U) != HAL_OK)
        {
            continue;
        }

        if ((command == (uint8_t)'s') ||
            (command == (uint8_t)'S'))
        {
            Control_StopSession();
            continue;
        }

        if ((app_mode != APP_MODE_IDLE) ||
            ((command != (uint8_t)'r') &&
             (command != (uint8_t)'R') &&
             (command != (uint8_t)'i') &&
             (command != (uint8_t)'I')))
        {
            continue;
        }

        if ((command == (uint8_t)'i') ||
            (command == (uint8_t)'I'))
        {
            app_mode = APP_MODE_INFERENCE;
            dsp_reset_requested = 1U;

            if (!AiInference_Init())
            {
                FatalError("AiInference_Init failed");
            }

            UART_Print("i\r\n# app_mode=inference\r\n# inference_ready\r\n");
            continue;
        }

        UART_Print("r\r\nSelect dataset class:\r\n");
        UART_Print("  0 = stationary\r\n");
        UART_Print("  1 = low_vibration\r\n");
        UART_Print("  2 = high_vibration\r\n");
        UART_Print("  3 = inference_label\r\n");
        UART_Print("# class_prompt\r\n");
        UART_Print("Input [0/1/2/3]: ");

        while (1)
        {
            if (HAL_UART_Receive(&huart2, &command, 1U, HAL_MAX_DELAY) != HAL_OK)
            {
                continue;
            }

            if ((command >= (uint8_t)'0') &&
                (command <= (uint8_t)'3'))
            {
                Control_StartRecord((uint8_t)(command - (uint8_t)'0'));
                break;
            }

            UART_Print("\r\nInvalid input. Use 0, 1, 2, or 3: ");
        }
    }
}

LOCAL void UART_Print(
    const char *text
)
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

LOCAL void UART_PrintChar(
    char character
)
{
    (void)HAL_UART_Transmit(
        &huart2,
        (uint8_t *)&character,
        1U,
        50U
    );
}

LOCAL void UART_PrintUInt32(
    uint32_t value
)
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

        UART_PrintChar(
            digits[index]
        );
    }
}

LOCAL void UART_PrintFixed3(
    uint32_t milli_value
)
{
    uint32_t whole_part;
    uint32_t fractional_part;

    whole_part = milli_value / 1000U;
    fractional_part = milli_value % 1000U;

    UART_PrintUInt32(whole_part);
    UART_PrintChar('.');

    if (fractional_part < 100U)
    {
        UART_PrintChar('0');
    }

    if (fractional_part < 10U)
    {
        UART_PrintChar('0');
    }

    UART_PrintUInt32(fractional_part);
}


LOCAL void UART_PrintHex8(
    uint8_t value
)
{
    static const char hex_digits[] =
        "0123456789ABCDEF";

    UART_Print("0x");

    UART_PrintChar(
        hex_digits[
            (value >> 4U) & 0x0FU
        ]
    );

    UART_PrintChar(
        hex_digits[
            value & 0x0FU
        ]
    );
}

LOCAL void FatalError(
    const char *message
)
{
    UART_Print("\r\nFATAL: ");
    UART_Print(message);
    UART_Print("\r\n");

    BSP_LED_On(LED_GREEN);

    while (1)
    {
    }
}

/*
 * Inisialisasi RFFT 256 titik dan Hann window.
 */
LOCAL bool DSP_InitFft(void)
{
    arm_status fft_status;

    fft_status =
        arm_rfft_fast_init_f32(
            &fft_instance,
            DSP_FRAME_LENGTH
        );

    if (fft_status != ARM_MATH_SUCCESS)
    {
        return false;
    }

    hann_window_sum = 0.0f;
    hann_window_power_sum = 0.0f;

    for (uint32_t i = 0U;
         i < DSP_FRAME_LENGTH;
         i++)
    {
        float32_t phase;

        phase =
            DSP_TWO_PI *
            (float32_t)i /
            (float32_t)(
                DSP_FRAME_LENGTH - 1U
            );

        hann_window[i] =
            0.5f -
            (0.5f * arm_cos_f32(phase));

        hann_window_sum +=
            hann_window[i];

        hann_window_power_sum +=
            hann_window[i] * hann_window[i];
    }

    if ((hann_window_sum <= 0.0f) ||
        (hann_window_power_sum <= 0.0f))
    {
        return false;
    }

    return true;
}

/*
 * FFT sumbu Z:
 * 1. hitung dan hilangkan mean;
 * 2. konversi raw LSB ke g;
 * 3. terapkan Hann window;
 * 4. jalankan RFFT;
 * 5. cari puncak spektrum selain DC.
 */
LOCAL uint32_t DSP_GToMg(
    float32_t value_g
)
{
    float32_t value_mg;

    if (value_g <= 0.0f)
    {
        return 0U;
    }

    value_mg = value_g * 1000.0f;

    if (value_mg > 1000000.0f)
    {
        return 1000000U;
    }

    return (uint32_t)(value_mg + 0.5f);
}

/*
 * Feature extraction sumbu Z:
 * 1. buat sinyal centered dalam satuan g;
 * 2. hitung RMS time-domain;
 * 3. terapkan Hann window;
 * 4. jalankan RFFT;
 * 5. cari peak frequency dan peak amplitude;
 * 6. hitung RMS band 0-5 Hz, 5-15 Hz, 15-30 Hz, dan very high 30-50 Hz.
 */
LOCAL bool DSP_ProcessZAxis(
    const AccelSample_t *frame,
    DSP_Features_t *features
)
{
#if (DSP_USE_SYNTHETIC_TEST == 0U)
    float32_t mean_raw = 0.0f;
#endif

    float32_t time_square_sum = 0.0f;
    float32_t peak_power = -1.0f;
    float32_t low_band_power = 0.0f;
    float32_t mid_band_power = 0.0f;
    float32_t high_band_power = 0.0f;
    float32_t very_high_band_power = 0.0f;

    float32_t peak_magnitude;
    float32_t peak_amplitude_g;
    float32_t rms_g;
    float32_t low_band_rms_g;
    float32_t mid_band_rms_g;
    float32_t high_band_rms_g;
    float32_t very_high_band_rms_g;
    float32_t band_scale;

    uint16_t selected_bin = DSP_FFT_MIN_BIN;

    if ((frame == NULL) ||
        (features == NULL))
    {
        return false;
    }

#if (DSP_USE_SYNTHETIC_TEST == 0U)
    for (uint32_t i = 0U;
         i < DSP_FRAME_LENGTH;
         i++)
    {
        mean_raw += (float32_t)frame[i].z;
    }

    mean_raw /= (float32_t)DSP_FRAME_LENGTH;
#endif

    for (uint32_t i = 0U;
         i < DSP_FRAME_LENGTH;
         i++)
    {
        float32_t signal_g;

#if (DSP_USE_SYNTHETIC_TEST == 1U)
        float32_t phase;

        phase =
            DSP_TWO_PI *
            (float32_t)DSP_TEST_BIN *
            (float32_t)i /
            (float32_t)DSP_FRAME_LENGTH;

        signal_g =
            DSP_TEST_AMPLITUDE_G *
            arm_sin_f32(phase);
#else
        signal_g =
            ((float32_t)frame[i].z - mean_raw) /
            DSP_ACCEL_LSB_PER_G;
#endif

        time_square_sum += signal_g * signal_g;
        fft_input[i] = signal_g * hann_window[i];
    }

    if (arm_sqrt_f32(
            time_square_sum /
            (float32_t)DSP_FRAME_LENGTH,
            &rms_g
        ) != ARM_MATH_SUCCESS)
    {
        return false;
    }

    arm_rfft_fast_f32(
        &fft_instance,
        fft_input,
        fft_output,
        0U
    );

    for (uint16_t bin = DSP_FFT_MIN_BIN;
         bin <= DSP_FFT_MAX_BIN;
         bin++)
    {
        float32_t real_part;
        float32_t imaginary_part;
        float32_t power;
        uint32_t frequency_millihz;

        real_part = fft_output[2U * bin];
        imaginary_part = fft_output[(2U * bin) + 1U];

        power =
            (real_part * real_part) +
            (imaginary_part * imaginary_part);

        frequency_millihz =
            (((uint32_t)bin *
              DSP_SAMPLE_RATE_HZ *
              1000U) +
             (DSP_FRAME_LENGTH / 2U)) /
            DSP_FRAME_LENGTH;

        if (power > peak_power)
        {
            peak_power = power;
            selected_bin = bin;
        }

        if (frequency_millihz < DSP_BAND_LOW_MAX_MILLIHZ)
        {
            low_band_power += power;
        }
        else if (frequency_millihz < DSP_BAND_MID_MAX_MILLIHZ)
        {
            mid_band_power += power;
        }
        else if (frequency_millihz < DSP_BAND_HIGH_MAX_MILLIHZ)
        {
            high_band_power += power;
        }
        else if (frequency_millihz < DSP_BAND_VERY_HIGH_MAX_MILLIHZ)
        {
            very_high_band_power += power;
        }
    }

    if (peak_power < 0.0f)
    {
        return false;
    }

    if (arm_sqrt_f32(
            peak_power,
            &peak_magnitude
        ) != ARM_MATH_SUCCESS)
    {
        return false;
    }

    peak_amplitude_g =
        (2.0f * peak_magnitude) /
        hann_window_sum;

    /*
     * Koreksi RMS untuk window Hann dan spektrum satu sisi:
     * RMS_band^2 = 2*sum(|X[k]|^2) / (N*sum(w[n]^2)).
     */
    band_scale =
        2.0f /
        ((float32_t)DSP_FRAME_LENGTH *
         hann_window_power_sum);

    if (arm_sqrt_f32(
            low_band_power * band_scale,
            &low_band_rms_g
        ) != ARM_MATH_SUCCESS)
    {
        return false;
    }

    if (arm_sqrt_f32(
            mid_band_power * band_scale,
            &mid_band_rms_g
        ) != ARM_MATH_SUCCESS)
    {
        return false;
    }

    if (arm_sqrt_f32(
            high_band_power * band_scale,
            &high_band_rms_g
        ) != ARM_MATH_SUCCESS)
    {
        return false;
    }

    if (arm_sqrt_f32(
            very_high_band_power * band_scale,
            &very_high_band_rms_g
        ) != ARM_MATH_SUCCESS)
    {
        return false;
    }

    features->peak_bin = selected_bin;

    features->peak_frequency_millihz =
        (((uint32_t)selected_bin *
          DSP_SAMPLE_RATE_HZ *
          1000U) +
         (DSP_FRAME_LENGTH / 2U)) /
        DSP_FRAME_LENGTH;

    features->peak_amplitude_mg =
        DSP_GToMg(peak_amplitude_g);

    features->rms_mg =
        DSP_GToMg(rms_g);

    features->band_low_rms_mg =
        DSP_GToMg(low_band_rms_g);

    features->band_mid_rms_mg =
        DSP_GToMg(mid_band_rms_g);

    features->band_high_rms_mg =
        DSP_GToMg(high_band_rms_g);

    features->band_very_high_rms_mg =
        DSP_GToMg(very_high_band_rms_g);

    return true;
}

/*
 * Header CSV. Baris metadata yang diawali '#' dapat diabaikan
 * saat dibaca oleh Python/pandas dengan parameter comment='#'.
 */
LOCAL void Dataset_PrintCsvHeader(void)
{
    UART_Print(
        "label,class_name,session_id,frame,start_ms,end_ms,"
        "span_ms,hop_ms,peak_bin,peak_hz,peak_mg,rms_mg,"
        "band_0_5_mg,band_5_15_mg,band_15_30_mg,band_30_50_mg,"
        "read_err,push_err,ring_ovf,wake_ovf\r\n"
    );
}

/*
 * Satu frame menghasilkan satu baris CSV berlabel.
 */
LOCAL void Dataset_PrintCsvRow(
    uint32_t frame_number,
    uint32_t frame_start_ms,
    uint32_t frame_end_ms,
    uint32_t frame_span_ms,
    uint32_t frame_hop_ms,
    const DSP_Features_t *features
)
{
    if (features == NULL)
    {
        return;
    }

    UART_PrintUInt32(dataset_active_label);
    UART_PrintChar(',');

    UART_Print(dataset_active_class_name);
    UART_PrintChar(',');

    UART_PrintUInt32(dataset_session_id);
    UART_PrintChar(',');

    UART_PrintUInt32(frame_number);
    UART_PrintChar(',');

    UART_PrintUInt32(frame_start_ms);
    UART_PrintChar(',');

    UART_PrintUInt32(frame_end_ms);
    UART_PrintChar(',');

    UART_PrintUInt32(frame_span_ms);
    UART_PrintChar(',');

    UART_PrintUInt32(frame_hop_ms);
    UART_PrintChar(',');

    UART_PrintUInt32((uint32_t)features->peak_bin);
    UART_PrintChar(',');

    UART_PrintFixed3(features->peak_frequency_millihz);
    UART_PrintChar(',');

    UART_PrintUInt32(features->peak_amplitude_mg);
    UART_PrintChar(',');

    UART_PrintUInt32(features->rms_mg);
    UART_PrintChar(',');

    UART_PrintUInt32(features->band_low_rms_mg);
    UART_PrintChar(',');

    UART_PrintUInt32(features->band_mid_rms_mg);
    UART_PrintChar(',');

    UART_PrintUInt32(features->band_high_rms_mg);
    UART_PrintChar(',');

    UART_PrintUInt32(features->band_very_high_rms_mg);
    UART_PrintChar(',');

    UART_PrintUInt32(sensor_read_error_count);
    UART_PrintChar(',');

    UART_PrintUInt32(sensor_push_error_count);
    UART_PrintChar(',');

    UART_PrintUInt32(
        AccelRingBuffer_GetOverflowCount(
            &accel_ring_buffer
        )
    );
    UART_PrintChar(',');

    UART_PrintUInt32(sensor_wakeup_overflow_count);
    UART_Print("\r\n");
}

/*
 * Handler periodik hanya membangunkan Sensor Task.
 * Tidak ada operasi I2C atau UART di handler.
 */
LOCAL void Sensor_CyclicHandler(
    void *exinf
)
{
    ER error_code;

    (void)exinf;

    error_code =
        tk_wup_tsk(sensor_task_id);

    if (error_code == E_QOVR)
    {
        sensor_wakeup_overflow_count++;
    }
}

/*
 * Producer ring buffer pada 100 Hz.
 */
LOCAL void Sensor_Task(
    INT stacd,
    void *exinf
)
{
    AccelSample_t sample;
    MPU_AccelRaw_t raw_data;

    HAL_StatusTypeDef status;

    SYSTIM operating_time;
    ER time_error;

    (void)stacd;
    (void)exinf;

    while (1)
    {
        /*
         * Menunggu wake-up setiap 10 ms
         * dari cyclic handler.
         */
        (void)tk_slp_tsk(TMO_FEVR);

        /*
         * Ambil timestamp sebelum transaksi I2C.
         */
        time_error =
            tk_get_otm(
                &operating_time
            );

        if (time_error == E_OK)
        {
            sample.timestamp_ms =
                (uint32_t)operating_time.lo;
        }
        else
        {
            sample.timestamp_ms = 0U;
        }

        /*
         * Baca data mentah accelerometer.
         */
        status =
            MPU_Sensor_ReadRaw(
                &raw_data
            );

        if (status == HAL_OK)
        {
            /*
             * Kurangi offset hasil kalibrasi.
             * Nilai tetap dalam satuan raw LSB agar
             * resolusi sensor tidak hilang sebelum FFT.
             */
            sample.x =
                (int32_t)raw_data.x -
                calibration.x_offset;

            sample.y =
                (int32_t)raw_data.y -
                calibration.y_offset;

            sample.z =
                (int32_t)raw_data.z -
                calibration.z_offset;

            /*
             * Masukkan sampel ke ring buffer.
             */
            if (AccelRingBuffer_Push(
                    &accel_ring_buffer,
                    &sample))
            {
                sensor_sample_count++;
            }
            else
            {
                sensor_push_error_count++;
            }
        }
        else
        {
            sensor_read_error_count++;
        }
    }
}

/*
 * Consumer ring buffer dan pembentuk frame overlap.
 *
 * Frame pertama memerlukan 256 sampel.
 * Sesudah frame selesai, 128 sampel terakhir dipindah
 * ke awal buffer. Karena itu frame berikutnya hanya
 * memerlukan 128 sampel baru.
 */
LOCAL void DSP_Task(
    INT stacd,
    void *exinf
)
{
    AccelSample_t sample;

    uint16_t frame_fill_count = 0U;
    uint32_t session_frame_count = 0U;

    uint32_t previous_frame_start_ms = 0U;
    uint8_t previous_frame_valid = 0U;

    (void)stacd;
    (void)exinf;

    while (1)
    {
        uint8_t consumed_any = 0U;

        if (dsp_reset_requested != 0U)
        {
            frame_fill_count = 0U;
            session_frame_count = 0U;
            previous_frame_valid = 0U;
            dsp_reset_requested = 0U;
        }

        /*
         * Kosongkan seluruh sampel yang tersedia.
         */
        while (AccelRingBuffer_Pop(
                   &accel_ring_buffer,
                   &sample))
        {
            uint32_t frame_start_ms;
            uint32_t frame_end_ms;
            uint32_t frame_span_ms;
            uint32_t frame_hop_ms;

            DSP_Features_t features = {0};

            consumed_any = 1U;
            dsp_consumed_count++;

            dsp_frame[frame_fill_count] =
                sample;

            frame_fill_count++;

            if (frame_fill_count ==
                DSP_FRAME_LENGTH)
            {
                dsp_frame_count++;

                frame_start_ms =
                    dsp_frame[0]
                        .timestamp_ms;

                frame_end_ms =
                    dsp_frame[
                        DSP_FRAME_LENGTH - 1U
                    ].timestamp_ms;

                frame_span_ms =
                    frame_end_ms -
                    frame_start_ms;

                if (previous_frame_valid != 0U)
                {
                    frame_hop_ms =
                        frame_start_ms -
                        previous_frame_start_ms;
                }
                else
                {
                    frame_hop_ms = 0U;
                    previous_frame_valid = 1U;
                }

                if (!DSP_ProcessZAxis(
                        dsp_frame,
                        &features))
                {
                    FatalError(
                        "DSP_ProcessZAxis gagal"
                    );
                }

                if ((app_mode == APP_MODE_INFERENCE) ||
                    (app_mode == APP_MODE_RECORD_INFERENCE))
                {
                    float nn_in[AI_IN_SIZE];
                    int prediction;

                    nn_in[0] =
                        (float)features.peak_frequency_millihz /
                        1000.0f;
                    nn_in[1] = (float)features.peak_amplitude_mg;
                    nn_in[2] = (float)features.rms_mg;
                    nn_in[3] = (float)features.band_low_rms_mg;
                    nn_in[4] = (float)features.band_mid_rms_mg;
                    nn_in[5] = (float)features.band_high_rms_mg;

                    prediction = AiInference_RunFromArray(nn_in);

                    if ((app_mode == APP_MODE_RECORD_INFERENCE) &&
                        (record_active != 0U) &&
                        (prediction >= 0) &&
                        (prediction <= 2))
                    {
                        dataset_active_label = (uint32_t)prediction;

                        if (prediction == 0)
                        {
                            dataset_active_class_name = "stationary";
                        }
                        else if (prediction == 1)
                        {
                            dataset_active_class_name = "low_vibration";
                        }
                        else
                        {
                            dataset_active_class_name = "high_vibration";
                        }

                        (void)tk_loc_mtx(
                            uart_output_mutex_id,
                            TMO_FEVR
                        );

                        if (record_active != 0U)
                        {
                            session_frame_count++;
                            Dataset_PrintCsvRow(
                                session_frame_count,
                                frame_start_ms,
                                frame_end_ms,
                                frame_span_ms,
                                frame_hop_ms,
                                &features
                            );
                        }

                        (void)tk_unl_mtx(uart_output_mutex_id);
                    }
                }
                else if ((app_mode == APP_MODE_RECORD) &&
                         (record_active != 0U))
                {
                    (void)tk_loc_mtx(
                        uart_output_mutex_id,
                        TMO_FEVR
                    );

                    if ((app_mode == APP_MODE_RECORD) &&
                        (record_active != 0U))
                    {
                        session_frame_count++;
                        Dataset_PrintCsvRow(
                            session_frame_count,
                            frame_start_ms,
                            frame_end_ms,
                            frame_span_ms,
                            frame_hop_ms,
                            &features
                        );
                    }

                    (void)tk_unl_mtx(uart_output_mutex_id);
                }

                previous_frame_start_ms =
                    frame_start_ms;

                /*
                 * Pertahankan 128 sampel terakhir.
                 *
                 * Frame lama:
                 * [0 ........ 127][128 ...... 255]
                 *
                 * Setelah digeser:
                 * [128 ...... 255][ruang sampel baru]
                 */
                for (uint16_t i = 0U;
                     i <
                     (DSP_FRAME_LENGTH -
                      DSP_HOP_LENGTH);
                     i++)
                {
                    dsp_frame[i] =
                        dsp_frame[
                            i + DSP_HOP_LENGTH
                        ];
                }

                frame_fill_count =
                    (uint16_t)(
                        DSP_FRAME_LENGTH -
                        DSP_HOP_LENGTH
                    );

                BSP_LED_Toggle(
                    LED_GREEN
                );
            }
        }

        if (consumed_any == 0U)
        {
            /*
             * Ring buffer kosong.
             * Beri kesempatan CPU kepada task lain.
             */
            (void)tk_dly_tsk(1U);
        }
    }
}

EXPORT INT usermain(void)
{
    ER error_code;
    uint8_t who_am_i = 0U;
    HAL_StatusTypeDef sensor_status;

    BSP_LED_Init(LED_GREEN);
    BSP_LED_Off(LED_GREEN);

    UART_Print("\r\n");
    UART_Print("# micro_tkernel_fft_dataset_logger\r\n");

    UART_Print("# sample_rate_hz=");
    UART_PrintUInt32(DSP_SAMPLE_RATE_HZ);
    UART_Print("\r\n");

    UART_Print("# frame_length=");
    UART_PrintUInt32(DSP_FRAME_LENGTH);
    UART_Print("\r\n");

    UART_Print("# hop_length=");
    UART_PrintUInt32(DSP_HOP_LENGTH);
    UART_Print("\r\n");

    UART_Print("# bands_hz=low:0-5;mid:5-15;high:15-30;very_high:30-50\r\n");

    AccelRingBuffer_Init(
        &accel_ring_buffer
    );

    sensor_sample_count = 0U;
    sensor_read_error_count = 0U;
    sensor_push_error_count = 0U;
    sensor_wakeup_overflow_count = 0U;

    dsp_consumed_count = 0U;
    dsp_frame_count = 0U;

    UART_Print("# init_cmsis_dsp_rfft\r\n");

    if (!DSP_InitFft())
    {
        FatalError(
            "DSP_InitFft gagal"
        );
    }

    UART_Print(
        "# rfft_256_hann_ready\r\n"
    );

#if (DSP_USE_SYNTHETIC_TEST == 1U)
    UART_Print(
        "# mode=synthetic_test\r\n"
    );
#else
    UART_Print(
        "# mode=accelerometer_z\r\n"
    );
#endif

    UART_Print("# init_mpu\r\n");

    sensor_status =
        MPU_Sensor_Init(
            &who_am_i
        );

    if (sensor_status != HAL_OK)
    {
        FatalError(
            "MPU_Sensor_Init gagal"
        );
    }

    UART_Print("# who_am_i=");
    UART_PrintHex8(who_am_i);
    UART_Print("\r\n");

    UART_Print(
        "# calibration_samples=1000;keep_sensor_flat_and_still\r\n"
    );

    sensor_status =
        MPU_Sensor_Calibrate(
            &calibration
        );

    if (sensor_status != HAL_OK)
    {
        FatalError(
            "MPU_Sensor_Calibrate gagal"
        );
    }

    UART_Print("# calibration_done\r\n");

    sensor_task_id =
        tk_cre_tsk(
            &sensor_task_config
        );

    if (sensor_task_id < E_OK)
    {
        FatalError(
            "tk_cre_tsk Sensor gagal"
        );
    }

    dsp_task_id =
        tk_cre_tsk(
            &dsp_task_config
        );

    if (dsp_task_id < E_OK)
    {
        FatalError(
            "tk_cre_tsk DSP gagal"
        );
    }

    uart_output_mutex_id =
        tk_cre_mtx(
            &uart_output_mutex_config
        );

    if (uart_output_mutex_id < E_OK)
    {
        FatalError(
            "tk_cre_mtx UART failed"
        );
    }

    ID control_task_id =
        tk_cre_tsk(
            &control_task_config
        );

    if (control_task_id < E_OK)
    {
        FatalError(
            "tk_cre_tsk Control failed"
        );
    }

    error_code =
        tk_sta_tsk(
            sensor_task_id,
            0
        );

    if (error_code < E_OK)
    {
        FatalError(
            "tk_sta_tsk Sensor gagal"
        );
    }

    error_code =
        tk_sta_tsk(
            control_task_id,
            0
        );

    if (error_code < E_OK)
    {
        FatalError(
            "tk_sta_tsk Control failed"
        );
    }

    error_code =
        tk_sta_tsk(
            dsp_task_id,
            0
        );

    if (error_code < E_OK)
    {
        FatalError(
            "tk_sta_tsk DSP gagal"
        );
    }

    /*
     * TA_STA membuat cyclic handler langsung aktif.
     */
    sensor_cyclic_id =
        tk_cre_cyc(
            &sensor_cyclic_config
        );

    if (sensor_cyclic_id < E_OK)
    {
        FatalError(
            "tk_cre_cyc gagal"
        );
    }

    UART_Print(
        "# sensor_task_100_hz_active\r\n"
    );

    /*
     * Initial task menunggu selamanya.
     */
    (void)tk_slp_tsk(TMO_FEVR);

    return 0;
}
