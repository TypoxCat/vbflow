#include "ring_buffer.h"
#include "main.h"

void AccelRingBuffer_Init(
    AccelRingBuffer_t *buffer
)
{
    if (buffer == NULL)
    {
        return;
    }

    buffer->write_index = 0U;
    buffer->read_index = 0U;
    buffer->overflow_count = 0U;
}

bool AccelRingBuffer_Push(
    AccelRingBuffer_t *buffer,
    const AccelSample_t *sample
)
{
    uint16_t current_write;
    uint16_t next_write;

    if ((buffer == NULL) ||
        (sample == NULL))
    {
        return false;
    }

    current_write =
        buffer->write_index;

    next_write =
        (uint16_t)(
            (current_write + 1U) &
            ACCEL_RING_BUFFER_MASK
        );

    /*
     * Jika write berikutnya bertemu read,
     * buffer sudah penuh.
     */
    if (next_write ==
        buffer->read_index)
    {
        buffer->overflow_count++;
        return false;
    }

    buffer->samples[current_write] =
        *sample;

    /*
     * Pastikan data sampel sudah ditulis
     * sebelum write_index dipublikasikan.
     */
    __DMB();

    buffer->write_index =
        next_write;

    return true;
}

bool AccelRingBuffer_Pop(
    AccelRingBuffer_t *buffer,
    AccelSample_t *sample
)
{
    uint16_t current_read;

    if ((buffer == NULL) ||
        (sample == NULL))
    {
        return false;
    }

    current_read =
        buffer->read_index;

    /*
     * write == read berarti kosong.
     */
    if (current_read ==
        buffer->write_index)
    {
        return false;
    }

    __DMB();

    *sample =
        buffer->samples[current_read];

    buffer->read_index =
        (uint16_t)(
            (current_read + 1U) &
            ACCEL_RING_BUFFER_MASK
        );

    return true;
}

uint16_t AccelRingBuffer_Count(
    const AccelRingBuffer_t *buffer
)
{
    uint16_t write_index;
    uint16_t read_index;

    if (buffer == NULL)
    {
        return 0U;
    }

    write_index =
        buffer->write_index;

    read_index =
        buffer->read_index;

    return (uint16_t)(
        (write_index - read_index) &
        ACCEL_RING_BUFFER_MASK
    );
}

uint32_t AccelRingBuffer_GetOverflowCount(
    const AccelRingBuffer_t *buffer
)
{
    if (buffer == NULL)
    {
        return 0U;
    }

    return buffer->overflow_count;
}
