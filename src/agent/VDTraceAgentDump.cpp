#include "pch.h"
#include "VDTrace/VDTraceIpc.h"
#include "agent/VDTraceAgentDump.h"
#include "agent/VDTracePeLayout.h"

#include <fstream>
#include <TlHelp32.h>

namespace vdtrace::agent
{
    namespace
    {
        struct ModuleInfo
        {
            std::wstring name;
            std::wstring path;
            uintptr_t base = 0;
            size_t size = 0;
        };

        std::wstring NarrowPathToFilename(const std::wstring &path)
        {
            return std::filesystem::path(path).filename().wstring();
        }

        std::string NarrowUtf8(const std::wstring &text)
        {
            if (text.empty())
            {
                return {};
            }

            const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (count <= 1)
            {
                return {};
            }

            std::string result(static_cast<size_t>(count), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
            if (!result.empty() && result.back() == '\0')
            {
                result.pop_back();
            }
            return result;
        }

        std::filesystem::path GetModulePathFromAddress(const void *address)
        {
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(address),
                    &module))
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
            return std::filesystem::path(buffer);
        }

        std::filesystem::path GetAgentModuleDirectory()
        {
            const auto module_path = GetModulePathFromAddress(reinterpret_cast<const void *>(&BuildLoadedModuleList));
            if (module_path.empty())
            {
                return std::filesystem::current_path();
            }

            return module_path.parent_path();
        }

