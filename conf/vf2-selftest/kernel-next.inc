# VF2_KERNEL = "next": linux-mainline built from one linux-next tag, to catch
# upstream regressions before a release. Not a baseline for comparisons.

require conf/vf2-selftest/kernel-mainline.inc

# linux-next is rebuilt every weekday, so pin a tag & its commit; a CI job
# overrides both in auto.conf. The commit is the peeled tag (ls-remote "^{}").
VF2_NEXT_TAG ?= "next-20261030"
VF2_NEXT_REV ?= "7649ba73f9e7e1e04d0b46213e39c79ae09dd55c"

SRC_URI:remove:pn-linux-mainline = "git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git;protocol=https;branch=${BRANCH}"
SRC_URI:prepend:pn-linux-mainline = "git://git.kernel.org/pub/scm/linux/kernel/git/next/linux-next.git;protocol=https;nobranch=1;tag=${VF2_NEXT_TAG} "
SRCREV:pn-linux-mainline = "${VF2_NEXT_REV}"
# A full linux-next clone is several GB & changes daily; history is not needed.
BB_GIT_SHALLOW:pn-linux-mainline = "1"
# kern-tools' kconfiglib rejects next-20261030 with a POWER_SUPPLY/THERMAL
# dependency loop that the kernel's own Kconfig accepts, so skip its audit.
KMETA_AUDIT:pn-linux-mainline = ""
