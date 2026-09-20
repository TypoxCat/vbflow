#ifndef APPLICATION_RING_BUFFER_H_
#define APPLICATION_RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Kapasitas harus pangkat dua:
 * 256, 512, 1024, dan seterusnya.
 */
#define ACCEL_RING_BUFFER_CAPACITY  512U

#define ACCEL_RING_BUFFER_MASK      \
    (ACCEL_RING_BUFFER_CAPACITY - 1U)

/*
 * Tambahkan _Static_assert di sini.
 * Letaknya di luar fungsi dan di luar struct.
 */
_Static_assert(
    (ACCEL_RING_BUFFER_CAPACITY &
     (ACCEL_RING_BUFFER_CAPACITY - 1U)) == 0U,
    "Ring buffer capacity harus pangkat dua"
);

typedef struct
{
    int32_t x;
    int32_t y;
    int32_t z;

    uint32_t timestamp_ms;

} AccelSample_t;

typedef struct
{
    AccelSample_t samples[
        ACCEL_RING_BUFFER_CAPACITY
    ];

    volatile uint16_t write_index;
    volatile uint16_t read_index;
    volatile uint32_t overflow_count;

} AccelRingBuffer_t;

/* Prototype fungsi ring buffer di bawah sini. */

void AccelRingBuffer_Init(
    AccelRingBuffer_t *buffer
);

bool AccelRingBuffer_Push(
    AccelRingBuffer_t *buffer,
    const AccelSample_t *sample
);

bool AccelRingBuffer_Pop(
    AccelRingBuffer_t *buffer,
    AccelSample_t *sample
);

uint16_t AccelRingBuffer_Count(
    const AccelRingBuffer_t *buffer
);

uint32_t AccelRingBuffer_GetOverflowCount(
    const AccelRingBuffer_t *buffer
);

#endif /* APPLICATION_RING_BUFFER_H_ */
