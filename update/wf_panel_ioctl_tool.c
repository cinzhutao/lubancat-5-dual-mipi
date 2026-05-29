#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

#include "drivers/gpu/drm/panel/wf_panel_ioctl_uapi.h"

#define DEFAULT_DEVICE "/dev/" WF_PANEL_IOCTL_NAME
#define DEFAULT_RETRIES 5
#define RETRY_DELAY_US 1000

static void usage(const char *prog)
{
	fprintf(stderr,
		"Usage:\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] dcs-write <byte> [byte...]\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] dcs-read <cmd> <rx_len>\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] generic-write <byte> [byte...]\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] generic-read <rx_len> [byte0 [byte1]]\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] raw <type> <tx_len> <rx_len> [byte...]\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] raw-ex <type> <channel> <flags> <tx_len> <rx_len> [byte...]\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] raw-batch <raw args> [-- <raw args>]...\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] set-brightness <value>\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] get-brightness\n"
		"  %s [-d /dev/wf_panel_dsi1] lpm-info\n"
		"  %s [-d /dev/wf_panel_dsi1] set-force-lpm <0|1>\n"
		"  %s [-d /dev/wf_panel_dsi1] get-force-lpm\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] csv <mode> <op> <args...>\n"
		"  %s [-d /dev/wf_panel_dsi1] [-r retries] display-on|display-off|sleep-out|sleep-in\n"
		"\n"
		"CSV ops: generic-write <reg> <value>, generic-read <rx_len> <reg>, raw-ex <type> <channel> <flags> <tx_len> <rx_len> [byte...]\n"
		"Bytes may be decimal or hex, for example 0x0a or 10.\n"
		"Use -r 0 for failure statistics so EAGAIN is not hidden by retries.\n",
		prog, prog, prog, prog, prog, prog, prog, prog, prog, prog, prog,
		prog, prog, prog);
}

static int parse_u32(const char *s, uint32_t min, uint32_t max, uint32_t *out)
{
	char *end;
	unsigned long val;

	errno = 0;
	val = strtoul(s, &end, 0);
	if (errno || !s[0] || *end || val < min || val > max)
		return -1;

	*out = (uint32_t)val;
	return 0;
}

static int fill_bytes(struct wf_panel_ioctl_xfer *xfer, int argc, char **argv)
{
	uint32_t val;
	int i;

	if (argc <= 0 || argc > WF_PANEL_IOCTL_MAX_PAYLOAD)
		return -1;

	memset(xfer, 0, sizeof(*xfer));
	xfer->tx_len = argc;

	for (i = 0; i < argc; i++) {
		if (parse_u32(argv[i], 0, 0xff, &val))
			return -1;
		xfer->data[i] = val;
	}

	return 0;
}

static int ioctl_retry(int fd, unsigned long request, void *arg, int retries)
{
	int attempt;
	int ret;

	for (attempt = 0; attempt <= retries; attempt++) {
		ret = ioctl(fd, request, arg);
		if (!ret)
			return 0;

		if (errno != EAGAIN || attempt == retries)
			return -1;

		usleep(RETRY_DELAY_US);
	}

	return -1;
}

static int ioctl_noarg_retry(int fd, unsigned long request, int retries)
{
	int attempt;
	int ret;

	for (attempt = 0; attempt <= retries; attempt++) {
		ret = ioctl(fd, request);
		if (!ret)
			return 0;

		if (errno != EAGAIN || attempt == retries)
			return -1;

		usleep(RETRY_DELAY_US);
	}

	return -1;
}

static void print_rx(const struct wf_panel_ioctl_xfer *xfer)
{
	uint32_t i;

	printf("rx_len=%u\n", xfer->rx_len);
	for (i = 0; i < xfer->rx_len; i++)
		printf("data[%u]=0x%02x\n", i, xfer->data[i]);
}

static void print_raw_rx(const struct wf_panel_ioctl_raw_transfer *raw)
{
	uint32_t i;

	printf("type=0x%02x channel=%u flags=0x%04x\n",
	       raw->type, raw->channel, raw->flags);
	printf("rx_len=%u\n", raw->rx_len);
	for (i = 0; i < raw->rx_len; i++)
		printf("data[%u]=0x%02x\n", i, raw->data[i]);
}

