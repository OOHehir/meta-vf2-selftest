# VF2_KERNEL = "mainline": linux-mainline from meta-riscv.
# Every line is a deviation from meta-riscv defaults, needed to build at all.

# linux-mainline does not list visionfive2 in COMPATIBLE_MACHINE (meta-riscv 38333d4).
PREFERRED_PROVIDER_virtual/kernel = "linux-mainline"
COMPATIBLE_MACHINE:pn-linux-mainline = "visionfive2"
# linux-mainline has no yocto-kernel-cache, so the cfg/efi.scc fragment that
# MACHINE_FEATURES "efi" pulls in does not exist. riscv defconfig sets CONFIG_EFI=y.
KERNEL_FEATURES:remove:pn-linux-mainline = "cfg/efi.scc"
# The closed GPU/VPU modules only build against the vendor 6.12 kernel.
MACHINE_ESSENTIAL_EXTRA_RRECOMMENDS:remove = " \
    linux-firmware-visionfive2-imggpu \
    vdec-module \
    venc-module \
    jpu-module \
    visionfive2-pvr-graphics-tools \
"
KERNEL_MODULE_AUTOLOAD:remove = "pvrsrvkm"
