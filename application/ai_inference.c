/* Minimal ai_inference wrapper for quick model smoke-test.
 * - Accepts 6 floats in training order
 * - Applies exact sklearn StandardScaler values
 * - Calls generated X-CUBE-AI runtime
 * - Prints `# nn_pred=<label>` over UART
 */

#include "ai_inference.h"
#include "network.h"
#include "network_data.h"
#include "ai_platform.h"
#include "usart.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

/* Activations buffer sized from generated params */
AI_ALIGNED(4) static ai_u8 ai_activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
static ai_handle ai_activations_ptr[1];
static ai_handle ai_weights_ptr[1];

static ai_handle g_net = AI_HANDLE_NULL;
static float ai_input[AI_IN_SIZE];
static float ai_output[AI_OUT_SIZE];

/* Exact scaler values exported from training */
static const float scaler_mean[AI_IN_SIZE] = {
    23.167744444444445f,
    5.288888888888889f,
    12.627777777777778f,
    2.2666666666666666f,
    4.981481481481482f,
    6.7407407407407405f
};

static const float scaler_scale[AI_IN_SIZE] = {
    13.539358669007358f,
    8.663346942545564f,
    23.393580289588684f,
    3.3198616660158424f,
    6.558824253095245f,
    13.893224014798626f
};

