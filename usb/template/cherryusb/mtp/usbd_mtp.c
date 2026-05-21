#include "usbd_mtp.h"
#include "mtp_device.h"
#include "usb_descriptor.h"

static void push_mtp_event(uint8_t ep, uint32_t nbytes);

static const uint8_t *mtp_saved_desc = NULL;
static uint16_t mtp_saved_desc_len = 0;
static uint8_t* ep0_xfer_buf = NULL;
static uint16_t ep0_xfer_len = 0;

typedef struct {
    uint8_t ep_addr;
    uint32_t xferred_bytes;
} mtp_xfer_evt_t;

#define MTP_EVT_Q_DEPTH 8
static mtp_xfer_evt_t evt_queue[MTP_EVT_Q_DEPTH];
static volatile uint8_t evt_head = 0;
static volatile uint8_t evt_tail = 0;

static void mtp_bulk_out_cb(uint8_t busid, uint8_t ep, uint32_t nbytes) { 
    (void)busid;
    USB_LOG_WRN("-> [MTP ISR] Bulk OUT (Host to Dev), ep: %02x, len: %lu\r\n", ep, (unsigned long)nbytes);
    push_mtp_event(ep, nbytes); 
}
static void mtp_bulk_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes) { 
    (void)busid;
    USB_LOG_WRN("<- [MTP ISR] Bulk IN (Dev to Host), ep: %02x, len: %lu\r\n", ep, (unsigned long)nbytes);
    push_mtp_event(ep, nbytes); 
}
static void mtp_int_in_cb(uint8_t busid, uint8_t ep, uint32_t nbytes) { 
    (void)busid;
    push_mtp_event(ep, nbytes); 
}

static void push_mtp_event(uint8_t ep, uint32_t nbytes) {
    uint8_t next_head = (evt_head + 1) % MTP_EVT_Q_DEPTH;
    if (next_head == evt_tail) {
        USB_LOG_ERR("[MTP] event queue full, drop ep: %02x, len: %lu\r\n", ep, (unsigned long)nbytes);
        return;
    }

    evt_queue[evt_head].ep_addr = ep;
    evt_queue[evt_head].xferred_bytes = nbytes;
    evt_head = next_head;
}

void usbd_mtp_task(void) {
    while (evt_tail != evt_head) {
        uint8_t ep = evt_queue[evt_tail].ep_addr;
        uint32_t bytes = evt_queue[evt_tail].xferred_bytes;
        evt_tail = (evt_tail + 1) % MTP_EVT_Q_DEPTH;
        
        USB_LOG_WRN("   [MTP Task] Process evt, ep: %02x, bytes: %lu\r\n", ep, (unsigned long)bytes);
        mtpd_xfer_cb(0, ep, XFER_RESULT_SUCCESS, bytes);
    }
}

// ====== 底层转发与日志 ======
bool tud_control_xfer(uint8_t rhport, tusb_control_request_t const * request, void* buffer, uint16_t len) {
    (void)rhport;
    (void)request;
    ep0_xfer_buf = (uint8_t*)buffer;
    ep0_xfer_len = len;
    return true; 
}

bool usbd_edpt_xfer(uint8_t rhport, uint8_t ep_addr, uint8_t *buffer, uint16_t total_bytes, bool is_isr) {
    (void)is_isr;
    int ret;

    USB_LOG_WRN("== [TinyUSB Req] Xfer ep: %02x, len: %u, buf: %p\r\n", ep_addr, (unsigned)total_bytes, buffer);
    if (ep_addr & 0x80) { ret = usbd_ep_start_write(rhport, ep_addr, buffer, total_bytes); }
    else                { ret = usbd_ep_start_read(rhport, ep_addr, buffer, total_bytes);  }
    if (ret != 0) {
        USB_LOG_ERR("[MTP] xfer failed, ep: %02x, len: %u, ret: %d\r\n", ep_addr, (unsigned)total_bytes, ret);
        return false;
    }
    return true;
}

