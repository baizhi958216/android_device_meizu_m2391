#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -e -o pipefail

# Run on a Linux x86_64 builder. The tree snapshot may include local changes.
# --sync initializes/syncs the official 16.2 source before installing the trees.
# --check-only installs the trees and checks inputs without compiling.
sync_source=false
check_only=false
for argument in "$@"; do
    case "$argument" in
        --sync) sync_source=true ;;
        --check-only) check_only=true ;;
        *) echo "Usage: $0 [--sync] [--check-only]" >&2; exit 2 ;;
    esac
done

: "${DERPFEST_SOURCE_ROOT:?Set DERPFEST_SOURCE_ROOT to the DerpFest source directory}"
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
M2391_TREE_SOURCE=${M2391_TREE_SOURCE:-$(cd "$script_dir/../../.." && pwd -P)}
BUILD_VARIANT=${BUILD_VARIANT:-userdebug}
JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN)}
SYNC_JOBS=${SYNC_JOBS:-8}
case "$BUILD_VARIANT" in
    user|userdebug|eng) ;;
    *) echo "Invalid BUILD_VARIANT: $BUILD_VARIANT" >&2; exit 2 ;;
esac
for count in "$JOBS" "$SYNC_JOBS"; do
    if [[ ! "$count" =~ ^[1-9][0-9]*$ ]]; then
        echo "JOBS and SYNC_JOBS must be positive integers" >&2
        exit 2
    fi
done
if ! "$check_only" && [[ "$(uname -s)/$(uname -m)" != Linux/x86_64 ]]; then
    echo "Compile on Linux x86_64; use --check-only for input checks on this host." >&2
    exit 1
fi

M2391_TREE_SOURCE=$(cd "$M2391_TREE_SOURCE" && pwd -P)
for tree in device vendor kernel; do
    test -d "$M2391_TREE_SOURCE/$tree/meizu/m2391" || {
        echo "Missing tree: $M2391_TREE_SOURCE/$tree/meizu/m2391" >&2
        exit 1
    }
done
mkdir -p "$DERPFEST_SOURCE_ROOT"
cd "$DERPFEST_SOURCE_ROOT"
DERPFEST_SOURCE_ROOT=$(pwd -P)
LOG_DIR=${LOG_DIR:-$DERPFEST_SOURCE_ROOT/build-logs/m2391/$(date +%Y%m%d-%H%M%S)}
mkdir -p "$LOG_DIR"
LOG_DIR=$(cd "$LOG_DIR" && pwd -P)
state() { printf '%s %s\n' "$(date -u +%FT%TZ)" "$*" | tee "$LOG_DIR/state"; }
trap 'rc=$?; if [ "$rc" -ne 0 ]; then state "FAILED exit=$rc line=$LINENO"; fi' EXIT

if "$sync_source"; then
    # Avoid changing the manifest of an existing build for another ROM.
    if [[ -d .repo ]]; then
        manifest_url=$(git --git-dir=.repo/manifests.git config --get remote.origin.url)
        case "$manifest_url" in
            https://github.com/DerpFest-AOSP/android_manifest|https://github.com/DerpFest-AOSP/android_manifest.git) ;;
            *) echo "Use a separate DerpFest source directory (found $manifest_url)." >&2; exit 1 ;;
        esac
    elif [[ -f build/envsetup.sh ]]; then
        echo "Source files already exist here without a repo manifest; use a separate directory." >&2
        exit 1
    fi
    state INITIALIZING
    repo init -u https://github.com/DerpFest-AOSP/android_manifest.git -b 16.2 --git-lfs --no-clone-bundle
    state SYNCING
    repo sync -c -j"$SYNC_JOBS" --no-clone-bundle --no-tags --retry-fetches=3 2>&1 | tee "$LOG_DIR/sync.log"
fi

# Check the ROM before copying any device files into its source checkout.
python3 - <<'PY'
import re
from pathlib import Path

config = Path('vendor/lineage/config')
version = config / 'version.mk'
if not (config / 'derpfest.mk').is_file() or not version.is_file():
    raise SystemExit('DerpFest sources are missing. Sync the official 16.2 manifest first.')
if not re.search(r'^DERPFEST_VERSION\s*:=\s*16\.2\s*$', version.read_text(), re.M):
    raise SystemExit('Expected DerpFest 16.2; this source checkout has another version.')
