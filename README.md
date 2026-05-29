# LubanCat 5 双路 MIPI 屏调试仓库

这个仓库把本地 RK3588 LubanCat 5 双路 MIPI 屏调试相关文件合并到一个 Git 项目中，方便记录、对比和继续编译测试。

## 目录说明

- `ubuntu-ebf-rk3588/`：用于构建 LubanCat RK3588 Ubuntu 镜像的 SDK。
- `dtb goertek/`：歌尔屏厂商提供的设备树示例，以及本地对比用文件。
- `update/`：从 SDK 中抽出的最小修改包，便于单独查看或同步到其他 SDK。

当前双路 DSI 屏的主要修改集中在：

- `ubuntu-ebf-rk3588/kernel-6.1/arch/arm64/boot/dts/rockchip/overlay/rk3588-lubancat-5-goertek-dual-dsi-overlay.dts`
- `ubuntu-ebf-rk3588/kernel-6.1/arch/arm64/boot/dts/rockchip/overlay/rk3588-lubancat-5-goertek-dual-dsi-panel.dtsi`
- `ubuntu-ebf-rk3588/kernel-6.1/arch/arm64/boot/dts/rockchip/overlay/Makefile`
- `ubuntu-ebf-rk3588/config/uEnv/lubancat-5.uEnv`

设备树目标是尽量靠拢 `dtb goertek/Test Code.dtsi`。少量差异是为了适配当前 SDK 能编译和绑定，例如使用 SDK 里的背光节点名，以及移除当前内核头文件中不存在的 DSI flag。

## 如何开始编译

进入 SDK 目录：

```bash
cd /home/zhutao/mipi/lubancat-5-dual-mipi/ubuntu-ebf-rk3588
```

安装构建依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential gcc-aarch64-linux-gnu bison \
qemu-user-static qemu-system-arm u-boot-tools binfmt-support \
debootstrap flex libssl-dev bc rsync kmod cpio xz-utils fakeroot parted \
udev dosfstools uuid-runtime git-lfs device-tree-compiler python3 fdisk \
python-is-python3 python2
```

首次从 GitHub clone 后，建议拉取 LFS 文件：

```bash
git lfs install
git lfs pull
```

完整重新编译 LubanCat 5 V2 镜像：

```bash
sudo ./build.sh --board=lubancat-5-v2 -c
```

`-c` 会删除并重建 `ubuntu-ebf-rk3588/build/`，适合需要重新生成内核、U-Boot、rootfs 和镜像时使用。最终镜像输出在：

```text
ubuntu-ebf-rk3588/images/
```

常用编译方式：

```bash
# 只编译内核，适合快速验证设备树 overlay 和 panel/DSI 驱动改动
sudo ./build.sh --board=lubancat-5-v2 -k

# 只编译 U-Boot
sudo ./build.sh --board=lubancat-5-v2 -u

# 只生成 server 镜像
sudo ./build.sh --board=lubancat-5-v2 -so

# 只生成 desktop 镜像
sudo ./build.sh --board=lubancat-5-v2 -do

# 打印更详细的脚本执行日志
sudo ./build.sh --board=lubancat-5-v2 -v
```

如果怀疑 `build.sh -c` 清理不彻底，可以先手动清掉旧产物：

```bash
cd /home/zhutao/mipi/lubancat-5-dual-mipi/ubuntu-ebf-rk3588
sudo sh -c 'findmnt -Rno TARGET /home/zhutao/mipi/lubancat-5-dual-mipi/ubuntu-ebf-rk3588/build/rootfs 2>/dev/null | sort -r | while read -r m; do umount -lf "$m" || true; done'
sudo rm -rf /home/zhutao/mipi/lubancat-5-dual-mipi/ubuntu-ebf-rk3588/build /home/zhutao/mipi/lubancat-5-dual-mipi/ubuntu-ebf-rk3588/images
```

然后再执行：

```bash
sudo ./build.sh --board=lubancat-5-v2 -c
```

## 本仓库不跟踪的生成文件

以下内容是编译产物，不提交到 Git：

- `ubuntu-ebf-rk3588/build/`
- `ubuntu-ebf-rk3588/images/`
- `dtb goertek/**/*.dtbo`
- `dtb goertek/**/*.dtb`
- `dtb goertek/**/*.pp.dts`

这些文件都可以从 SDK 或设备树源码重新生成。
