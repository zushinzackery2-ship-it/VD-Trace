#include "pch.h"
#include "VDTraceFileSinkFormatInternal.h"

namespace vdtrace::file_sink_format_detail
{
    bool TryFormatAnonymousExecLabel(uintptr_t address, std::string &text)
    {
        text.clear();
        if (address == 0)
        {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
        {
            return false;
        }

        const DWORD protection = mbi.Protect & 0xff;
        const bool executable = protection == PAGE_EXECUTE
            || protection == PAGE_EXECUTE_READ
            || protection == PAGE_EXECUTE_READWRITE
            || protection == PAGE_EXECUTE_WRITECOPY;
        if (mbi.State != MEM_COMMIT || mbi.Type == MEM_IMAGE || !executable)
        {
            return false;
        }

        const uintptr_t identity = reinterpret_cast<uintptr_t>(mbi.AllocationBase != nullptr ? mbi.AllocationBase : mbi.BaseAddress);
        std::ostringstream out;
        out << "anon-exec@" << HexText(identity);
        if (address > identity)
        {
            out << "+0x" << std::hex << (address - identity);
        }
        text = out.str();
        return true;
    }

    ExecutionRangeInfo DescribeExecutionRange(
        uintptr_t address,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map)
    {
        ExecutionRangeInfo info = {};
        if (address == 0)
        {
            info.text = "0x0";
            return info;
        }

        AddressLabel label = {};
        if (ResolveAddressLabel(address, label))
        {
            info.kind = ExecutionRangeKind::Module;
            info.identity = label.module_base;
            const auto cached = address_label_cache.find(address);
            if (cached != address_label_cache.end())
            {
                info.text = cached->second;
            }
            else
            {
                std::ostringstream out;
                out << Narrow(label.module_name);
                if (!label.symbol_name.empty())
                {
                    out << "!" << Narrow(label.symbol_name);
                    if (label.symbol_offset != 0)
                    {
                        out << "+0x" << std::hex << label.symbol_offset;
                    }
                }
                else
                {
                    out << "+0x" << std::hex << label.relative;
                }
                info.text = out.str();
                address_label_cache.emplace(address, info.text);
            }
            return info;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
        {
            info.text = HexText(address);
            return info;
        }

        const DWORD protection = mbi.Protect & 0xff;
        const bool executable = protection == PAGE_EXECUTE
            || protection == PAGE_EXECUTE_READ
            || protection == PAGE_EXECUTE_READWRITE
            || protection == PAGE_EXECUTE_WRITECOPY;
        if (executable)
        {
            info.kind = ExecutionRangeKind::AnonymousExecutable;
            info.identity = reinterpret_cast<uintptr_t>(mbi.AllocationBase != nullptr ? mbi.AllocationBase : mbi.BaseAddress);
            info.text = "anon-exec@" + HexText(info.identity);
        }
        else
        {
            info.kind = ExecutionRangeKind::Other;
            info.identity = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            info.text = "other@" + HexText(info.identity);
        }
        return info;
    }

    bool ShouldMarkTraceRangeJump(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        ExecutionRangeInfo &source_info,
        ExecutionRangeInfo &target_info,
        bool &outside_jump_in)
    {
        source_info = {};
        target_info = {};
        outside_jump_in = false;
        if (!event.has_target || event.target == 0)
        {
            return false;
        }

        source_info = DescribeExecutionRange(event.instruction, address_label_cache, async_probe_map);
        target_info = DescribeExecutionRange(event.target, address_label_cache, async_probe_map);
        if (source_info.identity == 0 || target_info.identity == 0)
        {
            return false;
        }

        outside_jump_in = source_info.kind != ExecutionRangeKind::Module
            && target_info.kind == ExecutionRangeKind::Module;
        return source_info.identity != target_info.identity;
    }

    std::string ResolveAddressLabelText(
        uintptr_t address,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map)
    {
        if (address == 0)
        {
            return "0x0";
        }

        const auto async_it = async_probe_map.find(address);
        if (async_it != async_probe_map.end())
        {
            return Narrow(async_it->second.module_name) + "!" + Narrow(async_it->second.symbol_name);
        }

        const auto cached = address_label_cache.find(address);
        if (cached != address_label_cache.end())
        {
            return cached->second;
        }

        AddressLabel label = {};
        if (!ResolveAddressLabel(address, label))
        {
            std::string fallback;
            if (!TryFormatAnonymousExecLabel(address, fallback))
            {
                std::ostringstream out;
                out << "0x" << std::hex << address;
                fallback = out.str();
            }
            address_label_cache.emplace(address, fallback);
            return fallback;
        }

        std::ostringstream out;
        out << Narrow(label.module_name);
        if (!label.symbol_name.empty())
        {
            out << "!" << Narrow(label.symbol_name);
            if (label.symbol_offset != 0)
            {
                out << "+0x" << std::hex << label.symbol_offset;
            }
        }
        else
        {
            out << "+0x" << std::hex << label.relative;
        }
        const std::string result = out.str();
        address_label_cache.emplace(address, result);
        return result;
    }

    std::string BuildCallSummaryText(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map)
    {
        if (event.kind != EventKind::Call || !event.has_target)
        {
            return {};
        }

        const auto async_it = async_probe_map.find(event.target);
        if (async_it != async_probe_map.end())
        {
            const ResolvedAsyncProbe &probe = async_it->second;
            std::ostringstream out;
            out << Narrow(probe.module_name)
                << "!" << Narrow(probe.symbol_name)
                << "(";
            bool need_separator = false;
            for (uint8_t index = 0; index < probe.argument_count && index < 4; index++)
            {
                const uint8_t argument_index = probe.argument_indices[index];
                const uintptr_t argument_value = ReadCallArgument(event, argument_index);
                const std::string value = probe.argument_is_pointer[index]
                    ? FormatValueWithPreview(argument_value, true, address_label_cache, async_probe_map)
                    : FormatValueWithPreview(argument_value, false, address_label_cache, async_probe_map);
                AppendNamedArgument(out, need_separator, Narrow(probe.argument_names[index]), value);
            }
            out << ")";
            return out.str();
        }

        std::ostringstream out;
        out << ResolveAddressLabelText(event.target, address_label_cache, async_probe_map)
            << "(";
        bool need_separator = false;
        const uint8_t limit = std::min<uint8_t>(event.call_argument_count, 4);
        for (uint8_t index = 0; index < limit; index++)
        {
            AppendNamedArgument(
                out,
                need_separator,
                "arg" + std::to_string(index),
                FormatValueWithPreview(ReadCallArgument(event, index), false, address_label_cache, async_probe_map));
        }
        out << ")";
        return out.str();
    }
}
