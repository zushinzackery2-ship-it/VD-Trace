#include "pch.h"
#include "core/async/VDTraceAsyncSupportInternal.h"

namespace vdtrace::async_support_detail
{
    const KnownAsyncProbeSpec kKnownAsyncProbeSpecs[] = {
        {L"Kernel32.dll", "CreateThread", AsyncDispatchKind::ThreadStart, 2, {2, 3, 0xFF, 0xFF}, {true, false, false, false}, {"lpStartAddress", "lpParameter", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 5, 2},
        {L"KernelBase.dll", "CreateThread", AsyncDispatchKind::ThreadStart, 2, {2, 3, 0xFF, 0xFF}, {true, false, false, false}, {"lpStartAddress", "lpParameter", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 5, 2},
        {L"Kernel32.dll", "CreateRemoteThread", AsyncDispatchKind::ThreadStart, 2, {3, 4, 0xFF, 0xFF}, {true, false, false, false}, {"lpStartAddress", "lpParameter", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 6, 3},
        {L"KernelBase.dll", "CreateRemoteThread", AsyncDispatchKind::ThreadStart, 2, {3, 4, 0xFF, 0xFF}, {true, false, false, false}, {"lpStartAddress", "lpParameter", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 6, 3},
        {L"Kernel32.dll", "CreateRemoteThreadEx", AsyncDispatchKind::ThreadStart, 2, {3, 4, 0xFF, 0xFF}, {true, false, false, false}, {"lpStartAddress", "lpParameter", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 0xFF, 3},
        {L"KernelBase.dll", "CreateRemoteThreadEx", AsyncDispatchKind::ThreadStart, 2, {3, 4, 0xFF, 0xFF}, {true, false, false, false}, {"lpStartAddress", "lpParameter", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 0xFF, 3},
        {L"ntdll.dll", "NtCreateThreadEx", AsyncDispatchKind::ThreadStart, 2, {4, 5, 0xFF, 0xFF}, {true, false, false, false}, {"StartRoutine", "Argument", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::OutputPointerArgument, 0, 0xFF, 4},
        {L"ucrtbase.dll", "_beginthreadex", AsyncDispatchKind::ThreadStart, 2, {2, 3, 0xFF, 0xFF}, {true, false, false, false}, {"start_address", "arglist", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 5, 2},
        {L"msvcrt.dll", "_beginthreadex", AsyncDispatchKind::ThreadStart, 2, {2, 3, 0xFF, 0xFF}, {true, false, false, false}, {"start_address", "arglist", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::ReturnValue, 0xFF, 5, 2},
        {L"Kernel32.dll", "QueueUserWorkItem", AsyncDispatchKind::WorkItem, 2, {0, 1, 0xFF, 0xFF}, {true, false, false, false}, {"Function", "Context", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"KernelBase.dll", "QueueUserWorkItem", AsyncDispatchKind::WorkItem, 2, {0, 1, 0xFF, 0xFF}, {true, false, false, false}, {"Function", "Context", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"ntdll.dll", "RtlQueueWorkItem", AsyncDispatchKind::WorkItem, 2, {0, 1, 0xFF, 0xFF}, {true, false, false, false}, {"Function", "Context", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"Kernel32.dll", "TrySubmitThreadpoolCallback", AsyncDispatchKind::ThreadPool, 2, {0, 1, 0xFF, 0xFF}, {true, false, false, false}, {"pfns", "pv", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"KernelBase.dll", "TrySubmitThreadpoolCallback", AsyncDispatchKind::ThreadPool, 2, {0, 1, 0xFF, 0xFF}, {true, false, false, false}, {"pfns", "pv", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"Kernel32.dll", "CreateThreadpoolWork", AsyncDispatchKind::ThreadPool, 2, {0, 1, 0xFF, 0xFF}, {true, false, false, false}, {"pfnwk", "pv", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"KernelBase.dll", "CreateThreadpoolWork", AsyncDispatchKind::ThreadPool, 2, {0, 1, 0xFF, 0xFF}, {true, false, false, false}, {"pfnwk", "pv", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"Kernel32.dll", "QueueUserAPC", AsyncDispatchKind::Apc, 2, {0, 2, 0xFF, 0xFF}, {true, false, false, false}, {"pfnAPC", "dwData", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"KernelBase.dll", "QueueUserAPC", AsyncDispatchKind::Apc, 2, {0, 2, 0xFF, 0xFF}, {true, false, false, false}, {"pfnAPC", "dwData", nullptr, nullptr}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
        {L"ntdll.dll", "NtQueueApcThread", AsyncDispatchKind::Apc, 4, {1, 2, 3, 4}, {true, false, false, false}, {"ApcRoutine", "ApcArgument1", "ApcArgument2", "ApcArgument3"}, ResolvedAsyncProbe::ThreadHandleSource::None, 0xFF, 0xFF, 0xFF},
    };

    bool TryReadPointerValue(uintptr_t address, uintptr_t &value)
    {
        __try
        {
            value = *reinterpret_cast<const uintptr_t *>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    std::wstring WideText(const char *text)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return {};
        }

        const int count = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
        if (count <= 1)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_ACP, 0, text, -1, result.data(), count);
        if (!result.empty() && result.back() == L'\0')
        {
            result.pop_back();
        }
        return result;
    }

    std::wstring ResolveModuleFilename(HMODULE module)
    {
        if (module == nullptr)
        {
            return {};
        }

        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        if (length == 0)
        {
            return {};
        }

        buffer.resize(length);
        return std::filesystem::path(buffer).filename().wstring();
    }

    std::wstring WideUtf8(const char *text)
    {
        if (text == nullptr || text[0] == '\0')
        {
            return {};
        }

        const int count = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
        if (count <= 1)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), count);
        if (!result.empty() && result.back() == L'\0')
        {
            result.pop_back();
        }
        return result;
    }

    bool BuildExportCache(HMODULE module, ExportCache &cache)
    {
        cache = {};

        MODULEINFO info = {};
        if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        {
            return false;
        }

        const uintptr_t base = reinterpret_cast<uintptr_t>(module);
        const size_t size = static_cast<size_t>(info.SizeOfImage);
        const auto contains_rva = [size](DWORD rva, size_t bytes)
        {
            return static_cast<size_t>(rva) < size && bytes <= (size - static_cast<size_t>(rva));
        };

        if (!contains_rva(0, sizeof(IMAGE_DOS_HEADER)))
        {
            cache.initialized = true;
            return true;
        }

        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        {
            cache.initialized = true;
            return true;
        }

        const DWORD nt_rva = static_cast<DWORD>(dos->e_lfanew);
        if (!contains_rva(nt_rva, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + sizeof(WORD)))
        {
            cache.initialized = true;
            return true;
        }

        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base + nt_rva);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
        {
            cache.initialized = true;
            return true;
        }

        const auto &directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (directory.VirtualAddress == 0 || directory.Size < sizeof(IMAGE_EXPORT_DIRECTORY))
        {
            cache.initialized = true;
            return true;
        }
        if (!contains_rva(directory.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY)))
        {
            cache.initialized = true;
            return true;
        }

        const uintptr_t export_base = base + directory.VirtualAddress;
        const uintptr_t export_end = export_base + directory.Size;
        const auto *export_directory = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY *>(export_base);
        if (!contains_rva(export_directory->AddressOfFunctions, static_cast<size_t>(export_directory->NumberOfFunctions) * sizeof(DWORD))
            || !contains_rva(export_directory->AddressOfNames, static_cast<size_t>(export_directory->NumberOfNames) * sizeof(DWORD))
            || !contains_rva(export_directory->AddressOfNameOrdinals, static_cast<size_t>(export_directory->NumberOfNames) * sizeof(WORD)))
        {
            cache.initialized = true;
            return true;
        }

        const auto *functions = reinterpret_cast<const DWORD *>(base + export_directory->AddressOfFunctions);
        const auto *names = reinterpret_cast<const DWORD *>(base + export_directory->AddressOfNames);
        const auto *ordinals = reinterpret_cast<const WORD *>(base + export_directory->AddressOfNameOrdinals);

        std::unordered_set<uintptr_t> seen;
        cache.symbols.reserve(export_directory->NumberOfNames);
        for (DWORD index = 0; index < export_directory->NumberOfNames; ++index)
        {
            const DWORD name_rva = names[index];
            const WORD ordinal = ordinals[index];
            if (ordinal >= export_directory->NumberOfFunctions || !contains_rva(name_rva, 1))
            {
                continue;
            }

            const DWORD function_rva = functions[ordinal];
            const uintptr_t function_address = base + function_rva;
            if (function_address < base || function_address >= (base + size))
            {
                continue;
            }
            if (function_address >= export_base && function_address < export_end)
            {
                continue;
            }
            if (!seen.insert(function_address).second)
            {
                continue;
            }

            const char *name_text = reinterpret_cast<const char *>(base + name_rva);
            ExportSymbol symbol = {};
            symbol.address = function_address;
            symbol.name = WideUtf8(name_text);
            if (!symbol.name.empty())
            {
                cache.symbols.push_back(std::move(symbol));
            }
        }

        std::sort(
            cache.symbols.begin(),
            cache.symbols.end(),
            [](const ExportSymbol &left, const ExportSymbol &right)
            {
                return left.address < right.address;
            });
        cache.initialized = true;
        return true;
    }

    bool TryResolveExportSymbol(HMODULE module, uintptr_t address, std::wstring &symbol_name, uintptr_t &symbol_offset)
    {
        symbol_name.clear();
        symbol_offset = 0;

        static std::mutex cache_lock;
        static std::unordered_map<uintptr_t, ExportCache> caches;

        const uintptr_t module_key = reinterpret_cast<uintptr_t>(module);
        ExportCache local_cache = {};
        {
            std::lock_guard<std::mutex> lock(cache_lock);
            auto &cache = caches[module_key];
            if (!cache.initialized)
            {
                BuildExportCache(module, cache);
            }
            local_cache = cache;
        }

        if (local_cache.symbols.empty())
        {
            return false;
        }

        const auto it = std::upper_bound(
            local_cache.symbols.begin(),
            local_cache.symbols.end(),
            address,
            [](uintptr_t value, const ExportSymbol &symbol)
            {
                return value < symbol.address;
            });
        if (it == local_cache.symbols.begin())
        {
            return false;
        }

        const ExportSymbol &symbol = *(it - 1);
        if (address < symbol.address)
        {
            return false;
        }

        const uintptr_t offset = address - symbol.address;
        if (offset > 0x4000)
        {
            return false;
        }

        symbol_name = symbol.name;
        symbol_offset = offset;
        return true;
    }
}