required = [
    'build/envsetup.sh', 'vendor/lineage/vars/aosp_target_release',
    'vendor/lineage/build/tasks/derp.mk', 'vendor/gms/products/gms.mk',
    'hardware/lineage/compat/Android.bp', 'hardware/qcom-caf/bootctrl/Android.bp',
    'device/qcom/sepolicy_vndr/SEPolicy.mk', 'device/qcom/sepolicy_vndr/sm8550',
    'lineage-sdk', 'packages/apps/Aperture', 'tools/extract-utils',
    'prebuilts/vndk/v33', 'vendor/qcom/opensource/interfaces',
    'vendor/codeaurora/telephony',
]
missing = [path for path in required if not Path(path).exists()]
if missing:
    raise SystemExit('Missing source dependencies:\n  ' + '\n  '.join(missing))
compat = Path('hardware/lineage/compat/Android.bp').read_text()
for module in ['libbase-v33', 'libhidlbase-v32', 'libtinyxml2-v34',
               'libprotobuf-cpp-full-3.9.1-vendorcompat',
               'libprotobuf-cpp-lite-3.9.1-vendorcompat']:
    if f'name: "{module}"' not in compat:
        raise SystemExit(f'Missing compatibility module: {module}')
PY

state INSTALLING_DEVICE_TREES
for tree in device vendor kernel; do
    path=$tree/meizu/m2391
    if [[ "$M2391_TREE_SOURCE" != "$DERPFEST_SOURCE_ROOT" ]]; then
        mkdir -p "$path"
        input_tree=$(cd "$M2391_TREE_SOURCE/$path" && pwd -P)
        target_tree=$(cd "$path" && pwd -P)
        [[ "$input_tree" == "$target_tree" ]] && continue
        # Copy the current files, including uncommitted bring-up changes.
        # Refresh mtimes so incremental builds do not retain older policies.
        rsync -rl --checksum --delete --exclude=.git --exclude=.DS_Store --exclude=__pycache__ \
            "$M2391_TREE_SOURCE/$path/" "$path/"
    fi
done
python3 - <<'PY'
import tarfile
from pathlib import Path

required = [
    'device/meizu/m2391/lineage_m2391.mk',
    'vendor/meizu/m2391/Android.bp', 'vendor/meizu/m2391/m2391-vendor.mk',
    'vendor/meizu/m2391/proprietary/boot/kernel',
    'vendor/meizu/m2391/radio/dtbo.img',
    'vendor/meizu/m2391/proprietary/vendor_boot/dtb/kalama.dtb',
    'kernel/meizu/m2391/include/kernel-uapi-headers.tar.gz',
]
for name in required:
    path = Path(name)
    if not path.is_file() or not path.stat().st_size:
        raise SystemExit(f'Missing or empty input: {name}')
    with path.open('rb') as stream:
        if stream.read(100).startswith(b'version https://git-lfs.github.com/spec/v1'):
            raise SystemExit(f'Unresolved Git LFS pointer: {name}; run git lfs pull in its tree.')
with tarfile.open(required[-1]) as archive:
    if not archive.getmembers():
        raise SystemExit('Empty kernel UAPI headers archive')
for partition in ['vendor_boot', 'vendor_dlkm']:
    if not list(Path('vendor/meizu/m2391/proprietary', partition, 'lib/modules').glob('*.ko')):
        raise SystemExit(f'Missing signed kernel modules for {partition}')
print('DerpFest 16.2 source and m2391 input checks passed.')
PY
if "$check_only"; then
    state INPUTS_READY
    exit 0
fi

state CONFIGURING
repo manifest -r -o "$LOG_DIR/manifest.xml"
sha256sum device/meizu/m2391/{lineage_m2391.mk,device.mk,BoardConfig.mk} \
    kernel/meizu/m2391/BoardConfigKernel.mk \
    vendor/meizu/m2391/{Android.bp,m2391-vendor.mk,proprietary/boot/kernel,radio/dtbo.img} \
    > "$LOG_DIR/device-inputs.sha256"
export DERPFEST_BUILD_TYPE=${DERPFEST_BUILD_TYPE:-Community}
export TZ=Asia/Shanghai
source build/envsetup.sh
source vendor/lineage/vars/aosp_target_release
lunch "lineage_m2391-$aosp_target_release-$BUILD_VARIANT" > >(tee "$LOG_DIR/lunch.log") 2>&1
product_out=$(get_abs_build_var PRODUCT_OUT)
package="$product_out/$(get_build_var LINEAGE_VERSION).zip"
state BUILDING
mka derp -j"$JOBS" 2>&1 | tee "$LOG_DIR/build.log"
state VERIFYING_ARTIFACTS
test -s "$package"
sha256sum "$package" | tee "$LOG_DIR/sha256.txt"
state COMPLETE
