#ifndef VDTRACE_DECRYPT_SMOKE_SUPPORT_H
#define VDTRACE_DECRYPT_SMOKE_SUPPORT_H

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace decrypt_smoke_test
{
    using CryptBufferFn = BOOL(__stdcall *)(uint8_t *data, size_t size);
    using AddressFn = uintptr_t(__stdcall *)(void);

    struct HelperApi
    {
        HMODULE module = nullptr;
        std::wstring helper_name;
        uintptr_t module_base = 0;
        CryptBufferFn encrypt_buffer = nullptr;
        CryptBufferFn decrypt_buffer = nullptr;
        AddressFn get_pipeline = nullptr;
        AddressFn get_expand_round_keys = nullptr;
        AddressFn get_pre_whiten_stage = nullptr;
        AddressFn get_dispatch_dynamic_stage = nullptr;
        AddressFn get_chunk_mirror_stage = nullptr;
        AddressFn get_tail_whiten_stage = nullptr;
        AddressFn get_dynamic_stage_base = nullptr;
    };

    [[noreturn]] void Fail(const wchar_t *message);
    void Require(bool condition, const wchar_t *message);

    std::filesystem::path GetExecutableDirectory();
    std::wstring GetExecutablePath();
    std::wstring GetFilenameOnly(const std::wstring &path);
    std::filesystem::path BuildPlainPath();
    std::filesystem::path BuildEncryptedPath();
    std::filesystem::path BuildDecryptedPath();
    std::filesystem::path BuildFailurePath();
    std::wstring BuildLogPath();

    std::vector<uint8_t> BuildPlainPayload();
    bool WriteBinaryFile(const std::filesystem::path &path, const std::vector<uint8_t> &bytes);
    std::wstring ReadAllText(const std::wstring &path);
    void WaitForStableFile(const std::filesystem::path &path);
    std::wstring FormatPreviewNeedle(const std::vector<uint8_t> &bytes, size_t limit);
    std::wstring FormatHexNeedle(uintptr_t value);

    bool LoadHelperApi(HelperApi &api);
    void FreeHelperApi(HelperApi &api);
}

#endif
