// SPDX-License-Identifier: Apache-2.0
#include <C2FenceFactory.h>

// Keep the real C++ return type: C2Fence is returned through a hidden argument,
// whose calling convention differs between ARM32 and ARM64.
C2Fence createSyncFenceCompat(int fenceFd)
        asm("_ZN15_C2FenceFactory15CreateSyncFenceEi");

C2Fence createSyncFenceCompat(int fenceFd) {
    return _C2FenceFactory::CreateSyncFence(fenceFd, true);
}
