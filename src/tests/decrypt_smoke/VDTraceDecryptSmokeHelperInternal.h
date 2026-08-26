#ifndef VDTRACE_DECRYPT_SMOKE_HELPER_INTERNAL_H
#define VDTRACE_DECRYPT_SMOKE_HELPER_INTERNAL_H

#include <Windows.h>

#include <cstddef>
#include <cstdint>

namespace decrypt_smoke_helper
{
    namespace jit_detail
    {
        using DynamicStageFn = void(__fastcall *)(uint8_t *data, size_t size, const uint32_t *round_keys, uint32_t nonce);

        bool BuildDynamicStage(uint8_t *&stage_base, DynamicStageFn &stage);
    }

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
