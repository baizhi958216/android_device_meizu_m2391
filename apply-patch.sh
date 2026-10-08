#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

usage() {
    echo "Usage: $0 [--check] [SOURCE_ROOT]"
    echo "Apply device patches to the Android source tree (default: this tree's root)."
    echo "  --check  Check all patches without changing source files."
}

check_only=false
source_root=
for argument in "$@"; do
    case "$argument" in
        --check) check_only=true ;;
        -h|--help) usage; exit 0 ;;
        -*) usage >&2; exit 2 ;;
        *)
            if [[ -n "$source_root" ]]; then
                usage >&2
                exit 2
            fi
            source_root=$argument
            ;;
    esac
done

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
source_root=${source_root:-$script_dir/../../..}
source_root=$(cd -- "$source_root" && pwd -P)
if [[ ! -f "$source_root/build/envsetup.sh" ]]; then
    echo "Android sources are missing at $source_root; pass the full source tree as SOURCE_ROOT." >&2
    exit 1
fi

repositories=()
patches=()
check_project() {
    local project=$1 patch_directory=$2 patch
    local repository=$source_root/$project
    local project_patches=("$script_dir/patches/$patch_directory/"*.patch)
    if [[ ! -d "$repository" ]]; then
        echo "Missing source project: $repository" >&2
        exit 1
    fi
    if [[ ! -f "${project_patches[0]}" ]]; then
        echo "No patches found in $script_dir/patches/$patch_directory" >&2
        exit 1
    fi
    for patch in "${project_patches[@]}"; do
        if git -C "$repository" apply --check "$patch" 2>/dev/null; then
            echo "Ready: $project/$(basename -- "$patch")"
            repositories+=("$repository")
            patches+=("$patch")
        elif git -C "$repository" apply --reverse --check "$patch" 2>/dev/null; then
            echo "Already applied: $project/$(basename -- "$patch")"
        else
            echo "Cannot apply patch: $patch (project: $project)" >&2
            git -C "$repository" apply --check "$patch" >&2 || true
            exit 1
        fi
    done
}

# Check every project before modifying any files. Patches remain unstaged.
check_project build/make build_make
check_project frameworks/base frameworks_base
if "$check_only"; then
    echo "All patches checked; source files unchanged."
    exit 0
fi
for ((index = 0; index < ${#patches[@]}; index++)); do
    git -C "${repositories[index]}" apply "${patches[index]}"
    echo "Applied: ${patches[index]}"
done
echo "All device patches are applied."
