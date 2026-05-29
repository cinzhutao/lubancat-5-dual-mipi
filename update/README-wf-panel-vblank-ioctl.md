# WF panel vblank ioctl usage

This update keeps the userspace ABI on `/dev/wf_panel_dsi1` and routes each
ioctl DSI command through a synchronized panel request.

## Kernel flow

The implemented path is:

1. userspace calls ioctl on `/dev/wf_panel_dsi1`
2. `panel-simple.c` locks `panel->ioctl_lock`
3. the ioctl request is saved in `panel->ioctl_req`
4. the driver copies the userspace payload into that request
5. the driver finds the active connector CRTC and calls `drm_crtc_vblank_get()`
6. if no active CRTC/vblank is available, ioctl fails with `EAGAIN`
7. if vblank is available, the driver schedules `drm_vblank_work` for the next
   vblank
8. the vblank work only records the vblank time and queues high-priority
   workqueue work; it does not call `mipi_dsi_*`
9. if `drm_vblank_work_schedule()` fails after vblank was acquired, the driver
   queues high-priority work that waits one CRTC vblank first
10. the workqueue worker checks that it is still inside the configured vblank
    window
11. the worker locks `panel->cmd_lock`, prepares DSI mode flags, and executes
    one MIPI DSI transfer
12. the worker restores DSI mode flags and stores `ret`, read length, and read
    bytes in `panel->ioctl_req`
13. the worker calls `drm_crtc_vblank_put()` when a vblank reference was held
14. the worker calls `complete()`
15. ioctl waits with `wait_for_completion_timeout()`
16. read ioctls copy the completed result back with `copy_to_user()`
17. ioctl unlocks `panel->ioctl_lock` and returns

The request storage is in the panel private structure, not on the ioctl stack.
`panel->ioctl_lock` serializes raw ioctls, so there is only one active raw panel
request at a time. `panel->cmd_lock` is taken in workqueue context around the
actual DSI operation. The vblank callback path never directly calls sleeping
DSI helpers such as `mipi_dsi_dcs_read()`.

## Updated files

Copy these files into the matching kernel tree paths:

```sh
cp update/drivers/gpu/drm/panel/panel-simple.c \
  ubuntu-ebf-rk3588/kernel-6.1/drivers/gpu/drm/panel/panel-simple.c
cp update/drivers/gpu/drm/panel/wf_panel_ioctl_uapi.h \
  ubuntu-ebf-rk3588/kernel-6.1/drivers/gpu/drm/panel/wf_panel_ioctl_uapi.h
cp update/drivers/gpu/drm/rockchip/dw-mipi-dsi2-rockchip.c \
  ubuntu-ebf-rk3588/kernel-6.1/drivers/gpu/drm/rockchip/dw-mipi-dsi2-rockchip.c
```

The UAPI header is:

```c
drivers/gpu/drm/panel/wf_panel_ioctl_uapi.h
```

Keep the userspace copy of this header byte-for-byte compatible with the
kernel copy.

## Userspace basics

This directory includes a small userspace test tool:

```sh
gcc -Wall -Wextra -std=gnu99 -Iupdate \
  -o wf_panel_ioctl_tool update/wf_panel_ioctl_tool.c
```

Example commands:

```sh
./wf_panel_ioctl_tool dcs-write 0x51 0x80
./wf_panel_ioctl_tool dcs-read 0x0a 1
./wf_panel_ioctl_tool generic-write 0x80 0xac
./wf_panel_ioctl_tool generic-read 1 0x80
./wf_panel_ioctl_tool raw 0x06 1 1 0x0a
./wf_panel_ioctl_tool raw 0x14 1 1 0x85
./wf_panel_ioctl_tool get-brightness
./wf_panel_ioctl_tool set-brightness 0x80
```

The tool retries `EAGAIN` a few times by default because the kernel refuses to
run a transfer when it misses the vblank window.

Open the misc device:

```c
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "wf_panel_ioctl_uapi.h"

int fd = open("/dev/wf_panel_dsi1", O_RDWR | O_CLOEXEC);
if (fd < 0) {
	perror("open /dev/wf_panel_dsi1");
	return 1;
}
```

All transfer payloads use:

```c
struct wf_panel_ioctl_xfer {
	__u32 tx_len;
	__u32 rx_len;
	__u8 data[256];
};
```

`tx_len` is the number of request bytes in `data[]`. `rx_len` is the requested
maximum receive length before a read ioctl. On successful read return, the
kernel updates `rx_len` to the actual number of bytes copied into `data[]`.

The ioctl return value is `0` on success. On failure it returns `-1` and sets
`errno`.

## DCS write

Put the DCS command byte first, followed by optional payload bytes.

```c
struct wf_panel_ioctl_xfer xfer;

memset(&xfer, 0, sizeof(xfer));
xfer.tx_len = 2;
xfer.data[0] = 0x51; /* DCS set display brightness */
xfer.data[1] = 0x80;

if (ioctl(fd, WF_PANEL_IOC_DCS_WRITE, &xfer) < 0)
	perror("WF_PANEL_IOC_DCS_WRITE");
```

## DCS read

Set `tx_len = 1`, put the DCS register in `data[0]`, and set the maximum
expected receive length in `rx_len`.

