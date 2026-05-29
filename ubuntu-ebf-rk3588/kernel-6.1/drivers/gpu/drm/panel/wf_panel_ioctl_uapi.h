#ifndef __WF_PANEL_IOCTL_UAPI_H__
#define __WF_PANEL_IOCTL_UAPI_H__

#include <linux/ioctl.h>
#include <linux/types.h>

#define WF_PANEL_IOCTL_NAME "wf_panel_dsi1"
#define WF_PANEL_IOCTL_MAX_PAYLOAD 256
#define WF_PANEL_IOCTL_MAX_BATCH 16

struct wf_panel_ioctl_xfer {
	__u32 tx_len;
	__u32 rx_len;
	__u8 data[WF_PANEL_IOCTL_MAX_PAYLOAD];
};

struct wf_panel_ioctl_brightness {
	__u16 brightness;
	__u16 reserved;
};

struct wf_panel_ioctl_raw_transfer {
	__u8 type;
	__u8 channel;
	__u16 flags;
	__u32 tx_len;
	__u32 rx_len;
	__u8 data[WF_PANEL_IOCTL_MAX_PAYLOAD];
};

struct wf_panel_ioctl_raw_batch {
	__u32 count;
	__s32 ret;
	struct wf_panel_ioctl_raw_transfer xfers[WF_PANEL_IOCTL_MAX_BATCH];
};

struct wf_panel_ioctl_lpm_info {
	__u8 type;
	__u8 channel;
	__u16 msg_flags;
	__u32 tx_len;
	__u32 rx_len;
	__u32 mode_flags;
	__u8 use_lpm;
	__u8 cmd_tx_mode;
	__u8 lpdt_display_cmd_en;
	__u8 force_lpm;
	__s32 ret;
};

struct wf_panel_ioctl_force_lpm {
	__u8 force_lpm;
	__u8 reserved[3];
};

#define WF_PANEL_IOC_MAGIC 'W'

#define WF_PANEL_IOC_DCS_WRITE \
	_IOW(WF_PANEL_IOC_MAGIC, 0x00, struct wf_panel_ioctl_xfer)
#define WF_PANEL_IOC_DCS_READ \
	_IOWR(WF_PANEL_IOC_MAGIC, 0x01, struct wf_panel_ioctl_xfer)
#define WF_PANEL_IOC_GENERIC_WRITE \
	_IOW(WF_PANEL_IOC_MAGIC, 0x02, struct wf_panel_ioctl_xfer)
#define WF_PANEL_IOC_GENERIC_READ \
	_IOWR(WF_PANEL_IOC_MAGIC, 0x03, struct wf_panel_ioctl_xfer)
#define WF_PANEL_IOC_SET_BRIGHTNESS \
	_IOW(WF_PANEL_IOC_MAGIC, 0x04, struct wf_panel_ioctl_brightness)
#define WF_PANEL_IOC_GET_BRIGHTNESS \
	_IOR(WF_PANEL_IOC_MAGIC, 0x05, struct wf_panel_ioctl_brightness)
#define WF_PANEL_IOC_DISPLAY_ON \
	_IO(WF_PANEL_IOC_MAGIC, 0x06)
#define WF_PANEL_IOC_DISPLAY_OFF \
	_IO(WF_PANEL_IOC_MAGIC, 0x07)
#define WF_PANEL_IOC_SLEEP_OUT \
	_IO(WF_PANEL_IOC_MAGIC, 0x08)
#define WF_PANEL_IOC_SLEEP_IN \
	_IO(WF_PANEL_IOC_MAGIC, 0x09)
#define WF_PANEL_IOC_RAW_TRANSFER \
	_IOWR(WF_PANEL_IOC_MAGIC, 0x0a, struct wf_panel_ioctl_raw_transfer)
#define WF_PANEL_IOC_GET_LPM_INFO \
	_IOR(WF_PANEL_IOC_MAGIC, 0x0b, struct wf_panel_ioctl_lpm_info)
#define WF_PANEL_IOC_RAW_BATCH \
	_IOWR(WF_PANEL_IOC_MAGIC, 0x0c, struct wf_panel_ioctl_raw_batch)
#define WF_PANEL_IOC_SET_FORCE_LPM \
	_IOW(WF_PANEL_IOC_MAGIC, 0x0d, struct wf_panel_ioctl_force_lpm)
#define WF_PANEL_IOC_GET_FORCE_LPM \
	_IOR(WF_PANEL_IOC_MAGIC, 0x0e, struct wf_panel_ioctl_force_lpm)

#endif
