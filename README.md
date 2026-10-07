# device tree for MEIZU 20 Pro

device tree for the MEIZU 20 Pro (`m2391`).

## Fresh build environment

Use an x86-64 Linux host; the current build environment uses Ubuntu 22.04.
Install the normal LineageOS build dependencies, Repo and Git LFS. Sync the
complete LineageOS 23.2 source tree (including Qualcomm platform dependencies):

```sh
mkdir -p lineage
cd lineage
repo init -u https://github.com/LineageOS/android.git -b lineage-23.2 --git-lfs
repo sync -c -j8
```

For reproducibility, use the revision-pinned source manifest from the matching
successful build instead of following moving branch heads.

Copy or check out both device-specific directories at these exact locations:

```text
device/meizu/m2391/   # includes extract-files.py, ims_fixup.py and overlays
kernel/meizu/m2391/   # BoardConfigKernel.mk, defconfig and UAPI header archive
```

The kernel directory is mandatory even though the kernel binary is extracted
from stock firmware. There is no `lineage.dependencies` to fetch it automatically.
Keep the entire device directory; copying only the makefiles omits required HAL,
SELinux, compatibility, overlay and extraction sources.

Generate `vendor/meizu/m2391` from the matching original full OTA as described
below. The `.work` scripts, Google Cloud VM, Docker container names and `/mnt`
paths used by the development workspace are not inputs to the device build.
Do not copy old `out/` contents into a fresh environment.

## Proprietary files and image generation

Use this tree with LineageOS 23.2 and its `tools/extract-utils`. From the
device directory, extract the matching official full OTA package on Linux
(reference incremental: `1764145455`, stock device: `meizu20Pro`):

```sh
cd device/meizu/m2391
./extract-files.py /path/to/update.zip
```

The host needs GNU `cpio`, `lz4`, and an `fsck.erofs` that supports
`--extract=DIR`. Ubuntu 22.04's stock erofs-utils is too old; the version in
LineageOS's `external/erofs-utils` supports the required extraction mode.

An unpacked firmware directory is also supported. It must contain the stock
`boot.img`, `vendor_boot.img`, `dtbo.img`, and the system, system_ext, product, vendor, ODM and DLKM images
or their extracted directories. IMS and carrier configuration require the
system-side partitions as well as vendor. The extraction hooks unpack the kernel, DTB
and vendor ramdisk modules. Use a disposable copy of a raw-image directory:
extract-utils removes processed images from that directory as it unpacks them.
`proprietary-files.txt` and
`proprietary-firmware.txt` pin the expected files to the reference firmware;
updating firmware requires reviewing those hashes and the matching module
load lists, properties, filesystem metadata and SELinux contexts together.
Stock preoptimized ODEX/VDEX caches are omitted; the signed APKs retain their DEX
code so the current ART runtime can compile it. Both stock QESDK library paths
are retained, including the 64-bit ELF originally stored under `vendor/lib`.

Generated blobs and build definitions are placed in `vendor/meizu/m2391`.
After editing the lists, regenerate the definitions with:

```sh
./setup-makefiles.py
```

The Android build generates `boot.img`, `init_boot.img`, `vendor_boot.img`,
`vendor.img`, `odm.img`, `vendor_dlkm.img` and `system_dlkm.img`. None of these
complete images belongs in a tree's `prebuilt` directory. The matching stock
kernel, DTB and signed `.ko` files remain proprietary inputs; this repository
contains a headers-only kernel Makefile, not a complete bootable kernel
source tree. `dtbo.img` remains an extracted firmware image. The reference
firmware's `system_dlkm` partition contains no kernel modules.

The vendor image combines source-built interface/support libraries and utilities
listed in `source-packages.mk` with the stock hardware implementations and
proprietary libraries. Source-built files are not duplicated in the extraction
list. VNDK 33 remains available for the original proprietary ABI.
The vendor service manager builds from source with the matching `libbinder`;
the stock executable uses an incompatible `IServiceManager` C++ ABI. A device
init override delays its first start until APEX activation, so its class restart
actions cannot start framework services in the bootstrap namespace.
ClearKey, the Qualcomm health service, the sensors Multi-HAL wrapper and the
standard Bluetooth audio/session components build from source. Qualcomm's
separate Bluetooth implementation libraries remain proprietary.
Old ABI consumers use the Lineage compatibility libraries and source shims.
The Qualcomm display and power libraries use `libtinyxml2-v34` to preserve
their embedded `XMLDocument` layout; current TinyXML2 is not ABI compatible.
The codec2 shim preserves the `C2Fence` C++ return type on both ARM32 and ARM64
while adapting the old one-argument fence factory to the current interface.
The SLIM daemon additionally uses compiler-rt's source implementation of
`__emutls_get_address`, which its original HIDL library exported.
The stock Wi-Fi supplicant retains its Meizu/QTI extensions and installs as
`wpa_supplicant.m2391`; the two stock init service definitions and SELinux
executable label use that path. Its legacy keystore engine and the 32-bit
Meizu media foundation library have distinct filenames so they coexist with
the source-built versions. This avoids overriding platform modules.

