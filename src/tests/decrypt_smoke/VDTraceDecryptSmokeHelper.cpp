#include "pch.h"
#include "tests/decrypt_smoke/VDTraceDecryptSmokeHelperInternal.h"

extern "C"
{
    __declspec(dllexport) BOOL __stdcall DecryptSmokeEncryptBuffer(uint8_t *data, size_t size)
    {
        return decrypt_smoke_helper::EncryptBuffer(data, size);
    }

    __declspec(dllexport) BOOL __stdcall DecryptSmokeDecryptBuffer(uint8_t *data, size_t size)
    {
        return decrypt_smoke_helper::DecryptBuffer(data, size);
    }

    __declspec(dllexport) uintptr_t __stdcall GetDecryptSmokePipelineAddress(void)
    {
        return decrypt_smoke_helper::GetPipelineAddress();
    }

    __declspec(dllexport) uintptr_t __stdcall GetDecryptSmokeExpandRoundKeysAddress(void)
    {
        return decrypt_smoke_helper::GetExpandRoundKeysAddress();
    }

    __declspec(dllexport) uintptr_t __stdcall GetDecryptSmokePreWhitenStageAddress(void)
    {
        return decrypt_smoke_helper::GetPreWhitenStageAddress();
    }

    __declspec(dllexport) uintptr_t __stdcall GetDecryptSmokeDispatchDynamicStageAddress(void)
    {
        return decrypt_smoke_helper::GetDispatchDynamicStageAddress();
    }

    __declspec(dllexport) uintptr_t __stdcall GetDecryptSmokeChunkMirrorStageAddress(void)
    {
        return decrypt_smoke_helper::GetChunkMirrorStageAddress();
    }

    __declspec(dllexport) uintptr_t __stdcall GetDecryptSmokeTailWhitenStageAddress(void)
    {
        return decrypt_smoke_helper::GetTailWhitenStageAddress();
    }

    __declspec(dllexport) uintptr_t __stdcall GetDecryptSmokeDynamicStageBaseAddress(void)
    {
        return decrypt_smoke_helper::GetDynamicStageBaseAddress();
    }
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        decrypt_smoke_helper::CleanupDynamicStage();
    }

    return TRUE;
}
