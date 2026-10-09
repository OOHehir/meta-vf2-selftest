SUMMARY = "Board self-test for the StarFive VisionFive 2 (JH7110)"
DESCRIPTION = "Checks each on-board feature at probe level (DT node enabled, \
driver bound) and function level (data moved / value read back), classifies \
failures (config-off, not-loaded, deferred, probe-error, function) and emits \
KTAP plus TSV/Markdown reports, with a --baseline diff for regression testing. \
POSIX sh, runs under busybox."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://vf2-selftest \
    file://vf2-selftest-autorun \
    file://vf2-selftest-autorun.service \
"
S = "${UNPACKDIR}"

inherit allarch update-rc.d systemd

# Everything is read from /proc and /sys; the only tool beyond busybox is aplay
# (audio.play SKIPs without it). stress-ng is only used with --stress.
RDEPENDS:${PN} = "alsa-utils-aplay"
RRECOMMENDS:${PN} = "stress-ng"

PACKAGES =+ "${PN}-autorun"
RDEPENDS:${PN}-autorun = "${PN}"
FILES:${PN}-autorun = " \
    ${libexecdir}/vf2-selftest-autorun \
    ${sysconfdir}/init.d/vf2-selftest-autorun \
    ${systemd_system_unitdir}/vf2-selftest-autorun.service \
"
INITSCRIPT_PACKAGES = "${PN}-autorun"
INITSCRIPT_NAME:${PN}-autorun = "vf2-selftest-autorun"
INITSCRIPT_PARAMS:${PN}-autorun = "start 99 5 ."
SYSTEMD_PACKAGES = "${PN}-autorun"
SYSTEMD_SERVICE:${PN}-autorun = "vf2-selftest-autorun.service"

do_install() {
    install -D -m 0755 ${S}/vf2-selftest ${D}${bindir}/vf2-selftest
    install -D -m 0755 ${S}/vf2-selftest-autorun ${D}${libexecdir}/vf2-selftest-autorun
    if ${@bb.utils.contains('DISTRO_FEATURES', 'sysvinit', 'true', 'false', d)}; then
        install -d ${D}${sysconfdir}/init.d
        ln -s ${libexecdir}/vf2-selftest-autorun ${D}${sysconfdir}/init.d/vf2-selftest-autorun
    fi
    if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
        install -D -m 0644 ${S}/vf2-selftest-autorun.service \
            ${D}${systemd_system_unitdir}/vf2-selftest-autorun.service
    fi
}
