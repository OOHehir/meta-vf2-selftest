# VF2_KERNEL = "open": linux-mainline with out-of-tree display & GPU drivers; the GPU still
# loads Imagination's binary firmware. Experimental, so not a baseline for comparisons.

require conf/vf2-selftest/kernel-mainline.inc

# Selects the patches & config fragment in
# dynamic-layers/riscv-layer/recipes-kernel/linux/linux-mainline.bbappend.
OVERRIDES .= ":vf2open"

# The PowerVR Vulkan driver; OpenGL runs on it through zink (on by default).
PACKAGECONFIG:append:pn-mesa = " imagination libclc"

CORE_IMAGE_EXTRA_INSTALL:append = " \
    powervr-firmware-jh7110 \
    mesa-vulkan-drivers \
    vulkan-loader \
    vulkan-tools \
    kmscube \
    libdrm-tests \
"
