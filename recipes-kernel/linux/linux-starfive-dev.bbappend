# The only change to the vendor kernel, & visible in results via kernel.config.md5.
# See files/randstruct.cfg.
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"
SRC_URI:append:visionfive2 = " file://randstruct.cfg"