static void print_raw_batch(const struct wf_panel_ioctl_raw_batch *batch)
{
	uint32_t i, j;

	printf("count=%u ret=%d\n", batch->count, batch->ret);
	for (i = 0; i < batch->count; i++) {
		const struct wf_panel_ioctl_raw_transfer *raw = &batch->xfers[i];

		printf("[%u] type=0x%02x channel=%u flags=0x%04x rx_len=%u\n",
		       i, raw->type, raw->channel, raw->flags, raw->rx_len);
		for (j = 0; j < raw->rx_len; j++)
			printf("[%u] data[%u]=0x%02x\n", i, j, raw->data[j]);
	}
}

static void print_lpm_info(const struct wf_panel_ioctl_lpm_info *info)
{
	printf("type=0x%02x channel=%u msg_flags=0x%04x\n",
	       info->type, info->channel, info->msg_flags);
	printf("tx_len=%u rx_len=%u ret=%d\n",
	       info->tx_len, info->rx_len, info->ret);
	printf("mode_flags=0x%08x use_lpm=%u cmd_tx_mode=%u lpdt_display_cmd_en=%u force_lpm=%u\n",
	       info->mode_flags, info->use_lpm, info->cmd_tx_mode,
	       info->lpdt_display_cmd_en, info->force_lpm);
}

static int parse_raw_transfer(struct wf_panel_ioctl_raw_transfer *raw,
			      int argc, char **argv, int extended);

