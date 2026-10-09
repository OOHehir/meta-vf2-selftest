# VF2_KERNEL = "vendor": linux-starfive-dev (StarFive 6.12 fork, meta-riscv's
# default for visionfive2) with its closed GPU/VPU modules.

PREFERRED_PROVIDER_virtual/kernel = "linux-starfive-dev"

# meta-riscv marks the closed PowerVR firmware/DDK LICENSE_FLAGS "commercial".
LICENSE_FLAGS_ACCEPTED += "commercial"

# meta-riscv's soft_3rdpart mirror no longer serves it; StarFive's GitHub repo
# has the identical pinned commit. Needs git-lfs on the build host.
PREMIRRORS:append = " \
    git://strlcat.eu/mirroring/jh7110_soft_3rdpart.git git://github.com/starfive-tech/soft_3rdpart.git;protocol=https \n \
"
# Kernel config change: see recipes-kernel/linux/linux-starfive-dev.bbappend.
