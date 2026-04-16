#ifndef VDTRACE_DECRYPT_SMOKE_HELPER_INTERNAL_H
#define VDTRACE_DECRYPT_SMOKE_HELPER_INTERNAL_H

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace decrypt_smoke_helper
{
    BOOL EncryptBuffer(uint8_t *data, size_t size);
    BOOL DecryptBuffer(uint8_t *data, size_t size);

    uintptr_t GetPipelineAddress();
    uintptr_t GetExpandRoundKeysAddress();
    uintptr_t GetPreWhitenStageAddress();
    uintptr_t GetDispatchDynamicStageAddress();
    uintptr_t GetChunkMirrorStageAddress();
    uintptr_t GetTailWhitenStageAddress();
    uintptr_t GetDynamicStageBaseAddress();

    void CleanupDynamicStage();
}

#endif
