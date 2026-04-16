#include "pch.h"
#include "VDTraceInternal.h"
#include "VDTraceIntegerHex.h"
#include "VDTraceStaticRefsInternal.h"

namespace vdtrace
{
    std::string FormatStaticReferenceBlock(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        std::unordered_set<uintptr_t> &analyzed_blocks,
        std::unordered_map<std::string, StaticReferenceExportEntry> *export_entries)
    {
        using namespace static_refs_detail;

        if (!event.inside_module
            || event.block_begin == 0
            || event.block_end <= event.block_begin
            || event.block_end - event.block_begin > 0x1000)
        {
            return {};
        }

        if (!analyzed_blocks.insert(event.block_begin).second)
        {
            return {};
        }

        std::vector<StaticReferenceHit> hits;
        std::unordered_set<uintptr_t> seen_addresses;
        uintptr_t cursor = event.block_begin;
        while (cursor < event.block_end)
        {
            uint8_t buffer[16] = {};
            if (!SafeReadMemoryBytes(cursor, buffer, sizeof(buffer)))
            {
                break;
            }

            ZydisDecodedInstruction instruction = {};
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
            if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&GetStaticRefDecoder(), buffer, sizeof(buffer), &instruction, operands)))
            {
                break;
            }

            for (uint8_t index = 0; index < instruction.operand_count_visible; ++index)
            {
                const ZydisDecodedOperand &operand = operands[index];
                if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY || operand.mem.base != ZYDIS_REGISTER_RIP)
                {
                    continue;
                }

                ZyanU64 absolute = 0;
                if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operand, cursor, &absolute)))
                {
                    continue;
                }

                const uintptr_t absolute_address = static_cast<uintptr_t>(absolute);
                if (!seen_addresses.insert(absolute_address).second)
                {
                    continue;
                }

                StaticReferenceHit hit = {};
                const uint8_t operand_size = static_cast<uint8_t>(std::min<ZyanU16>(operand.size / 8u, kEnhancedSampleMaxBytes));
                if (CaptureStaticReference(absolute_address, operand_size, hit))
                {
                    hits.push_back(hit);
                }
            }

            if (instruction.length == 0)
            {
                break;
            }
            cursor += instruction.length;
        }

        if (hits.empty())
        {
            return {};
        }

        std::ostringstream out;
        for (const StaticReferenceHit &hit : hits)
        {
            const std::string label_text = ResolveCachedAddressLabel(hit.address, address_label_cache);
            std::string dereference_text;

            out << "   [static] " << Narrow(std::wstring(hit.section_name))
                << " " << label_text
                << " mem=" << FormatBytes(hit.bytes, hit.size)
                << " ascii=\"" << FormatAscii(hit.bytes, hit.size) << "\""
                << " hex=" << FormatLittleEndianIntegerHex(hit.bytes, hit.size) << "\n";
            if (hit.has_dereference)
            {
                out << "   [static.ptr] -> ";
                const auto deref_cached = address_label_cache.find(hit.dereference_address);
                if (deref_cached != address_label_cache.end())
                {
                    dereference_text = deref_cached->second;
                }
                else
                {
                    dereference_text = FormatResolvedAddressLabel(hit.dereference_address);
                    address_label_cache[hit.dereference_address] = dereference_text;
                }
                out << dereference_text;
                out << " mem=" << FormatBytes(hit.dereference_bytes, hit.dereference_size)
                    << " ascii=\"" << FormatAscii(hit.dereference_bytes, hit.dereference_size) << "\""
                    << " hex=" << FormatLittleEndianIntegerHex(hit.dereference_bytes, hit.dereference_size);
                if (!hit.dereference_guess.empty())
                {
                    out << " guess=" << hit.dereference_guess;
                }
                if (!hit.dereference_detail.empty())
                {
                    out << " " << hit.dereference_detail;
                }
                out << "\n";
            }
            if (export_entries != nullptr)
            {
                AppendStaticReferenceExport(
                    event,
                    hit,
                    label_text,
                    dereference_text,
                    *export_entries,
                    address_label_cache);
            }
        }
        return out.str();
    }
}
