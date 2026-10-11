# VF2_KERNEL = "open" only; other modes build linux-mainline unchanged. The patches'
# Upstream-Status names each source. The series' Rockchip PHY conversion is left out: it
# conflicts on 7.2 & that driver is not built for riscv.
FILESEXTRAPATHS:prepend:vf2open := "${THISDIR}/files:"
SRC_URI:append:vf2open = " \
    file://jh7110-open/0001-drm-bridge-inno-hdmi-fix-swapped-HSYNC-and-VSYNC-pol.patch \
    file://jh7110-open/0002-dt-bindings-phy-Add-starfive-jh7110-inno-hdmi-phy.patch \
    file://jh7110-open/0003-dt-bindings-display-bridge-Add-starfive-jh7110-inno-.patch \
    file://jh7110-open/0004-dt-bindings-soc-starfive-Add-starfive-jh7110-hdmi-su.patch \
    file://jh7110-open/0005-dt-bindings-soc-starfive-Add-starfive-jh7110-vout-sy.patch \
    file://jh7110-open/0006-dt-bindings-display-verisilicon-Add-starfive-jh7110-.patch \
    file://jh7110-open/0007-dt-bindings-soc-starfive-Add-starfive-jh7110-vout-su.patch \
    file://jh7110-open/0008-drm-bridge-inno-hdmi-Split-probe-out-of-bind.patch \
    file://jh7110-open/0009-drm-bridge-inno-hdmi-Allow-the-register-map-to-come-.patch \
    file://jh7110-open/0010-drm-bridge-inno-hdmi-Add-.disable-platform-operation.patch \
    file://jh7110-open/0011-drm-bridge-inno-hdmi-Add-.mode_valid-platform-operat.patch \
    file://jh7110-open/0012-drm-bridge-inno-hdmi-Make-the-PHY-configuration-tabl.patch \
    file://jh7110-open/0013-drm-bridge-inno-hdmi-Make-the-register-clock-source-.patch \
    file://jh7110-open/0014-soc-starfive-Add-jh7110-hdmi-subsystem-driver.patch \
    file://jh7110-open/0015-soc-starfive-Add-jh7110-vout-subsystem-driver.patch \
    file://jh7110-open/0016-drm-bridge-starfive-Add-JH7110-HDMI-controller-drive.patch \
    file://jh7110-open/0017-phy-Add-common-Innosilicon-HDMI-PHY-helpers.patch \
    file://jh7110-open/0018-phy-starfive-Add-jh7110-inno-hdmi-phy-driver.patch \
    file://jh7110-open/0019-riscv-dts-starfive-jh7110-Update-DT-for-display-subs.patch \
    file://jh7110-open/0020-MAINTAINERS-Add-StarFive-JH7110-display-subsystem-en.patch \
    file://jh7110-open/0021-riscv-alternative-Add-an-ALTERNATIVE_3-macro.patch \
    file://jh7110-open/0022-riscv-alternatives-support-auipc-load-pair.patch \
    file://jh7110-open/0023-riscv-errata-sifive-support-auipc-load-pair-in-patch.patch \
    file://jh7110-open/0024-riscv-errata-sifive-Add-an-errata-to-simulate-Svpbmt.patch \
    file://jh7110-open/0025-clk-starfive-jh7110-vout-Allow-pixel-clock-rate-prop.patch \
    file://jh7110-open/0026-riscv-dts-starfive-add-PowerVR-BXE-4-32-GPU-node-to-.patch \
    file://jh7110-open/0027-drm-imagination-skip-the-gpu-supported-test.patch \
    file://jh7110-open/0028-drm-imagination-flush-pending-fence-release-work-bef.patch \
    file://jh7110-open/0029-drm-imagination-surface-silent-FW-hard-resets-in-dme.patch \
    file://jh7110-open/0030-drm-imagination-skip-FW-cleanup-for-contexts-free-li.patch \
    file://jh7110-open/0031-drm-imagination-enable-JH7110-s-extra-GPU-clocks-and.patch \
    file://jh7110-open/0032-drm-imagination-power-up-rascal-dust-via-RGX_CR_POWE.patch \
    file://jh7110-open/0033-drm-imagination-don-t-flush-the-MMU-on-unmap-after-t.patch \
    file://jh7110-open/0034-drm-imagination-fix-wrong-vm_bo-passed-to-drm_gpuva_.patch \
    file://jh7110-open/0035-drm-imagination-fix-GEM-refcount-leak-in-pvr_vm_gpuv.patch \
    file://jh7110-open/0036-drm-imagination-fix-the-per-submit-heap-guard-leak.patch \
    file://jh7110-open/0037-drm-imagination-don-t-wedge-the-FW-when-tearing-down.patch \
    file://jh7110-open.cfg \
"
