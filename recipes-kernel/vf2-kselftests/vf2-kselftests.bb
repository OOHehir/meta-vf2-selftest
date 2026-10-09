SUMMARY = "Upstream kernel DT/device kselftests, built from the image's own kernel source"
DESCRIPTION = "Packages tools/testing/selftests/dt (test_unprobed_devices.sh) and \
devices/error_logs from the kernel being built, with compatible_list generated \
from that same source tree, so results are the upstream tests' verdict for this \
exact kernel. vf2-selftest runs them and reports them alongside its own checks."
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

inherit kernelsrc python3native

# compatible_list is specific to the kernel source it was generated from.
PACKAGE_ARCH = "${MACHINE_ARCH}"
B = "${WORKDIR}/build"

KST = "${S}/tools/testing/selftests"
KST_DEST = "${libdir}/vf2-selftest/kselftest"

do_configure[noexec] = "1"

do_compile() {
    # Same as tools/testing/selftests/dt/Makefile's compatible_list rule
    nativepython3 ${S}/scripts/dtc/dt-extract-compatibles -d ${S} > ${B}/compatible_list
}

do_install() {
    install -D -m 0644 ${KST}/kselftest/ktap_helpers.sh ${D}${KST_DEST}/kselftest/ktap_helpers.sh
    install -D -m 0644 ${KST}/kselftest/ksft.py ${D}${KST_DEST}/kselftest/ksft.py
    install -D -m 0755 ${KST}/dt/test_unprobed_devices.sh ${D}${KST_DEST}/dt/test_unprobed_devices.sh
    install -D -m 0644 ${KST}/dt/compatible_ignore_list ${D}${KST_DEST}/dt/compatible_ignore_list
    install -D -m 0644 ${B}/compatible_list ${D}${KST_DEST}/dt/compatible_list
    install -D -m 0755 ${KST}/devices/error_logs/test_device_error_logs.py \
        ${D}${KST_DEST}/devices/error_logs/test_device_error_logs.py
}

FILES:${PN} = "${KST_DEST}"
RDEPENDS:${PN} = "bash python3-core"
