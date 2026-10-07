# Device tree for MEIZU 20 Pro — DerpFest 16.2 / LineageOS 23.2

device tree for the MEIZU 20 Pro (`m2391`).

## DerpFest 16.2 bring-up

Use the official **16.2** manifest, not the default branch:

- [DerpFest 16.2 manifest and build instructions](https://github.com/DerpFest-AOSP/android_manifest/tree/16.2)
- [ROM project paths](https://github.com/DerpFest-AOSP/android_manifest/blob/16.2/snippets/derp.xml)
- [Common phone configuration](https://github.com/DerpFest-AOSP/android_vendor_derpfest/blob/16.2/config/common_full_phone.mk)

This release is based on LineageOS 23.2. It still expects `lineage_m2391`,
`vendor/lineage/config/common_full_phone.mk`, and target release `bp4a`.
Its OTA target is **`derp`**, and its default build type is `Community`.
The official common configuration includes GMS.

The device tree detects `vendor/lineage/config/derpfest.mk` and omits
`M2391SettingsOverlay`: DerpFest removed the Lineage
`config_show_peak_refresh_rate_switch` / `config_show_min_refresh_rate_switch`
resources and supplies its own minimum/maximum refresh-rate settings page.
The framework refresh-rate defaults, UDFPS calibration, Doze, proximity check,
Aperture overlay, IMS fixups and vendor compatibility libraries are retained.
DerpFest `userdebug` builds keep `ro.debuggable=1` for bring-up and `adb root`;
`user` builds retain normal release behavior.

The DerpFest-only `M2391DerpFestSettingsOverlay` sets the About phone maintainer
name to `baizhi958216`. Edit
`overlay/DerpFestSettings/res/values/strings.xml` to change the
`derpfest_maintainer` string. DerpFest 16.2 reads this Settings resource rather
than a `DERPFEST_MAINTAINER` make variable. This display name is independent
of the `Community`/`Official` build type.

### Sources and device inputs

Compile on Linux x86_64 with an Android build environment, `repo`, Git LFS,
Python 3 and `rsync`. Use a separate source directory and output directory for
DerpFest. Sync the complete manifest so that GMS and the Qualcomm/Lineage
compatibility dependencies are present:

```sh
mkdir -p /mnt/build/derpfest
cd /mnt/build/derpfest
repo init -u https://github.com/DerpFest-AOSP/android_manifest.git -b 16.2 --git-lfs
repo sync -c -j8 --no-clone-bundle --no-tags --retry-fetches=3
```

Install all three matching trees at:

| Tree | Path in the Android source checkout | Baseline |
| --- | --- | --- |
| Device | `device/meizu/m2391` | This `derpfest-16.2` bring-up branch, including local changes |
| Vendor | `vendor/meizu/m2391` | `m2391-dev/android_vendor_meizu_m2391`, commit `95e53be19a21100b5998cb11011d32c08ffa00ca` |
| Kernel headers | `kernel/meizu/m2391` | `m2391-dev/android_kernel_meizu_m2391`, commit `1b39c5ae9a3c8488c7b55641e1b99d244d374c3f` |

The vendor/kernel repositories use `lineage-23.2`; run `git lfs pull` in both
if cloning them. The kernel tree provides captured configuration/UAPI headers;
the build uses the extracted **5.15 prebuilt kernel** and signed modules.
It is not a kernel source build. Keep the Android 13 firmware/blob baseline
and shipping API level 33.

To transfer the current local files to a Linux builder without losing
uncommitted changes, create a snapshot from the directory containing the three
trees (the example output is outside that directory):

```sh
tar --exclude=.git --exclude=.DS_Store --exclude=__pycache__ \
    -czf ../m2391-derpfest-16.2-trees.tar.gz \
    device/meizu/m2391 vendor/meizu/m2391 kernel/meizu/m2391
```

Transfer the archive to the builder, then extract it under
`/mnt/build/m2391-trees`. This is a staging directory, separate from the Android
source checkout. Use its updated device tree rather than the old pinned
Lineage installer in `build-support/install-device.sh`.

### Build with the helper

```sh
export DERPFEST_SOURCE_ROOT=/mnt/build/derpfest
export M2391_TREE_SOURCE=/mnt/build/m2391-trees
export JOBS=16
export BUILD_VARIANT=userdebug

# Install the snapshot and check source dependencies/kernel inputs first.
bash "$M2391_TREE_SOURCE/device/meizu/m2391/build-derpfest.sh" --check-only

# Build an already-synced source checkout.
bash "$M2391_TREE_SOURCE/device/meizu/m2391/build-derpfest.sh"
```

Pass `--sync` to initialize/sync the official 16.2 manifest before building.
The helper copies the current three trees (including local edits), checks the
ROM version, required source projects, compatibility modules, kernel/DTB/DTBO,
UAPI archive and signed-module inputs, and rejects unresolved LFS pointers in
the checked inputs. It then builds `mka derp`, checks the exact expected OTA
file and records its SHA-256. Logs, the pinned ROM manifest and key device
input hashes are saved under `build-logs/m2391/<timestamp>/` in the source
checkout; override this with `LOG_DIR` if needed.

### Manual build after installing the trees

```sh
cd /mnt/build/derpfest
source build/envsetup.sh
source vendor/lineage/vars/aosp_target_release
export DERPFEST_BUILD_TYPE=Community
lunch "lineage_m2391-$aosp_target_release-userdebug"
mka derp -j16
```

Outputs are in `out/target/product/m2391/` unless `OUT_DIR` is overridden.
The OTA name follows
`DerpFest-v16.2-<date>-m2391-Community-<variant>.zip`.
For initial debugging, `mka recoveryimage` and `mka sepolicy` can be built
before the full OTA.

### Validation status

Validated checks include GNU Make evaluation of both ROM product configurations,
DerpFest Settings/SystemUI/SDK overlay resource comparisons, device XML and shell
syntax, and the helper's copy/build control flow and error handling with a mock
builder. The mock builder does not compile Android.

This port has source/configuration checks only. A complete DerpFest OTA build
and on-device DerpFest boot have not yet been verified. The macOS checkout
contains the three device trees, not the full Android build system.
`--check-only` verifies inputs; it does not establish Soong/Kati build success.
After the first Linux build, verify the generated VINTF/SELinux policy and test
recovery/OTA, boot/decryption, IMS calls, cameras, fingerprint, display modes,
Doze and wake gestures on the device.

## LineageOS 23.2 build

The same product can still be used with the LineageOS 23.2 manifest; its
Settings overlay remains enabled there:

```sh
source build/envsetup.sh
source vendor/lineage/vars/aosp_target_release
lunch lineage_m2391 "$aosp_target_release" userdebug
mka bacon
```

## Device specifications

| Feature | Specification |
| --- | --- |
| Device | MEIZU 20 Pro |
| Codename | `m2391`; stock OTA identifier: `meizu20Pro` |
| SoC | Qualcomm Snapdragon 8 Gen 2 (`kalama`) |
| GPU | Adreno 740 |
| Memory | 8 GB / 12 GB, depending on variant |
| Storage | 128 GB / 256 GB / 512 GB, depending on variant |
| Display | 6.81-inch OLED, 3200 × 1440, 120 Hz |
| Battery | 5000 mAh (typical), 80 W wired / 50 W wireless charging |
| Rear cameras | 50 MP main + 50 MP ultrawide + 50 MP portrait telephoto |
| Front camera | 32 MP |

<p align="center">
<img src="https://openfile.meizu.com/group1/M00/0B/50/Cgbj0GTkEiiACPvqAAdsxsQKM48695.png" width="500" height="590">
</p>
