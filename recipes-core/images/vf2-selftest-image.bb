SUMMARY = "Minimal VisionFive 2 image carrying the board self-test"
DESCRIPTION = "Boots to a console with vf2-selftest, the upstream DT/device \
kselftests and kernel modules, and an SSH server for oeqa testimage. Built \
from oe-core + meta-riscv + meta-vf2-selftest only."
LICENSE = "MIT"

inherit core-image

IMAGE_INSTALL = " \
    packagegroup-core-boot \
    kernel-modules \
    vf2-selftest \
    vf2-kselftests \
    ${CORE_IMAGE_EXTRA_INSTALL} \
"
# Run automatically once per boot (results on the console + /var/log). Remove
# vf2-selftest-autorun from IMAGE_INSTALL for manual-only runs.
IMAGE_INSTALL:append = " vf2-selftest-autorun"

IMAGE_FEATURES += "ssh-server-dropbear"
IMAGE_LINGUAS = ""

COMPATIBLE_MACHINE = "visionfive2"
