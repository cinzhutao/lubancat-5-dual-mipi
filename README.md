# LubanCat 5 Dual MIPI Panel Bring-up

This repository collects the local RK3588 LubanCat 5 dual-MIPI display bring-up files in one git project.

## Directory Layout

- `ubuntu-ebf-rk3588/`: Firefly/Embedfire Ubuntu SDK tree used to build the LubanCat RK3588 image.
- `dtb goertek/`: Goertek vendor device-tree reference files and local comparison material.
- `update/`: Minimal patch/update package copied from the SDK for easier review and transfer.

## Current Display Work

The main local work is aligned toward `dtb goertek/Test Code.dtsi` for the Goertek dual-DSI panel. The SDK overlay and related files are kept in:

- `ubuntu-ebf-rk3588/kernel-6.1/arch/arm64/boot/dts/rockchip/overlay/rk3588-lubancat-5-goertek-dual-dsi-overlay.dts`
- `ubuntu-ebf-rk3588/kernel-6.1/arch/arm64/boot/dts/rockchip/overlay/rk3588-lubancat-5-goertek-dual-dsi-panel.dtsi`
- `ubuntu-ebf-rk3588/kernel-6.1/arch/arm64/boot/dts/rockchip/overlay/Makefile`
- `ubuntu-ebf-rk3588/config/uEnv/lubancat-5.uEnv`

Known local deviations from the vendor sample are limited to settings required by this SDK/kernel to compile and bind correctly, such as using the SDK backlight node name and removing unsupported DSI flag definitions.

## Excluded Generated Files

Generated build artifacts are intentionally not tracked:

- `ubuntu-ebf-rk3588/build/`
- `ubuntu-ebf-rk3588/images/`
- `dtb goertek/**/*.dtbo`
- `dtb goertek/**/*.dtb`
- `dtb goertek/**/*.pp.dts`

These files can be regenerated from the SDK and should not be pushed to git.

## Build Note

Before a clean rebuild, remove old SDK outputs from the original SDK tree:

```bash
cd /home/zhutao/mipi/ubuntu-ebf-rk3588
sudo sh -c 'findmnt -Rno TARGET /home/zhutao/mipi/ubuntu-ebf-rk3588/build/rootfs 2>/dev/null | sort -r | while read -r m; do umount -lf "$m" || true; done'
sudo rm -rf /home/zhutao/mipi/ubuntu-ebf-rk3588/build /home/zhutao/mipi/ubuntu-ebf-rk3588/images
```

Then rebuild with the LubanCat 5 board target as needed.
