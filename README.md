# meta-vf2-selftest

A board self-test for the StarFive VisionFive 2 (JH7110), for reporting **what
works with a given kernel** (normally mainline) and for catching regressions
between kernels and patches.

The layer depends on **oe-core only**. The self-test build uses nothing but
bitbake + oe-core + meta-riscv + this layer: the same minimal set meta-riscv
declares. That way the results describe that baseline, not meta-oe, meta-poky
or the demo layer in
[starfive-visionfive2-yocto](https://github.com/OOHehir/starfive-visionfive2-yocto).

## Contents

| Path | What it is |
|---|---|
| `recipes-support/vf2-selftest/files/vf2-selftest` | The self-test. POSIX sh that runs under busybox and reads `/proc` and `/sys`. Its only runtime dependency is `aplay`. |
| `recipes-support/vf2-selftest/` | Package `vf2-selftest`, plus `vf2-selftest-autorun`, which runs the test once per boot and writes the results to the console and `/var/log/vf2-selftest/`. |
| `recipes-kernel/vf2-kselftests/` | The upstream kernel kselftests `dt/test_unprobed_devices.sh` and `devices/error_logs`, packaged from the image's own kernel source. Their `compatible_list` is generated from that same tree. |
| `recipes-core/images/vf2-selftest-image.bb` | A minimal console image with the above, kernel modules and dropbear. |
| `lib/oeqa/runtime/cases/vf2_selftest.py` | An oeqa runtime case, so `bitbake -c testimage` runs the self-test on real hardware. |
| `conf/templates/default/` | `local.conf` and `bblayers.conf` templates for the minimal build (nodistro). The kernel is chosen with `VF2_KERNEL`. |
| `conf/vf2-selftest/kernel-{mainline,vendor}.inc` | Each kernel's deviations from meta-riscv defaults, with the reason for each. |
| `dynamic-layers/riscv-layer/recipes-kernel/linux/linux-starfive-dev.bbappend` | The vendor build's single kernel change: `RANDSTRUCT_NONE`. Without it, the closed GPU module soft-locks at init. |

## Build

The `bblayers.conf` template expects this layout, which is the one
starfive-visionfive2-yocto uses:

```
<top>/
├── meta-vf2-selftest/       this repository
└── sources/
    ├── bitbake/
    ├── oe-core/
    └── meta-riscv/
```

Then, from `<top>`:

```bash
TEMPLATECONF=$PWD/meta-vf2-selftest/conf/templates/default \
    source sources/oe-core/oe-init-build-env build-selftest
bitbake vf2-selftest-image
```

Where:
- `TEMPLATECONF` seeds `conf/local.conf` & `conf/bblayers.conf` only when the
  build directory is new; an existing one keeps its own
- `build-selftest` is the build directory; any name will do

The image is
`build-selftest/tmp/deploy/images/visionfive2/vf2-selftest-image-visionfive2.rootfs.wic.gz`.
Inside starfive-visionfive2-yocto, `scripts/bb-selftest.sh vf2-selftest-image`
does the same.

For the StarFive vendor kernel (6.12 fork plus its closed GPU/VPU modules), use
a second build directory & select the kernel in its `conf/auto.conf`:

```bash
TEMPLATECONF=$PWD/meta-vf2-selftest/conf/templates/default \
    source sources/oe-core/oe-init-build-env build-selftest-vendor
echo 'VF2_KERNEL = "vendor"' > conf/auto.conf
bitbake vf2-selftest-image
```

`scripts/bb-selftest.sh --vendor vf2-selftest-image` does the same in
starfive-visionfive2-yocto.

The two builds use the same layers, `local.conf`, U-Boot, OpenSBI and
userspace. Only `VF2_KERNEL` in `conf/auto.conf` differs, so a difference in
the results comes from the kernel and its modules. The environment header in
each report shows this: `u-boot`, `sbi` and `os` should match, while
`kernel*` and `fdt.md5` differ. The vendor build needs `git-lfs` on the host
for the GPU/VPU blobs.

The script itself has no build dependency. To test another image, for
example the vendor 6.12 demo image, copy it over and run it:
`scp vf2-selftest root@board:/tmp/ && ssh root@board /tmp/vf2-selftest`.

## Run

```bash
vf2-selftest                          # KTAP on stdout
vf2-selftest -o /tmp/st               # also writes /tmp/st/{ktap.txt,results.tsv,report.md}
vf2-selftest --baseline old.tsv -o /tmp/st   # diff against an earlier run
vf2-selftest --no-link                # no Ethernet cables on the bench
vf2-selftest --stress 300             # add a 5-minute stress-ng CPU+memory run
```

Exit codes:

| Code | Meaning |
|---|---|
| 0 | no FAIL |
| 1 | at least one FAIL, or with `--baseline`, at least one regression |
| 2 | usage error |

Under oeqa (set `TEST_TARGET_IP` and `TEST_SERVER_IP` in `local.conf`, then boot
the board on the image):

```bash
bitbake vf2-selftest-image -c testimage        # in the build environment above
# reports copied to build-selftest/tmp/work/.../vf2-selftest-image/*/vf2-selftest/
```

`TEST_SUITES` runs the upstream generic oeqa cases (`ping ssh df parselogs`)
before `vf2_selftest`. `VF2_SELFTEST_ARGS` passes options through, and
`VF2_SELFTEST_BASELINE` points at a host-side `results.tsv` to diff against.

## Reading the results

Each check is one KTAP result. Its ID (for example `eth.gmac1.link`) stays the
same across runs, so results can be diffed.

**Levels:** a result claims only what its level covers.

| Level | Meaning |
|---|---|
| `probe` | The DT node is enabled and a driver is bound to it. This says nothing about whether the hardware works. |
| `function` | The feature did something observable. Examples: 32 MiB read from the SD card, carrier at 1000 Mb/s and the gateway answers ping, every cpufreq OPP read back, 4 KiB from the TRNG covering 250 or more byte values, the crypto engine's algorithms passing the kernel crypto-manager self-tests. Where a check stops short of physical verification, its detail says so, for example "PWM: config read back, waveform not verified" or "audio: silence played, output not observed". |

**Failure classes:** these tell you where a FAIL needs to go.

| Class | Meaning | Usually goes to |
|---|---|---|
| `config-off` | No driver for the compatible in this kernel build (`CONFIG_*` not set) | defconfig / distro config |
| `not-loaded` | The driver is a module that exists but isn't loaded | userspace / module autoload |
| `deferred` | Probe deferred; the reason comes from debugfs `devices_deferred` | a missing supplier (clock, regulator, PHY) |
| `probe-error` | The driver is present but didn't bind (dmesg lines attached) | driver / DT maintainer |
| `function` | Bound, but the functional check failed | driver maintainer |
| `log` | Error-level kernel log lines, or a tainted kernel | whichever subsystem logged it |

**SKIP reasons:**

| Reason | Meaning |
|---|---|
| `not-in-dt` | Not described in this DT. This is how GPU, display, VPU and ISP currently show up on mainline. If the binding lands under one of the expected compatibles, the result changes automatically. |
| `disabled-in-dt` | The node exists but is disabled. |
| `hw-absent` | No card, NVMe or USB device is attached. |
| `probe-failed` | The function check didn't run because its probe check failed. |
| `option` | The check wasn't requested, or its tool is missing. |

Every result is followed by a `# repro:` command, so it can be checked by hand
without this script, and by `#  |` evidence lines with the raw values behind
the verdict. The environment header records everything needed to reproduce
the run:

- kernel release, build string and compiler
- an md5 of `/proc/config.gz` and an md5 of the FDT the kernel actually booted with (`/sys/firmware/fdt`)
- the SBI implementation and version, and the U-Boot version (`/chosen/u-boot,version`)
- whether the system booted through EFI
- the kernel command line and the root device

## Showing that the checks can fail

A check nobody has seen fail isn't evidence. Before quoting a result, run
negative and positive controls.

**Negative control:** unbind a driver and confirm that both its probe and
function checks flip:

```bash
echo 1600c000.rng > /sys/bus/platform/drivers/jh7110-trng/unbind
vf2-selftest -q | grep trng      # -> not ok trng.probe ... / SKIP trng.read [probe-failed]
echo 1600c000.rng > /sys/bus/platform/drivers/jh7110-trng/bind
```

**Positive control:** run the same script on the vendor 6.12 demo image,
where GPU and display work. The `gpu.*` and `display.*` checks should PASS
there and SKIP (`not-in-dt`) on mainline.

## Vendor kernel vs mainline

Run the self-test on the same board, once with each image, and keep each
`results.tsv`:

```bash
vf2-selftest -o /tmp/st     # on the vendor image, then on the mainline image
vf2-selftest --compare vendor=vendor/results.tsv mainline=mainline/results.tsv > compare.md
```

`--compare` doesn't test anything. It runs on any host, needs no root, and
takes two or more runs. The Markdown it produces contains:

- an environment table, one column per run
- pass/fail/skip totals for each run
- **Differences**: every check that passes in some runs but not all
- the full matrix

For mainline maintainers, the Differences table is the gap list. Each
mainline cell carries its class, so the gaps sort into four kinds:

| Mainline cell | Kind of gap |
|---|---|
| `skip: not-in-dt` | binding or DT work |
| `FAIL config-off` | defconfig |
| `FAIL probe-error` / `deferred` | driver work |
| `FAIL function` | a driver that binds but doesn't work |

Check IDs are the same for both kernels, and so are unit addresses, because
the hardware is the same. Where the vendor DT uses its own compatibles,
`first_compat` lists both the vendor name and the expected upstream name.

## Using it in a PR or upstream report

1. Run on a stock kernel and keep `results.tsv` as the baseline.
2. Apply the patch, rebuild and boot, then run
   `vf2-selftest --baseline baseline.tsv -o after`.
3. Paste `after/report.md` into the PR. It has a "Change vs baseline" table
   followed by the full results. Attach `after/ktap.txt` for the evidence.
4. For a kernel patch, the `Tested-by:` note can be a single line: "on
   VisionFive 2 v1.3B: `<check ids>` FAIL→PASS, no regressions in N other
   checks".

Results from the upstream kselftests (`kselftest.*`) and from oe-core's
`parselogs` are reported separately from this script's own verdicts. Where an
upstream test covers something, its result should be quoted first.

## Limitations

- **Board-specific checks:** the unit addresses and expected devices (such as
  the VL805 behind PCIe0) are those of the v1.2A and v1.3B boards.
- **Unconfirmed compatibles:** for features with no mainline binding yet
  (GPU, DC8200, HDMI, DSI, Wave5, JPU and the RTC), the upstream compatibles
  checked are best guesses; the vendor ones are taken from the 6.12 DT. Add
  the real upstream names to `first_compat` when the bindings land.
  Otherwise the check keeps reporting `not-in-dt` on mainline.
- **Vendor-only blocks:** CAN, S/PDIF, PDM, the SoC timer and camera VIN have
  no mainline nodes at all. On mainline they report `not-in-dt`, which keeps
  them visible in the comparison.
- **Bench-specific Ethernet:** the eth0 PHY on the bring-up unit is known to be
  faulty (`docs/PRODUCTIONIZE.md` #7), so on that unit `eth.gmac0.link` is
  expected to fail. Use `--no-link` when no cables are connected.

## Upstream candidates

These are things this work suggests contributing upstream:

- **Linux:** a `tools/testing/selftests/devices/probe/boards/starfive,visionfive-2-v1.3b.yaml`
  describing the PCIe and USB topology, so upstream's `test_discoverable_devices.py`
  covers this board.
- **oe-core:** a `parselogs-ignores-visionfive2.txt` (or `-riscv64`) for benign
  boot messages, once a few boots of logs have been collected.
- **Linux defconfig:** any check that is `config-off` on the riscv defconfig
  (for example `CONFIG_EEPROM_AT24`, `CONFIG_PWM_OCORES` or
  `CONFIG_CRYPTO_DEV_JH7110`) could be a defconfig patch, if the subsystem
  maintainers agree.