SELinux policy is compiled from the LineageOS Qualcomm SM8550 policy and the
device additions in `sepolicy/`; no extracted vendor CIL replaces the compiler's
output.
The stock Meizu IR, QTI USB/Gadget, NXP UWB and Xingji face services have
explicit executable labels for their existing HAL domains; their names do
not match the upstream service paths. Without these labels init cannot
start them, and SystemServer waits for the declared IR HAL during boot.
Vendor account IDs are declared in `config.fs`, and native rules generate
passwd/group. The device additions were ported from the reference firmware and must
pass the normal policy checks. Framework and recovery pieces also build from
source. Native VINTF rules combine the stock SKU manifest and HAL fragments
and generate the vendor compatibility matrix.

The stock Wi-Fi HIDL service loads its matching QCOM function table through
`configs/wifi/qcom-hals.xml`. Its QTI and Xingji supplicant AIDL extensions
retain their stock `hal_wifi_supplicant_service` labels; failed registration
otherwise terminates the whole supplicant. The dual-SIM default network mode
is restored to the stock `26,26` (NR/LTE/GSM/WCDMA). Existing subscriptions
retain their separately stored user network preferences.
The face HAL retains access to the stock TEE device and QSEECom secure heaps
needed to start its trusted application, with SELinux enforcing.

The camera HAL requires `ro.vendor.qti.va_aosp.support=1` to select the stock
device configuration; otherwise CHI selects a GSI configuration and exposes
only the main and front cameras. The stock `ro.vendor.camera.no_vts=1` camera
enumeration setting is also retained. Aperture's device overlay enables the
standalone ultrawide and telephoto selectors. Runtime testing exposed camera
IDs 0--4, and both auxiliary cameras successfully switched and saved photos.
This does not provide a separate stock-camera macro mode.
Framework-composited Ultra HDR (`JPEG_R`) stalls the stock HAL, so
`ro.camera.disableJpegR=true` keeps ordinary JPEG capture available.
The two `camera.qcom.so` variants read the bootloader PSN through the typed
`ro.vendor.camera.psn` property populated by vendor init. Their fixed-length
property-name replacement and original/fixed SHA1 hashes are recorded in the
extraction scripts and list. This restores EEPROM pair verification without
granting camera access to generic properties.
Hardware video decoding remains under investigation: a captured H.264 file
decodes all 47 frames in software, while the current Qualcomm decoder returns
no output. Camera capture/encoding and video playback must be tested separately.

`extract-files.py` configures extraction, applies narrow library fixups and
unpacks boot components. The Power HAL drops a redundant V3 AIDL dependency;
its V4 dependency provides every Power symbol it uses.
The extraction library generates ELF dependencies and enables its normal ELF
checks; the list does not blanket-disable dependency or symbol checking.
`Android.bp` declares the few device-specific overlays and links using standard
module types. The extraction list exposes their inputs through `FILEGROUP`.
Toybox and Qualcomm RFS modules generate their own links. There are no device
Soong plugins or custom image-generation rules.
Stock HAL init fragments that overlap platform install rules use a `.m2391.rc`
filename. Service names and permissions are preserved, with the documented
path and startup fixes applied during extraction. Android init imports them
from the normal vendor init directory. Three
proprietary HIDL startup declarations lack source interface metadata; their
HALs still register their interfaces at runtime. Gamepad and Wi-Fi learner
start with their existing init classes instead of waiting for a lazy-interface
trigger that the native init checker cannot validate.

The device vibrator HAL builds into vendor and replaces the unused Qualcomm
vibrator service. Timed vibration uses the AW8697 continuous-mode control;
clicks use the stock mBack effect. Cancellation, expiry and init service
stop/restart hooks stop the motor. The legacy `enable` interface repeats the
previous RAM effect and therefore does not produce smooth sustained vibration.
IMS uses the stock Android 16 Qualcomm application and native media libraries.
LineageOS builds `qti-telephony-hidl-wrapper`, `qti-telephony-utils`, their shared
library declarations and `ims-ext-common` from source. Its required shared
libraries are checked by Soong. Extraction removes the QSSI-only self-overlay
manifest gate while preserving DEX and resources; the IMS APK is platform-signed
and receives only its stock privileged permission allowlist. The framework MMTEL
binding points to `org.codeaurora.ims`. The matching stock carrier overlay restores per-carrier IMS support and signal
thresholds; device IMS capability flags match the stock framework overlay.
Voice, SMS and carrier registration still
require testing on the device after installing the resulting build.

Other Meizu-signed APKs keep their original signatures; source
`mac_permissions.xml` retains their seinfo assignments by package name.

Native LineageOS tools assemble the images and target-files, check VINTF and
generate the OTA payload and metadata. The OTA device identifier is `m2391`;
use the matching Lineage Recovery. This tree does not add the stock
`meizu20Pro` identifier to OTA metadata.

## Build

After extracting the files in a complete LineageOS checkout:

```sh
source build/envsetup.sh
source vendor/lineage/vars/aosp_target_release
lunch lineage_m2391 "$aosp_target_release" userdebug
mka recoveryimage
mka bacon
```

File extraction and a successful build are separate checks; hardware boot,
recovery and OTA installation still require testing on the device.

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
