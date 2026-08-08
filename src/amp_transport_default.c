/*
 * Default AMP transport adapter for the existing 64-byte Raw HID endpoint.
 * Projects using USB Bulk, UART, BLE or TCP can provide a strong replacement
 * for these weak functions without changing the AMP message/service layers.
 */
#include "amp_protocol.h"

#include "driver.h"
#include "string.h"

__WEAK void amp_transport_prepare_session(void)
{
}

__WEAK int amp_transport_send(const uint8_t *message, uint16_t message_len)
{
    uint8_t report[AMP_FRAME_REPORT_SIZE] = {0};
    if (message == NULL || message_len > sizeof(report))
    {
        return 1;
    }
    memcpy(report, message, message_len);
    return hid_send_raw(report, sizeof(report));
}

__WEAK uint16_t amp_transport_max_payload(void)
{
    return AMP_FRAME_REPORT_SIZE - AMP_FRAME_HEADER_SIZE;
}