```c
struct wf_panel_ioctl_xfer xfer;

memset(&xfer, 0, sizeof(xfer));
xfer.tx_len = 1;
xfer.rx_len = 1;
xfer.data[0] = 0x0a; /* DCS get power mode */

if (ioctl(fd, WF_PANEL_IOC_DCS_READ, &xfer) < 0) {
	perror("WF_PANEL_IOC_DCS_READ");
} else {
	printf("rx_len=%u\n", xfer.rx_len);
	for (uint32_t i = 0; i < xfer.rx_len; i++)
		printf("data[%u]=0x%02x\n", i, xfer.data[i]);
}
```

## Generic write

Generic write sends the bytes in `data[0..tx_len-1]`.

```c
struct wf_panel_ioctl_xfer xfer;

memset(&xfer, 0, sizeof(xfer));
xfer.tx_len = 2;
xfer.data[0] = 0x80;
xfer.data[1] = 0xac;

if (ioctl(fd, WF_PANEL_IOC_GENERIC_WRITE, &xfer) < 0)
	perror("WF_PANEL_IOC_GENERIC_WRITE");
```

## Generic read

Generic read supports 0, 1, or 2 request bytes. This matches the Linux MIPI DSI
generic read helpers.

```c
struct wf_panel_ioctl_xfer xfer;

memset(&xfer, 0, sizeof(xfer));
xfer.tx_len = 1;
xfer.rx_len = 1;
xfer.data[0] = 0x80;

if (ioctl(fd, WF_PANEL_IOC_GENERIC_READ, &xfer) < 0) {
	perror("WF_PANEL_IOC_GENERIC_READ");
} else {
	printf("rx_len=%u\n", xfer.rx_len);
	for (uint32_t i = 0; i < xfer.rx_len; i++)
		printf("data[%u]=0x%02x\n", i, xfer.data[i]);
}
```

## Raw transfer

Raw transfer lets userspace specify the MIPI DSI packet data type directly.
The kernel still saves the request and schedules the transfer through the same
vblank/workqueue path; it does not run the transfer at ioctl call time.

Command format:

```sh
./wf_panel_ioctl_tool raw <type> <tx_len> <rx_len> [tx bytes...]
```

Examples:

```sh
# DCS read: type 0x06, one request byte, one response byte.
./wf_panel_ioctl_tool raw 0x06 1 1 0x0a

# Generic read, one parameter: type 0x14.
./wf_panel_ioctl_tool raw 0x14 1 1 0x85

# Generic read, two parameters: type 0x24.
./wf_panel_ioctl_tool raw 0x24 2 2 0x85 0x00

# DCS long write: type 0x39.
./wf_panel_ioctl_tool raw 0x39 2 0 0x51 0x80
```

Raw transfer removes the DCS/generic helper shape checks. The remaining limits
are only ABI buffer bounds: `tx_len <= 256` and `rx_len <= 256`. For raw reads,
the host logs the raw RX header and returns the payload it receives.

## Brightness helpers

```c
struct wf_panel_ioctl_brightness b = {
	.brightness = 0x80,
};

if (ioctl(fd, WF_PANEL_IOC_SET_BRIGHTNESS, &b) < 0)
	perror("WF_PANEL_IOC_SET_BRIGHTNESS");

memset(&b, 0, sizeof(b));
if (ioctl(fd, WF_PANEL_IOC_GET_BRIGHTNESS, &b) < 0)
	perror("WF_PANEL_IOC_GET_BRIGHTNESS");
else
	printf("brightness=0x%04x\n", b.brightness);
```

## Panel command ioctls

These ioctls take no argument:

```c
if (ioctl(fd, WF_PANEL_IOC_DISPLAY_ON) < 0)
	perror("WF_PANEL_IOC_DISPLAY_ON");
if (ioctl(fd, WF_PANEL_IOC_DISPLAY_OFF) < 0)
	perror("WF_PANEL_IOC_DISPLAY_OFF");
if (ioctl(fd, WF_PANEL_IOC_SLEEP_OUT) < 0)
	perror("WF_PANEL_IOC_SLEEP_OUT");
if (ioctl(fd, WF_PANEL_IOC_SLEEP_IN) < 0)
	perror("WF_PANEL_IOC_SLEEP_IN");
```

## Runtime checks

On the board:

```sh
ls -l /dev/wf_panel_dsi1
dmesg | grep -E 'wf_panel|vblank-read|vblank-write|vblank-get-brightness|video tx cfg|CRI has no available'
```

Expected behavior:

- ioctl blocks until the kernel work finishes or times out.
- when an active CRTC exists, vblank work schedules workqueue work after the
  next DRM CRTC vblank
- when no active CRTC/vblank exists, ioctl returns `EAGAIN`; the driver does
  not run a DSI transfer at the userspace call time
- if `drm_vblank_work_schedule()` fails after a vblank reference was acquired,
  fallback work waits one CRTC vblank before checking the window and running
  the DSI transfer
- DSI helpers run only from workqueue context under `panel->cmd_lock`
- each ioctl issues one DSI transfer, with no panel-layer retry loop
- read ioctls update `rx_len` and copy returned bytes into `data[]`

Common errors:

- `EHOSTDOWN`: panel is not prepared
- `ENODEV`: no DSI device
- `EINVAL`: invalid payload length or invalid read request shape
- `EAGAIN`: no active CRTC/vblank is available, so the driver refused to run a
  transfer at userspace call time
- `ETIMEDOUT`: kernel work did not finish before the ioctl timeout
- host log `CRI has no available read data`: DSI host did not receive read data

Read stability still depends on the Rockchip DSI host BTA/RX behavior and on
whether the panel supports readback while running in video mode.