static void ai_uart_print(const char *s)
{
    if (s == NULL) return;
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

/* Simple float -> string converter that doesn't rely on printf %f support.
 * Writes into provided buffer (must be reasonably large). Returns number of
 * characters written (excluding terminating NUL).
 */
static int ftoa_simple(char *out, size_t out_size, float v, int prec)
{
    if (out_size == 0) return 0;
    if (isnan(v)) {
        strncpy(out, "nan", out_size);
        out[out_size-1] = '\0';
        return (int)strlen(out);
    }
    if (isinf(v)) {
        if (v < 0) strncpy(out, "-inf", out_size);
        else strncpy(out, "inf", out_size);
        out[out_size-1] = '\0';
        return (int)strlen(out);
    }

    char *p = out;
    size_t remaining = out_size;

    if (v < 0.0f) {
        if (remaining > 1) { *p++ = '-'; --remaining; }
        v = -v;
    }

    long ipart = (long)v;
    float frac = v - (float)ipart;

    /* scale fractional part */
    long pow10 = 1;
    for (int i = 0; i < prec; ++i) pow10 *= 10;
    long frac_int = (long)(frac * (float)pow10 + 0.5f);
    if (frac_int >= pow10) { ipart += 1; frac_int -= pow10; }

    int n = snprintf(p, remaining, "%ld", ipart);
    if (n < 0) n = 0;
    if ((size_t)n >= remaining) { out[out_size-1] = '\0'; return (int)strlen(out); }
    p += n; remaining -= (size_t)n;

    if (prec > 0 && remaining > 1) {
        *p++ = '.'; --remaining;
        /* fractional with leading zeros */
        char fracbuf[32];
        snprintf(fracbuf, sizeof(fracbuf), "%0*ld", prec, frac_int);
        size_t fl = strnlen(fracbuf, sizeof(fracbuf));
        if (fl >= remaining) fl = remaining - 1;
        memcpy(p, fracbuf, fl);
        p += fl; remaining -= fl;
    }

    *p = '\0';
    return (int)strlen(out);
}

static void ai_hex_print(const void *ptr, size_t len)
{
    const uint8_t *p = (const uint8_t*)ptr;
    char buf[4*16+32];
    char *q = buf;
    size_t rem = sizeof(buf);
    int n;
    for (size_t i = 0; i < len && i < 16; ++i) {
        n = snprintf(q, rem, "%02X", p[i]);
        if (n <= 0) break;
        q += n; rem -= (size_t)n;
        if (i < len-1 && i < 15) { if (rem>1) { *q++ = ' '; --rem; } }
    }
    *q = '\0';
    ai_uart_print(buf);
    ai_uart_print("\r\n");
}

bool AiInference_Init(void)
{
    ai_error err;

    /* Prepare activation and weight handles at runtime (function calls not constant at compile time) */
    ai_activations_ptr[0] = AI_HANDLE_PTR(ai_activations);
    /* ai_network_data_weights_get() returns a pointer to the weights table (magic, ptr, magic).
     * The actual weights pointer is at index 1. Extract it to pass to create_and_init.
     */
    ai_handle *wt_tbl = (ai_handle*)ai_network_data_weights_get();
    if (wt_tbl && wt_tbl[1] != AI_HANDLE_NULL) {
        ai_weights_ptr[0] = wt_tbl[1];
    } else {
        ai_weights_ptr[0] = AI_HANDLE_NULL;
    }

    err = ai_network_create_and_init(&g_net, (const ai_handle*)ai_activations_ptr, (const ai_handle*)ai_weights_ptr);
    if (err.type != AI_ERROR_NONE || g_net == AI_HANDLE_NULL) {
        return false;
    }

    /* Diagnostic: confirm weights pointer and the first bytes (hex) */
    if (ai_weights_ptr[0] == AI_HANDLE_NULL) {
        ai_uart_print("# ai_weights_ptr=NULL\r\n");
    } else {
        ai_uart_print("# ai_weights_ptr OK hex: ");
        ai_hex_print((const void*)ai_weights_ptr[0], 16);
    }

    /* Also check initial error state */
    {
        ai_error aerr = ai_network_get_error(g_net);
        if (aerr.type != AI_ERROR_NONE) {
            char eb[64];
            snprintf(eb, sizeof(eb), "# nn_init_err=%u,%u\r\n", (unsigned)aerr.type, (unsigned)aerr.code);
            ai_uart_print(eb);
        }
    }

    return true;
}

int AiInference_RunFromArray(const float in[AI_IN_SIZE])
{
    if (g_net == AI_HANDLE_NULL) return -1;
    if (in == NULL) return -1;

    /* copy and apply StandardScaler: (x - mean) / scale */
    for (int i = 0; i < AI_IN_SIZE; ++i) {
        ai_input[i] = (in[i] - scaler_mean[i]) / scaler_scale[i];
    }

    /* Debug: print raw inputs and scaled inputs (use ftoa_simple to avoid %f issues) */
    {
        char dbuf[160];
        char fraw[6][24];
        char fscaled[6][24];
        for (int i = 0; i < 6; ++i) {
            ftoa_simple(fraw[i], sizeof(fraw[i]), in[i], 3);
            ftoa_simple(fscaled[i], sizeof(fscaled[i]), ai_input[i], 6);
        }
        snprintf(dbuf, sizeof(dbuf), "# raw=%s,%s,%s,%s,%s,%s\r\n",
                 fraw[0], fraw[1], fraw[2], fraw[3], fraw[4], fraw[5]);
        ai_uart_print(dbuf);

        snprintf(dbuf, sizeof(dbuf), "# scaled=%s,%s,%s,%s,%s,%s\r\n",
                 fscaled[0], fscaled[1], fscaled[2], fscaled[3], fscaled[4], fscaled[5]);
        ai_uart_print(dbuf);
    }

    ai_buffer *input  = ai_network_inputs_get(g_net, NULL);
    ai_buffer *output = ai_network_outputs_get(g_net, NULL);

    input[0].data  = AI_HANDLE_PTR(ai_input);
    output[0].data = AI_HANDLE_PTR(ai_output);

    ai_i32 batch = ai_network_run(g_net, input, output);
    if (batch <= 0) {
        ai_error aerr = ai_network_get_error(g_net);
        char eb[64];
        snprintf(eb, sizeof(eb), "# nn_run_err=%u,%u\r\n", (unsigned)aerr.type, (unsigned)aerr.code);
        ai_uart_print(eb);
        return -1;
    }
    /* Debug: print raw model outputs (format without %f) */
    {
        char obuf[80];
        char fout0[24], fout1[24], fout2[24];
        ftoa_simple(fout0, sizeof(fout0), ai_output[0], 6);
        ftoa_simple(fout1, sizeof(fout1), ai_output[1], 6);
        ftoa_simple(fout2, sizeof(fout2), ai_output[2], 6);
        snprintf(obuf, sizeof(obuf), "# out=%s,%s,%s\r\n", fout0, fout1, fout2);
        ai_uart_print(obuf);
    }

    /* If outputs contain NaN, dump diagnostics */
    if (isnan(ai_output[0]) || isnan(ai_output[1]) || isnan(ai_output[2])) {
        ai_uart_print("# nn_out_nan detected\r\n");
        /* print input bytes and first weights */
        ai_uart_print("# input hex: ");
        ai_hex_print((const void*)ai_input, sizeof(ai_input));
        if (ai_weights_ptr[0] != AI_HANDLE_NULL) {
            ai_uart_print("# weights hex: ");
            ai_hex_print((const void*)ai_weights_ptr[0], 32);
        }
        /* print any runtime error code */
        ai_error aerr = ai_network_get_error(g_net);
        char eb2[64];
        snprintf(eb2, sizeof(eb2), "# nn_err_after_run=%u,%u\r\n", (unsigned)aerr.type, (unsigned)aerr.code);
        ai_uart_print(eb2);
        return -2;
    }

    /* argmax */
    int pred = 0;
    float maxv = ai_output[0];
    for (int i = 1; i < AI_OUT_SIZE; ++i) {
        if (ai_output[i] > maxv) { maxv = ai_output[i]; pred = i; }
    }

    const char *names[3] = {"stationary", "low_vibration", "high_vibration"};
    char out[128];
    char confidence[24];
    snprintf(out, sizeof(out), "# nn_pred=%s\r\n", names[pred]);
    ai_uart_print(out);

    /* Machine-readable event consumed by the dashboard over Web Serial. */
    ftoa_simple(confidence, sizeof(confidence), maxv, 4);
    snprintf(out, sizeof(out),
             "{\"class\":\"%s\",\"confidence\":%s}\r\n",
             names[pred], confidence);
    ai_uart_print(out);

    return pred;
}

void AiInference_Deinit(void)
{
    if (g_net != AI_HANDLE_NULL) {
        ai_network_destroy(g_net);
        g_net = AI_HANDLE_NULL;
    }
}
