#include "pch.h"
#include "autostart/VDTraceAutoStartConfigInternal.h"

#include <fstream>

namespace vdtrace::autostart::detail
{
    std::filesystem::path GetAutoStartModuleDirectory()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        if (length == 0)
        {
            return std::filesystem::current_path();
        }

        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path();
    }

    std::wstring TrimAutoStartText(const std::wstring &text)
    {
        const size_t begin = text.find_first_not_of(L" \t\r\n");
        if (begin == std::wstring::npos)
        {
            return {};
        }

        const size_t end = text.find_last_not_of(L" \t\r\n");
        return text.substr(begin, end - begin + 1);
    }

    bool ReadAutoStartUtf8TextFile(const std::filesystem::path &path, std::wstring &text)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
        {
            return false;
        }

        std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (bytes.size() >= 3
            && static_cast<unsigned char>(bytes[0]) == 0xEF
            && static_cast<unsigned char>(bytes[1]) == 0xBB
            && static_cast<unsigned char>(bytes[2]) == 0xBF)
        {
            bytes.erase(0, 3);
        }

        if (bytes.empty())
        {
            text.clear();
            return true;
        }

        int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
        if (count <= 0)
        {
            count = MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
            if (count <= 0)
            {
                return false;
            }

            text.resize(static_cast<size_t>(count));
            MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), text.data(), count);
            return true;
        }

        text.resize(static_cast<size_t>(count));
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), text.data(), count);
        return true;
    }

    bool WriteAutoStartUtf8TextFile(const std::filesystem::path &path, const std::string &text)
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            return false;
        }

        output.write(text.data(), static_cast<std::streamsize>(text.size()));
        return output.good();
    }

    std::string BuildDefaultAutoStartConfigText()
    {
        return
            "; VD-Trace autostart settings\n"
            "; tool: launch game -> wait BepInEx-equivalent timing -> load Agent -> configure/start -> wait trace end\n"
            "; wait.mode=disabled means helper loads Agent immediately after entering target process\n"
            "; call_depth is the default follow depth; outside_call_depth / anonymous_exec_call_depth / module_call_depths can override it\n"
            "; outside_execution_mode / anonymous_exec_execution_mode support EDGE or TF; module_call_depths format uses Module.dll:3:TF\n"
            "; backend supports DR / TF only\n"
            "; idle_escape_threshold controls first-hit hot empty-spin escape; 0 disables it\n"
            "[launch]\n"
            "game_path = \n"
            "working_directory = \n"
            "arguments = \n"
            "helper_path = .\\bin\\release\\VDTraceAutoStart.dll\n"
            "loader_timeout_ms = 60000\n"
            "agent_timeout_ms = 30000\n"
            "trace_start_timeout_ms = 120000\n"
            "trace_finish_timeout_ms = 1800000\n"
            "wait_for_trace_end = true\n"
            "\n"
            "[wait]\n"
            "mode = bepinex_il2cpp_scene_change\n"
            "module_name = GameAssembly.dll\n"
            "invoke_export = il2cpp_runtime_invoke\n"
            "method_name_export = il2cpp_method_get_name\n"
            "target_method_name = Internal_ActiveSceneChanged\n"
            "module_poll_interval_ms = 50\n"
            "wait_timeout_ms = 600000\n"
            "\n"
            "[trace]\n"
            "agent_path = .\\bin\\release\\VDTraceAgent.dll\n"
            "thread_id = 0\n"
            "auto_select_thread = true\n"
            "block_main_thread = false\n"
            "modules = \n"
            "output_path = .\\traces\\VDTrace.log\n"
            "max_events = 0\n"
            "backend = DR\n"
            "idle_escape_threshold = 32\n"
            "call_depth = 4\n"
            "outside_call_depth = \n"
            "outside_execution_mode = EDGE\n"
            "anonymous_exec_call_depth = \n"
            "anonymous_exec_execution_mode = EDGE\n"
            "module_call_depths = \n"
            "trigger_point = \n"
            "probe_spec = \n"
            "trigger_enabled = false\n"
            "trace_outside_modules = false\n"
            "repeat_hits = false\n"
            "enhanced_sampling = true\n"
            "root_stop_on_return = false\n"
            "async_thread_handoff = true\n";
    }

    std::string NarrowAutoStartUtf8(const std::wstring &text)
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

    bool ParseAutoStartBool(const std::wstring &text, bool fallback)
    {
        const std::wstring lowered = TrimAutoStartText(text);
        if (_wcsicmp(lowered.c_str(), L"1") == 0 || _wcsicmp(lowered.c_str(), L"true") == 0 || _wcsicmp(lowered.c_str(), L"yes") == 0 || _wcsicmp(lowered.c_str(), L"on") == 0)
        {
            return true;
        }

        if (_wcsicmp(lowered.c_str(), L"0") == 0 || _wcsicmp(lowered.c_str(), L"false") == 0 || _wcsicmp(lowered.c_str(), L"no") == 0 || _wcsicmp(lowered.c_str(), L"off") == 0)
        {
            return false;
        }

        return fallback;
    }

    bool ParseAutoStartUint64(const std::wstring &text, uint64_t &value)
    {
        const std::wstring trimmed = TrimAutoStartText(text);
        if (trimmed.empty())
        {
            return false;
        }

        wchar_t *end = nullptr;
        value = wcstoull(trimmed.c_str(), &end, 0);
        return end != trimmed.c_str() && end != nullptr && *end == L'\0';
    }

    bool ParseAutoStartCallDepth(const std::wstring &text, uint32_t &value)
    {
        const std::wstring trimmed = TrimAutoStartText(text);
        if (_wcsicmp(trimmed.c_str(), L"all") == 0)
        {
            value = kUnlimitedCallDepth;
            return true;
        }

        if (_wcsicmp(trimmed.c_str(), L"same") == 0 || _wcsicmp(trimmed.c_str(), L"single") == 0)
        {
            value = 0;
            return true;
        }

        uint64_t parsed = 0;
        if (!ParseAutoStartUint64(trimmed, parsed) || parsed > kUnlimitedCallDepth)
        {
            return false;
        }

        value = static_cast<uint32_t>(parsed);
        return true;
    }

    bool ParseAutoStartIniText(const std::wstring &text, SectionMap &sections)
    {
        sections.clear();
        std::wistringstream input(text);
        std::wstring line;
        std::wstring current_section;
        while (std::getline(input, line))
        {
            if (!line.empty() && line.back() == L'\r')
            {
                line.pop_back();
            }

            const std::wstring trimmed = TrimAutoStartText(line);
            if (trimmed.empty() || trimmed[0] == L';' || trimmed[0] == L'#')
            {
                continue;
            }

            if (trimmed.front() == L'[' && trimmed.back() == L']' && trimmed.size() > 2)
            {
                current_section = TrimAutoStartText(trimmed.substr(1, trimmed.size() - 2));
                sections[current_section];
                continue;
            }

            const size_t equal = trimmed.find(L'=');
            if (equal == std::wstring::npos || current_section.empty())
            {
                continue;
            }

            const std::wstring key = TrimAutoStartText(trimmed.substr(0, equal));
            const std::wstring value = TrimAutoStartText(trimmed.substr(equal + 1));
            sections[current_section][key] = value;
        }

        return true;
    }

    std::wstring GetAutoStartValue(const SectionMap &sections, const wchar_t *section_name, const wchar_t *key_name, const std::wstring &fallback)
    {
        const auto section_it = sections.find(section_name);
        if (section_it == sections.end())
        {
            return fallback;
        }

        const auto value_it = section_it->second.find(key_name);
        if (value_it == section_it->second.end())
        {
            return fallback;
        }

        return value_it->second;
    }

    std::filesystem::path ResolveConfigRelativePath(const std::filesystem::path &base_directory, const std::wstring &text)
    {
        std::filesystem::path result = TrimAutoStartText(text);
        if (result.empty())
        {
            return {};
        }

        if (result.is_relative())
        {
            result = base_directory / result;
        }

        return result.lexically_normal();
    }
}