        bool IsSystemPath(const std::wstring &path)
        {
            wchar_t windows_directory[MAX_PATH] = {};
            const UINT length = GetWindowsDirectoryW(windows_directory, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
            {
                return false;
            }

            const std::wstring windows_root = std::filesystem::path(windows_directory).lexically_normal().wstring();
            const std::wstring normalized = std::filesystem::path(path).lexically_normal().wstring();
            return normalized.size() >= windows_root.size()
                && _wcsnicmp(normalized.c_str(), windows_root.c_str(), windows_root.size()) == 0;
        }

        bool IsReadableProtection(DWORD protection)
        {
            if ((protection & PAGE_GUARD) != 0 || (protection & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD basic = protection & 0xff;
            return basic == PAGE_READONLY
                || basic == PAGE_READWRITE
                || basic == PAGE_WRITECOPY
                || basic == PAGE_EXECUTE_READ
                || basic == PAGE_EXECUTE_READWRITE
                || basic == PAGE_EXECUTE_WRITECOPY;
        }

        bool EnumerateModules(bool include_system_modules, std::vector<ModuleInfo> &modules, std::wstring &error)
        {
            modules.clear();
            error.clear();

            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
            if (snapshot == INVALID_HANDLE_VALUE)
            {
                error = L"创建模块快照失败。";
                return false;
            }

            MODULEENTRY32W entry = {};
            entry.dwSize = sizeof(entry);
            if (!Module32FirstW(snapshot, &entry))
            {
                CloseHandle(snapshot);
                error = L"枚举模块失败。";
                return false;
            }

            do
            {
                ModuleInfo info = {};
                info.name = entry.szModule;
                info.path = entry.szExePath;
                info.base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                info.size = static_cast<size_t>(entry.modBaseSize);
                if ((!include_system_modules && IsSystemPath(info.path)) || info.name.empty() || info.base == 0 || info.size == 0)
                {
                    continue;
                }

                modules.push_back(std::move(info));
            } while (Module32NextW(snapshot, &entry));

            CloseHandle(snapshot);

            std::sort(
                modules.begin(),
                modules.end(),
                [](const ModuleInfo &left, const ModuleInfo &right)
                {
                    return _wcsicmp(left.name.c_str(), right.name.c_str()) < 0;
                });
            return !modules.empty();
        }

        bool CopyModuleImage(const ModuleInfo &module, std::vector<std::uint8_t> &bytes, std::wstring &error)
        {
            bytes.assign(module.size, 0);
            size_t offset = 0;
            while (offset < module.size)
            {
                MEMORY_BASIC_INFORMATION information = {};
                const auto current = reinterpret_cast<const std::uint8_t *>(module.base + offset);
                if (VirtualQuery(current, &information, sizeof(information)) == 0)
                {
                    error = L"查询模块内存布局失败。";
                    return false;
                }

                const uintptr_t region_base = reinterpret_cast<uintptr_t>(information.BaseAddress);
                const size_t region_offset = static_cast<size_t>((module.base + offset) - region_base);
                const size_t region_remaining = information.RegionSize > region_offset ? information.RegionSize - region_offset : 0;
                const size_t chunk_size = std::min(module.size - offset, region_remaining);
                if (chunk_size == 0 || information.State != MEM_COMMIT || !IsReadableProtection(information.Protect))
                {
                    error = L"模块存在不可读页，无法完整 dump。";
                    return false;
                }

                __try
                {
                    std::memcpy(bytes.data() + offset, current, chunk_size);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    error = L"读取模块镜像时发生异常。";
                    return false;
                }

                offset += chunk_size;
            }

            return true;
        }

        bool WriteBinaryFile(const std::filesystem::path &path, const std::vector<std::uint8_t> &bytes)
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

        std::filesystem::path BuildDumpPath(const std::filesystem::path &output_directory, const std::wstring &module_name, const std::wstring &suffix)
        {
            const std::filesystem::path file_name = std::filesystem::path(module_name).filename();
            const std::wstring stem = file_name.stem().wstring();
            const std::wstring extension = file_name.extension().wstring();
            if (!stem.empty())
            {
                return output_directory / (stem + suffix + extension);
            }

            return output_directory / (file_name.wstring() + suffix);
        }
    }

    bool BuildLoadedModuleList(bool include_system_modules, std::string &message)
    {
        std::vector<ModuleInfo> modules;
        std::wstring error;
        if (!EnumerateModules(include_system_modules, modules, error))
        {
            message = error.empty() ? "failed to enumerate modules" : NarrowUtf8(error);
            return false;
        }

        std::ostringstream stream;
        for (size_t index = 0; index < modules.size(); ++index)
        {
            const std::string name = NarrowUtf8(modules[index].name);
            const size_t next_size = name.size() + (index != 0 ? 1 : 0);
            if (stream.tellp() >= 0
                && static_cast<size_t>(stream.tellp()) + next_size >= (kIpcMessageCapacity - 1))
            {
                break;
            }

            if (index != 0)
            {
                stream << '\n';
            }
            stream << name;
        }

        message = stream.str();
        return true;
    }

    bool DumpModuleToDirectory(const std::wstring &module_name, const std::wstring &output_directory, std::string &message)
    {
        std::vector<ModuleInfo> modules;
        std::wstring error;
        if (!EnumerateModules(true, modules, error))
        {
            message = error.empty() ? "failed to enumerate modules" : NarrowUtf8(error);
            return false;
        }

        const auto it = std::find_if(
            modules.begin(),
            modules.end(),
            [&](const ModuleInfo &module)
            {
                return _wcsicmp(module.name.c_str(), module_name.c_str()) == 0
                    || _wcsicmp(NarrowPathToFilename(module.path).c_str(), module_name.c_str()) == 0;
            });
        if (it == modules.end())
        {
            message = "target module is not loaded";
            return false;
        }

        std::filesystem::path output_root = output_directory.empty() ? std::filesystem::path(L".\\dump") : std::filesystem::path(output_directory);
        if (output_root.is_relative())
        {
            output_root = GetAgentModuleDirectory() / output_root;
        }

        std::error_code create_error;
        std::filesystem::create_directories(output_root, create_error);
        if (create_error)
        {
            message = "failed to create dump output directory";
            return false;
        }

        std::vector<std::uint8_t> raw_image_bytes;
        if (!CopyModuleImage(*it, raw_image_bytes, error))
        {
            message = NarrowUtf8(error);
            return false;
        }

        std::vector<std::uint8_t> fixed_image_bytes = raw_image_bytes;
        if (!NormalizeMemoryDumpPeLayout(fixed_image_bytes))
        {
            message = "failed to normalize dumped image layout";
            return false;
        }

        const std::filesystem::path raw_dump_path = BuildDumpPath(output_root, it->name, L"_dump_raw");
        const std::filesystem::path fixed_dump_path = BuildDumpPath(output_root, it->name, L"_dump_fix");
        if (!WriteBinaryFile(raw_dump_path, raw_image_bytes) || !WriteBinaryFile(fixed_dump_path, fixed_image_bytes))
        {
            message = "failed to write dump output files";
            return false;
        }

        std::ostringstream stream;
        stream << "dump complete ";
        stream << "dir=" << NarrowUtf8(output_root.wstring()) << " ";
        stream << "fixed=" << NarrowUtf8(fixed_dump_path.filename().wstring()) << " ";
        stream << "raw=" << NarrowUtf8(raw_dump_path.filename().wstring()) << " ";
        stream << "fixed_size=" << fixed_image_bytes.size() << "\n";
        stream << "raw_size=" << raw_image_bytes.size();
        message = stream.str();
        return true;
    }
}
