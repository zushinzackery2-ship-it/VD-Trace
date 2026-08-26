#include "pch.h"
#include "VDTrace/VDTraceIpc.h"
#include "agent/VDTraceAgentDump.h"
#include "agent/VDTraceAgentDumpInternal.h"
#include "agent/VDTracePeLayout.h"

namespace vdtrace::agent
{
    using namespace dump_detail;

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