bool usbd_edpt_stalled(uint8_t rhport, uint8_t ep_addr) { (void)rhport; (void)ep_addr; return false; }
bool usbd_edpt_claim(uint8_t rhport, uint8_t ep_addr) { (void)rhport; (void)ep_addr; return true; }
bool usbd_edpt_release(uint8_t rhport, uint8_t ep_addr) { (void)rhport; (void)ep_addr; return true; }
void usbd_edpt_stall(uint8_t rhport, uint8_t ep_addr) { usbd_ep_set_stall(rhport, ep_addr); }
void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr) { usbd_ep_clear_stall(rhport, ep_addr); }
bool usbd_edpt_open(uint8_t rhport, tusb_desc_endpoint_t const * ep_desc) { (void)rhport; (void)ep_desc; return true; }
tusb_speed_t tud_speed_get(void) { return (MTP_DATA_EPSIZE > 64) ? TUSB_SPEED_HIGH : TUSB_SPEED_FULL; }

struct usbd_endpoint mtp_epout = { .ep_cb = mtp_bulk_out_cb };
struct usbd_endpoint mtp_epin  = { .ep_cb = mtp_bulk_in_cb };
struct usbd_endpoint mtp_epint = { .ep_cb = mtp_int_in_cb };

bool usbd_open_edpt_pair(uint8_t rhport, uint8_t const* p_desc, uint8_t count, uint8_t type, uint8_t* ep_out, uint8_t* ep_in) {
    (void)rhport;
    (void)p_desc;
    (void)count;
    (void)type;
    *ep_out = mtp_epout.ep_addr;
    *ep_in  = mtp_epin.ep_addr;
    return true;
}

static int mtp_class_interface_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len) {
    // 拦截 MS OS 1.0 兼容 ID 请求
    if (setup->bmRequestType == 0xC0 && setup->bRequest == 0x20) {
        // 由于你在别处拦截了，这里可以根据你的实际情况决定是否需要
        return -1; 
    }
    tusb_control_request_t tusb_req;
    memcpy(&tusb_req, setup, 8);
    ep0_xfer_buf = NULL;
    ep0_xfer_len = 0;
    if (!mtpd_control_xfer_cb(busid, CONTROL_STAGE_SETUP, &tusb_req)) return -1;
    if (ep0_xfer_buf) { *data = ep0_xfer_buf; *len = ep0_xfer_len; }
    mtpd_control_xfer_cb(busid, CONTROL_STAGE_ACK, &tusb_req);
    return 0;
}

static void mtp_notify_handler(uint8_t busid, uint8_t event, void *arg) {
    (void)arg;

    switch (event) {
        case USBD_EVENT_RESET:
            evt_head = evt_tail = 0;
            ep0_xfer_buf = NULL;
            ep0_xfer_len = 0;
            mtpd_reset(busid);
            break;
        case USBD_EVENT_CONFIGURED:
            if (mtp_saved_desc) {
                uint16_t res = mtpd_open(busid, (tusb_desc_interface_t const *)mtp_saved_desc, mtp_saved_desc_len);
                if (res == 0) {
                    USB_LOG_ERR("[MTP] mtpd_open failed, intf: %u, eps: %02x/%02x/%02x, desc_len: %u\r\n",
                                mtp_saved_desc[2], mtp_epint.ep_addr, mtp_epin.ep_addr, mtp_epout.ep_addr, (unsigned)mtp_saved_desc_len);
                } else {
                    USB_LOG_INFO("[MTP] mtpd_open ok, intf: %u, eps: %02x/%02x/%02x\r\n",
                                 mtp_saved_desc[2], mtp_epint.ep_addr, mtp_epin.ep_addr, mtp_epout.ep_addr);
                }
            } else {
                USB_LOG_ERR("[MTP] configured without saved descriptor\r\n");
            }
            break;
        default: break;
    }
}

struct usbd_interface *usbd_mtp_init_intf(uint8_t busid, struct usbd_interface *intf, const uint8_t *desc, uint32_t desc_len, uint8_t ep_out, uint8_t ep_in, uint8_t ep_int) {
    intf->intf_num = desc[2];
    intf->class_interface_handler = mtp_class_interface_request_handler;
    intf->vendor_handler = mtp_class_interface_request_handler; 
    intf->notify_handler = mtp_notify_handler;

    mtp_saved_desc = desc;
    mtp_saved_desc_len = desc_len;
    mtpd_init();

    mtp_epout.ep_addr = ep_out;
    mtp_epin.ep_addr  = ep_in;
    mtp_epint.ep_addr = ep_int;

    usbd_add_endpoint(busid, &mtp_epout);
    usbd_add_endpoint(busid, &mtp_epin);
    usbd_add_endpoint(busid, &mtp_epint);

    return intf;
}
