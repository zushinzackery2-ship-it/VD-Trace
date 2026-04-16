#include "pch.h"
#include "VDTraceFileSinkFormatInternal.h"

namespace vdtrace::file_sink_format_detail
{
    uint32_t ResolveDynamicRangeId(std::unordered_map<uintptr_t, uint32_t> &dynamic_range_ids, uintptr_t identity)
    {
        const auto it = dynamic_range_ids.find(identity);
        if (it != dynamic_range_ids.end())
        {
            return it->second;
        }

        const uint32_t next_id = static_cast<uint32_t>(dynamic_range_ids.size());
        dynamic_range_ids.emplace(identity, next_id);
        return next_id;
    }

    std::string Narrow(const std::wstring &text)
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

    std::string NarrowKind(EventKind kind)
    {
        return Narrow(EventKindName(kind));
    }

    ZydisDecoder &GetProbeDecoder()
    {
        static ZydisDecoder decoder = {};
        static bool initialized = false;
        if (!initialized)
        {
            ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
            initialized = true;
        }
        return decoder;
    }

    ZydisFormatter &GetProbeFormatter()
    {
        static ZydisFormatter formatter = {};
        static bool initialized = false;
        if (!initialized)
        {
            ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
            initialized = true;
        }
        return formatter;
    }

    std::string FormatProbeBytes(const uint8_t *bytes, uint8_t size)
    {
        std::ostringstream out;
        for (uint8_t index = 0; index < size; ++index)
        {
            if (index != 0)
            {
                out << ' ';
            }
            out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(bytes[index]);
        }
        return out.str();
    }

    std::string FormatProbeAscii(const uint8_t *bytes, uint8_t size)
    {
        std::string text;
        text.reserve(size);
        for (uint8_t index = 0; index < size; ++index)
        {
            const uint8_t value = bytes[index];
            text.push_back(value >= 0x20 && value <= 0x7e ? static_cast<char>(value) : '.');
        }
        return text;
    }

    std::string FormatSingleProbeDisasm(const RecorderQueuedEvent &event)
    {
        if (event.instruction_size == 0)
        {
            return {};
        }

        ZydisDecodedInstruction instruction = {};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &GetProbeDecoder(),
                event.instruction_bytes,
                sizeof(event.instruction_bytes),
                &instruction,
                operands)))
        {
            return {};
        }

        char text[256] = {};
        if (!ZYAN_SUCCESS(ZydisFormatterFormatInstruction(
                &GetProbeFormatter(),
                &instruction,
                operands,
                instruction.operand_count_visible,
                text,
                sizeof(text),
                event.instruction,
                nullptr)))
        {
            return {};
        }

        std::ostringstream out;
        out << "0x" << std::hex << event.instruction
            << "  " << FormatProbeBytes(event.instruction_bytes, event.instruction_size)
            << "  " << text;
        return out.str();
    }

    std::string HexText(uintptr_t value)
    {
        std::ostringstream out;
        out << "0x" << std::hex << value;
        return out.str();
    }

    std::wstring ResolveModuleFilename(uintptr_t module_base)
    {
        if (module_base == 0)
        {
            return {};
        }

        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(reinterpret_cast<HMODULE>(module_base), buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(reinterpret_cast<HMODULE>(module_base), buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        if (length == 0)
        {
            return {};
        }

        buffer.resize(length);
        return std::filesystem::path(buffer).filename().wstring();
    }

    std::string ResolveModuleName(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &module_name_cache)
    {
        if (!event.inside_module)
        {
            return "outside";
        }

        if (event.module_name[0] != L'\0')
        {
            return Narrow(event.module_name);
        }

        const auto it = module_name_cache.find(event.module_base);
        if (it != module_name_cache.end())
        {
            return it->second;
        }

        const std::string resolved = Narrow(ResolveModuleFilename(event.module_base));
        const std::string cached = resolved.empty() ? "inside" : resolved;
        module_name_cache[event.module_base] = cached;
        return cached;
    }

    uintptr_t ReadCallArgument(const RecorderQueuedEvent &event, uint8_t index)
    {
        return index < event.call_argument_count && index < 8
            ? event.call_arguments[index]
            : 0;
    }

    std::string CallIndent(uint32_t call_depth)
    {
        std::string indent;
        const uint32_t clamped_depth = std::min<uint32_t>(call_depth, 24);
        for (uint32_t index = 0; index < clamped_depth; index++)
        {
            indent += "   | ";
        }
        return indent;
    }

    void AppendNamedArgument(std::ostringstream &out, bool &need_separator, const std::string &name, const std::string &value)
    {
        if (need_separator)
        {
            out << ", ";
        }
        out << name << "=" << value;
        need_separator = true;
    }

    std::string PrefixMultiline(const std::string &text, const std::string &prefix)
    {
        if (text.empty())
        {
            return {};
        }

        std::ostringstream out;
        std::istringstream input(text);
        std::string line;
        while (std::getline(input, line))
        {
            if (line.empty())
            {
                continue;
            }

            out << prefix << line << "\n";
        }
        return out.str();
    }
}
