#include "tests/decrypt_smoke/vdtrace_decrypt_smoke_support.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace decrypt_smoke_test
{
    [[noreturn]] void Fail(const wchar_t *message)
    {
        std::ofstream output(BuildFailurePath(), std::ios::binary | std::ios::trunc);
        if (output.is_open())
        {
            const int count = WideCharToMultiByte(CP_UTF8, 0, message, -1, nullptr, 0, nullptr, nullptr);
            if (count > 1)
            {
                std::string utf8(static_cast<size_t>(count - 1), '\0');
                WideCharToMultiByte(CP_UTF8, 0, message, -1, utf8.data(), count, nullptr, nullptr);
                output.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
            }
        }
        std::wcerr << message << L"\n";
        std::exit(1);
    }

    void Require(bool condition, const wchar_t *message)
    {
        if (!condition)
        {
            Fail(message);
        }
    }

    std::wstring GetExecutablePath()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        }
        buffer.resize(length);
        return buffer;
    }

    std::filesystem::path GetExecutableDirectory()
    {
        return std::filesystem::path(GetExecutablePath()).parent_path();
    }

    std::wstring GetFilenameOnly(const std::wstring &path)
    {
        return std::filesystem::path(path).filename().wstring();
    }

    std::filesystem::path BuildPlainPath()
    {
        return GetExecutableDirectory() / L"VDTraceDecryptSmoke-plain.bin";
    }

    std::filesystem::path BuildEncryptedPath()
    {
        return GetExecutableDirectory() / L"VDTraceDecryptSmoke-encrypted.bin";
    }

    std::filesystem::path BuildDecryptedPath()
    {
        return GetExecutableDirectory() / L"VDTraceDecryptSmoke-decrypted.bin";
    }

    std::filesystem::path BuildFailurePath()
    {
        return GetExecutableDirectory() / L"VDTraceDecryptSmoke-failure.txt";
    }

    std::wstring BuildLogPath()
    {
        return (GetExecutableDirectory() / L"VDTraceDecryptSmoke.log").wstring();
    }

    std::vector<uint8_t> BuildPlainPayload()
    {
        static const char payload[] =
            "VDTrace-Dynamic-Decrypt-Smoke-AnonymousStage-OK\n"
            "stage=bootstrap;module=helper;mode=dynamic_exec;result=stable\n"
            "payload=The quick brown fox jumps over the lazy dog twice.\n";
        return std::vector<uint8_t>(payload, payload + sizeof(payload) - 1u);
    }

    bool WriteBinaryFile(const std::filesystem::path &path, const std::vector<uint8_t> &bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
        {
            return false;
        }
        if (!bytes.empty())
        {
            stream.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        }
        return stream.good();
    }

    std::wstring ReadAllText(const std::wstring &path)
    {
        std::ifstream input(std::filesystem::path(path), std::ios::binary);
        if (!input.is_open())
        {
            return {};
        }

        const std::string data((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (data.empty())
        {
            return {};
        }

        const int count = MultiByteToWideChar(CP_UTF8, 0, data.data(), static_cast<int>(data.size()), nullptr, 0);
        if (count <= 0)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, data.data(), static_cast<int>(data.size()), result.data(), count);
        return result;
    }

    void WaitForStableFile(const std::filesystem::path &path)
    {
        const ULONGLONG deadline = GetTickCount64() + 5000u;
        uintmax_t previous_size = 0;
        while (GetTickCount64() < deadline)
        {
            if (std::filesystem::exists(path))
            {
                const uintmax_t current_size = std::filesystem::file_size(path);
                if (current_size != 0 && current_size == previous_size)
                {
                    return;
                }
                previous_size = current_size;
            }
            Sleep(50);
        }
    }

    std::wstring FormatPreviewNeedle(const std::vector<uint8_t> &bytes, size_t limit)
    {
        std::wostringstream stream;
        const size_t count = std::min(limit, bytes.size());
        for (size_t index = 0; index < count; ++index)
        {
            if (index != 0)
            {
                stream << L' ';
            }
            stream << std::setw(2) << std::setfill(L'0') << std::hex << static_cast<unsigned>(bytes[index]);
        }
        return stream.str();
    }

    std::wstring FormatHexNeedle(uintptr_t value)
    {
        std::wostringstream stream;
        stream << L"0x" << std::hex << value;
        return stream.str();
    }

    bool LoadHelperApi(HelperApi &api)
    {
        api = {};
        api.helper_name = L"VDTraceDecryptSmokeHelper.dll";
        const std::filesystem::path helper_path = GetExecutableDirectory() / api.helper_name;
        api.module = LoadLibraryW(helper_path.c_str());
        if (api.module == nullptr)
        {
            return false;
        }

        api.module_base = reinterpret_cast<uintptr_t>(api.module);
        api.encrypt_buffer = reinterpret_cast<CryptBufferFn>(GetProcAddress(api.module, "DecryptSmokeEncryptBuffer"));
        api.decrypt_buffer = reinterpret_cast<CryptBufferFn>(GetProcAddress(api.module, "DecryptSmokeDecryptBuffer"));
        api.get_pipeline = reinterpret_cast<AddressFn>(GetProcAddress(api.module, "GetDecryptSmokePipelineAddress"));
        api.get_expand_round_keys = reinterpret_cast<AddressFn>(GetProcAddress(api.module, "GetDecryptSmokeExpandRoundKeysAddress"));
        api.get_pre_whiten_stage = reinterpret_cast<AddressFn>(GetProcAddress(api.module, "GetDecryptSmokePreWhitenStageAddress"));
        api.get_dispatch_dynamic_stage = reinterpret_cast<AddressFn>(GetProcAddress(api.module, "GetDecryptSmokeDispatchDynamicStageAddress"));
        api.get_chunk_mirror_stage = reinterpret_cast<AddressFn>(GetProcAddress(api.module, "GetDecryptSmokeChunkMirrorStageAddress"));
        api.get_tail_whiten_stage = reinterpret_cast<AddressFn>(GetProcAddress(api.module, "GetDecryptSmokeTailWhitenStageAddress"));
        api.get_dynamic_stage_base = reinterpret_cast<AddressFn>(GetProcAddress(api.module, "GetDecryptSmokeDynamicStageBaseAddress"));
        return api.encrypt_buffer != nullptr
            && api.decrypt_buffer != nullptr
            && api.get_pipeline != nullptr
            && api.get_expand_round_keys != nullptr
            && api.get_pre_whiten_stage != nullptr
            && api.get_dispatch_dynamic_stage != nullptr
            && api.get_chunk_mirror_stage != nullptr
            && api.get_tail_whiten_stage != nullptr
            && api.get_dynamic_stage_base != nullptr;
    }

    void FreeHelperApi(HelperApi &api)
    {
        if (api.module != nullptr)
        {
            FreeLibrary(api.module);
        }
        api = {};
    }
}
