SUMMARY = "Firmware for the JH7110's PowerVR BXE-4-32 GPU (open drm/imagination driver)"
DESCRIPTION = "Imagination's GPU firmware for BVNC 36.50.54.182. Upstream \
linux-firmware does not carry it yet; Imagination's linux-firmware fork does."
HOMEPAGE = "https://gitlab.freedesktop.org/imagination/linux-firmware"
# Same text as oe-core linux-firmware's LICENSE.powervr.
LICENSE = "LicenseRef-Firmware-powervr"
NO_GENERIC_LICENSE[Firmware-powervr] = "LICENSE.powervr-${PV}-${FW_COMMIT}"
LIC_FILES_CHKSUM = "file://LICENSE.powervr-${PV}-${FW_COMMIT};md5=83045ed2a2cda15b4eaff682c98c9533"

# Pinned to the build the bring-up notes use ("v1.1"); see
# https://github.com/domibel/visionfive2_gpu_bringup
FW_COMMIT = "8a58f81883f7be458daa34e418cc4079f995b279"
FW_URI = "https://gitlab.freedesktop.org/imagination/linux-firmware/-/raw/${FW_COMMIT}"
# Commit in the download names: the shared DL_DIR then never mixes versions.
SRC_URI = "${FW_URI}/powervr/rogue_36.50.54.182_v1.fw;name=fw;downloadfilename=rogue_36.50.54.182_v1.fw-${PV}-${FW_COMMIT} \
           ${FW_URI}/LICENSE.powervr;name=license;downloadfilename=LICENSE.powervr-${PV}-${FW_COMMIT} \
           "
SRC_URI[fw.sha256sum] = "59127d3a02e48876310a520b7dc355f7430d3748d2940caaa33f39237d517abb"
SRC_URI[license.sha256sum] = "1c9aa6bd6703a7ce1cdb879542fa1d8aca115a327bd819b193c971de9c53f402"

S = "${UNPACKDIR}"
INHIBIT_DEFAULT_DEPS = "1"

inherit allarch

do_install() {
    install -Dm 0644 ${S}/rogue_36.50.54.182_v1.fw-${PV}-${FW_COMMIT} \
        ${D}${nonarch_base_libdir}/firmware/powervr/rogue_36.50.54.182_v1.fw
}

FILES:${PN} += "${nonarch_base_libdir}/firmware/powervr"

# An ELF for the GPU's own core, not the host; oe-core's linux-firmware skips the same.
# nooelint: oelint.vars.insaneskip
INSANE_SKIP:${PN} += "arch already-stripped ldflags"
