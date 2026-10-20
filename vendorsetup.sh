#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
if [[ -f vendor/lineage/vars/aosp_target_release ]]; then
    source vendor/lineage/vars/aosp_target_release
fi

# Keep source-built userspace and build integration dependencies consistent.
_m2391_top="${ANDROID_BUILD_TOP:-$PWD}"
for _m2391_spec in \
    "hardware/qcom-caf/common:hardware-qcom-caf-common-audio-multilib.patch" \
    "hardware/qcom-caf/sm8550/audio/agm:hardware-qcom-caf-sm8550-audio-agm-portability.patch"; do
    _m2391_repo="${_m2391_spec%%:*}"
    _m2391_patch="${_m2391_top}/device/meizu/m2391/patches/${_m2391_spec#*:}"
    if [[ -f "${_m2391_patch}" && -d "${_m2391_repo}" ]]; then
        if git -C "${_m2391_repo}" apply --check "${_m2391_patch}" 2>/dev/null; then
            git -C "${_m2391_repo}" apply "${_m2391_patch}" || return 1
        elif ! git -C "${_m2391_repo}" apply --reverse --check "${_m2391_patch}" 2>/dev/null; then
            echo "m2391: source patch needs rebasing: ${_m2391_patch}" >&2
            return 1
        fi
    fi
done
unset _m2391_top _m2391_spec _m2391_repo _m2391_patch