static uint64_t now_us(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

static int ioctl_get_lpm_info(int fd, struct wf_panel_ioctl_lpm_info *info)
{
	memset(info, 0, sizeof(*info));
	return ioctl(fd, WF_PANEL_IOC_GET_LPM_INFO, info);
}

static void print_csv_header(void)
{
	printf("mode,op,reg,value,ret,elapsed_us,readback,dmesg_use_lpm,dmesg_cmd_tx_mode,dmesg_lpdt_display_cmd_en\n");
}

static void print_csv_row(const char *mode, const char *op,
			  const char *reg, const char *value, int ret,
			  uint64_t elapsed_us, const char *readback,
			  const struct wf_panel_ioctl_lpm_info *info)
{
	printf("%s,%s,%s,%s,%d,%llu,%s,%u,%u,%u\n",
	       mode, op, reg, value, ret,
	       (unsigned long long)elapsed_us, readback,
	       info->use_lpm, info->cmd_tx_mode, info->lpdt_display_cmd_en);
}

static int run_csv_cmd(int fd, int retries, int argc, char **argv)
{
	const char *mode;
	const char *op;
	const char *reg = "";
	const char *value = "";
	char readback[3 * WF_PANEL_IOCTL_MAX_PAYLOAD + 1] = "";
	struct wf_panel_ioctl_xfer xfer;
	struct wf_panel_ioctl_raw_transfer raw;
	struct wf_panel_ioctl_lpm_info lpm_info;
	uint64_t start_us;
	uint64_t elapsed_us;
	uint32_t val;
	int ret = 0;

	if (argc < 2)
		return -2;

	mode = argv[0];
	op = argv[1];
	argc -= 2;
	argv += 2;

	start_us = now_us();
	if (!strcmp(op, "generic-write")) {
		if (argc != 2 || fill_bytes(&xfer, argc, argv))
			return -2;
		reg = argv[0];
		value = argv[1];
		ret = ioctl_retry(fd, WF_PANEL_IOC_GENERIC_WRITE, &xfer, retries);
	} else if (!strcmp(op, "generic-read")) {
		if (argc != 2 ||
		    parse_u32(argv[0], 1, WF_PANEL_IOCTL_MAX_PAYLOAD, &val))
			return -2;

		memset(&xfer, 0, sizeof(xfer));
		xfer.rx_len = val;
		xfer.tx_len = 1;
		if (parse_u32(argv[1], 0, 0xff, &val))
			return -2;
		xfer.data[0] = val;
		reg = argv[1];
		ret = ioctl_retry(fd, WF_PANEL_IOC_GENERIC_READ, &xfer, retries);
		if (!ret) {
			char *p = readback;
			size_t left = sizeof(readback);

			for (uint32_t i = 0; i < xfer.rx_len && left > 1; i++) {
				int n = snprintf(p, left, "%s%02x",
						 i ? ":" : "", xfer.data[i]);
				if (n < 0 || (size_t)n >= left)
					break;
				p += n;
				left -= n;
			}
		}
	} else if (!strcmp(op, "raw-ex")) {
		if (parse_raw_transfer(&raw, argc, argv, 1))
			return -2;

		reg = argc >= 6 ? argv[5] : "";
		value = raw.tx_len >= 2 ? argv[6] : "";
		ret = ioctl_retry(fd, WF_PANEL_IOC_RAW_TRANSFER, &raw, retries);
		if (!ret && raw.rx_len) {
			char *p = readback;
			size_t left = sizeof(readback);

			for (uint32_t i = 0; i < raw.rx_len && left > 1; i++) {
				int n = snprintf(p, left, "%s%02x",
						 i ? ":" : "", raw.data[i]);
				if (n < 0 || (size_t)n >= left)
					break;
				p += n;
				left -= n;
			}
		}
	} else {
		return -2;
	}
	elapsed_us = now_us() - start_us;

	if (ret)
		ret = -errno;

	if (ioctl_get_lpm_info(fd, &lpm_info))
		memset(&lpm_info, 0, sizeof(lpm_info));

	print_csv_header();
	print_csv_row(mode, op, reg, value, ret, elapsed_us,
		      readback[0] ? readback : "", &lpm_info);

	return ret ? -1 : 0;
}

static int parse_raw_transfer(struct wf_panel_ioctl_raw_transfer *raw,
			      int argc, char **argv, int extended)
{
	uint32_t val;
	int argi = 0;

	if ((!extended && argc < 3) || (extended && argc < 5) ||
	    parse_u32(argv[argi++], 0, 0xff, &val))
		return -1;

	memset(raw, 0, sizeof(*raw));
	raw->type = val;

	if (extended) {
		if (parse_u32(argv[argi++], 0, 3, &val))
			return -1;
		raw->channel = val;

		if (parse_u32(argv[argi++], 0, 0xffff, &val))
			return -1;
		raw->flags = val;
	}

	if (parse_u32(argv[argi++], 0, WF_PANEL_IOCTL_MAX_PAYLOAD, &val))
		return -1;
	raw->tx_len = val;

	if (parse_u32(argv[argi++], 0, WF_PANEL_IOCTL_MAX_PAYLOAD, &val))
		return -1;
	raw->rx_len = val;

	if (argc - argi != (int)raw->tx_len)
		return -1;

	for (uint32_t i = 0; i < raw->tx_len; i++) {
		if (parse_u32(argv[argi + i], 0, 0xff, &val))
			return -1;
		raw->data[i] = val;
	}

	return 0;
}

static int parse_raw_batch(struct wf_panel_ioctl_raw_batch *batch,
			   int argc, char **argv)
{
	int start = 0;

	if (argc <= 0)
		return -1;

	memset(batch, 0, sizeof(*batch));
	while (start < argc) {
		int end = start;

		if (batch->count >= WF_PANEL_IOCTL_MAX_BATCH)
			return -1;

		while (end < argc && strcmp(argv[end], "--"))
			end++;

		if (end == start)
			return -1;

		if (parse_raw_transfer(&batch->xfers[batch->count],
				       end - start, &argv[start], 0))
			return -1;

		batch->count++;
		start = end + 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	const char *dev = DEFAULT_DEVICE;
	const char *cmd;
	int retries = DEFAULT_RETRIES;
	int fd;
	int argi = 1;
	uint32_t val;
	struct wf_panel_ioctl_xfer xfer;
	struct wf_panel_ioctl_brightness brightness;
	struct wf_panel_ioctl_raw_transfer raw;
	struct wf_panel_ioctl_raw_batch batch;
	struct wf_panel_ioctl_lpm_info lpm_info;
	struct wf_panel_ioctl_force_lpm force_lpm;
	unsigned long request;

	while (argi < argc && argv[argi][0] == '-') {
		if (!strcmp(argv[argi], "-d")) {
			if (++argi >= argc) {
				usage(argv[0]);
				return 2;
			}
			dev = argv[argi++];
		} else if (!strcmp(argv[argi], "-r")) {
			if (++argi >= argc ||
			    parse_u32(argv[argi], 0, 1000, &val)) {
				usage(argv[0]);
				return 2;
			}
			retries = val;
			argi++;
		} else if (!strcmp(argv[argi], "-h") ||
			   !strcmp(argv[argi], "--help")) {
			usage(argv[0]);
			return 0;
		} else {
			usage(argv[0]);
			return 2;
		}
	}

	if (argi >= argc) {
		usage(argv[0]);
		return 2;
	}

	cmd = argv[argi++];
	fd = open(dev, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		perror(dev);
		return 1;
	}

	if (!strcmp(cmd, "dcs-write")) {
		if (fill_bytes(&xfer, argc - argi, &argv[argi])) {
			usage(argv[0]);
			return 2;
		}
		if (ioctl_retry(fd, WF_PANEL_IOC_DCS_WRITE, &xfer, retries)) {
			perror("WF_PANEL_IOC_DCS_WRITE");
			return 1;
		}
	} else if (!strcmp(cmd, "dcs-read")) {
		if (argc - argi != 2 ||
		    parse_u32(argv[argi], 0, 0xff, &val)) {
			usage(argv[0]);
			return 2;
		}
		memset(&xfer, 0, sizeof(xfer));
		xfer.tx_len = 1;
		xfer.data[0] = val;
		if (parse_u32(argv[argi + 1], 1, WF_PANEL_IOCTL_MAX_PAYLOAD, &val)) {
			usage(argv[0]);
			return 2;
		}
		xfer.rx_len = val;
		if (ioctl_retry(fd, WF_PANEL_IOC_DCS_READ, &xfer, retries)) {
			perror("WF_PANEL_IOC_DCS_READ");
			return 1;
		}
		print_rx(&xfer);
	} else if (!strcmp(cmd, "generic-write")) {
		if (fill_bytes(&xfer, argc - argi, &argv[argi])) {
			usage(argv[0]);
			return 2;
		}
		if (ioctl_retry(fd, WF_PANEL_IOC_GENERIC_WRITE, &xfer, retries)) {
			perror("WF_PANEL_IOC_GENERIC_WRITE");
			return 1;
		}
	} else if (!strcmp(cmd, "generic-read")) {
		int tx_argc = argc - argi - 1;

		if (argc - argi < 1 || tx_argc < 0 || tx_argc > 2 ||
		    parse_u32(argv[argi], 1, WF_PANEL_IOCTL_MAX_PAYLOAD, &val)) {
			usage(argv[0]);
			return 2;
		}

		memset(&xfer, 0, sizeof(xfer));
		xfer.rx_len = val;
		xfer.tx_len = tx_argc;
		for (int i = 0; i < tx_argc; i++) {
			if (parse_u32(argv[argi + 1 + i], 0, 0xff, &val)) {
				usage(argv[0]);
				return 2;
			}
			xfer.data[i] = val;
		}

		if (ioctl_retry(fd, WF_PANEL_IOC_GENERIC_READ, &xfer, retries)) {
			perror("WF_PANEL_IOC_GENERIC_READ");
			return 1;
		}
		print_rx(&xfer);
	} else if (!strcmp(cmd, "raw") || !strcmp(cmd, "raw-ex")) {
		if (parse_raw_transfer(&raw, argc - argi, &argv[argi],
				       !strcmp(cmd, "raw-ex"))) {
			usage(argv[0]);
			return 2;
		}
		if (ioctl_retry(fd, WF_PANEL_IOC_RAW_TRANSFER, &raw, retries)) {
			perror("WF_PANEL_IOC_RAW_TRANSFER");
			return 1;
		}
		if (raw.rx_len)
			print_raw_rx(&raw);
	} else if (!strcmp(cmd, "raw-batch")) {
		if (parse_raw_batch(&batch, argc - argi, &argv[argi])) {
			usage(argv[0]);
			return 2;
		}
		if (ioctl_retry(fd, WF_PANEL_IOC_RAW_BATCH, &batch, retries)) {
			perror("WF_PANEL_IOC_RAW_BATCH");
			return 1;
		}
		print_raw_batch(&batch);
	} else if (!strcmp(cmd, "set-brightness")) {
		if (argc - argi != 1 ||
		    parse_u32(argv[argi], 0, 0xffff, &val)) {
			usage(argv[0]);
			return 2;
		}
		memset(&brightness, 0, sizeof(brightness));
		brightness.brightness = val;
		if (ioctl_retry(fd, WF_PANEL_IOC_SET_BRIGHTNESS,
				&brightness, retries)) {
			perror("WF_PANEL_IOC_SET_BRIGHTNESS");
			return 1;
		}
	} else if (!strcmp(cmd, "get-brightness")) {
		if (argc != argi) {
			usage(argv[0]);
			return 2;
		}
		memset(&brightness, 0, sizeof(brightness));
		if (ioctl_retry(fd, WF_PANEL_IOC_GET_BRIGHTNESS,
				&brightness, retries)) {
			perror("WF_PANEL_IOC_GET_BRIGHTNESS");
			return 1;
		}
		printf("brightness=0x%04x\n", brightness.brightness);
		} else if (!strcmp(cmd, "lpm-info")) {
			if (argc != argi) {
				usage(argv[0]);
				return 2;
			}
		memset(&lpm_info, 0, sizeof(lpm_info));
		if (ioctl(fd, WF_PANEL_IOC_GET_LPM_INFO, &lpm_info)) {
			perror("WF_PANEL_IOC_GET_LPM_INFO");
			return 1;
			}
			print_lpm_info(&lpm_info);
		} else if (!strcmp(cmd, "set-force-lpm")) {
			if (argc - argi != 1 ||
			    parse_u32(argv[argi], 0, 1, &val)) {
				usage(argv[0]);
				return 2;
			}
			memset(&force_lpm, 0, sizeof(force_lpm));
			force_lpm.force_lpm = val;
			if (ioctl(fd, WF_PANEL_IOC_SET_FORCE_LPM, &force_lpm)) {
				perror("WF_PANEL_IOC_SET_FORCE_LPM");
				return 1;
			}
		} else if (!strcmp(cmd, "get-force-lpm")) {
			if (argc != argi) {
				usage(argv[0]);
				return 2;
			}
			memset(&force_lpm, 0, sizeof(force_lpm));
			if (ioctl(fd, WF_PANEL_IOC_GET_FORCE_LPM, &force_lpm)) {
				perror("WF_PANEL_IOC_GET_FORCE_LPM");
				return 1;
			}
			printf("force_lpm=%u\n", force_lpm.force_lpm);
		} else if (!strcmp(cmd, "csv")) {
			int csv_ret = run_csv_cmd(fd, retries, argc - argi, &argv[argi]);

			if (csv_ret == -2) {
				usage(argv[0]);
				return 2;
			}
			if (csv_ret)
				return 1;
		} else if (!strcmp(cmd, "display-on") ||
			   !strcmp(cmd, "display-off") ||
		   !strcmp(cmd, "sleep-out") ||
		   !strcmp(cmd, "sleep-in")) {
		if (argc != argi) {
			usage(argv[0]);
			return 2;
		}

		if (!strcmp(cmd, "display-on"))
			request = WF_PANEL_IOC_DISPLAY_ON;
		else if (!strcmp(cmd, "display-off"))
			request = WF_PANEL_IOC_DISPLAY_OFF;
		else if (!strcmp(cmd, "sleep-out"))
			request = WF_PANEL_IOC_SLEEP_OUT;
		else
			request = WF_PANEL_IOC_SLEEP_IN;

		if (ioctl_noarg_retry(fd, request, retries)) {
			perror(cmd);
			return 1;
		}
	} else {
		usage(argv[0]);
		return 2;
	}

	close(fd);
	return 0;
}
