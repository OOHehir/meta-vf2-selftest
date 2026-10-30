SUMMARY = "Target checks for vulkan-wsi-layer on the VisionFive 2"
DESCRIPTION = "wsi-probe & vf2-vulkan-wsi-test: check that vulkan-wsi-layer \
advertises present timing consistently, that every surface format it \
advertises can back a swapchain, & that headless, VK_KHR_display & Wayland \
presentation work, as root & as a video-group user. KTAP output."
HOMEPAGE = "https://github.com/OOHehir/meta-vf2-selftest"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

DEPENDS = "vulkan-headers vulkan-loader wayland"

SRC_URI = "file://wsi-probe.c file://vf2-vulkan-wsi-test"
S = "${UNPACKDIR}"

inherit features_check
REQUIRED_DISTRO_FEATURES = "vulkan wayland"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} -std=c11 -DPROBE_WAYLAND -o ${B}/wsi-probe ${S}/wsi-probe.c \
        -lvulkan -lwayland-client
}

do_install() {
    install -D -m 0755 ${B}/wsi-probe ${D}${bindir}/wsi-probe
    install -D -m 0755 ${S}/vf2-vulkan-wsi-test ${D}${bindir}/vf2-vulkan-wsi-test
}

# No dependency on vulkan-wsi-layer: meta-riscv builds it for jh7110 only, so world
# builds for other machines would fail. Images add it; the script skips when it is missing.
RDEPENDS:${PN} = "vulkan-loader"
RRECOMMENDS:${PN} = "vulkan-tools vulkan-validation-layers"
