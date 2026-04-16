#include "pch.h"
#include "VDTraceFunctionPreviewInternal.h"

namespace vdtrace
{
    std::string BuildFunctionPreviewText(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        const TextFileRecorderHeapPeek *heap_peek)
    {
        using namespace function_preview_detail;

        const uintptr_t entry = event.target;
        if (entry == 0)
        {
            return {};
        }

        const PreviewRange range = ResolvePreviewRange(entry);
        if (!range.valid)
        {
            return {};
        }

        std::vector<PreviewInstruction> instructions;
        instructions.reserve(kMaxPreviewInstructions);
        std::unordered_set<uintptr_t> queued_entries;
        std::unordered_set<uintptr_t> seen_instructions;
        std::deque<uintptr_t> worklist;
        worklist.push_back(entry);
        queued_entries.insert(entry);

        uintptr_t min_address = entry;
        uintptr_t max_end = entry;
        while (!worklist.empty() && instructions.size() < kMaxPreviewInstructions)
        {
            uintptr_t cursor = worklist.front();
            worklist.pop_front();
            if (!IsAddressInsidePreviewRange(range, cursor) || !IsInsidePreviewWindow(entry, cursor))
            {
                continue;
            }

            for (;;)
            {
                if (!IsAddressInsidePreviewRange(range, cursor)
                    || !IsInsidePreviewWindow(entry, cursor)
                    || seen_instructions.find(cursor) != seen_instructions.end())
                {
                    break;
                }

                uint8_t bytes[16] = {};
                if (!SafeReadMemoryBytes(cursor, bytes, sizeof(bytes)))
                {
                    break;
                }

                ZydisDecodedInstruction instruction = {};
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
                if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&GetPreviewDecoder(), bytes, sizeof(bytes), &instruction, operands)))
                {
                    break;
                }

                char text[256] = {};
                if (!ZYAN_SUCCESS(ZydisFormatterFormatInstruction(
                        &GetPreviewFormatter(),
                        &instruction,
                        operands,
                        instruction.operand_count_visible,
                        text,
                        sizeof(text),
                        cursor,
                        nullptr)))
                {
                    break;
                }

                PreviewInstruction preview = {};
                preview.address = cursor;
                preview.size = instruction.length;
                std::memcpy(preview.bytes, bytes, sizeof(preview.bytes));
                preview.text = text;
                instructions.push_back(std::move(preview));
                seen_instructions.insert(cursor);
                min_address = std::min(min_address, cursor);
                max_end = std::max(max_end, cursor + instruction.length);

                const bool is_conditional_branch =
                    instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE
                    && instruction.mnemonic != ZYDIS_MNEMONIC_JMP
                    && instruction.mnemonic != ZYDIS_MNEMONIC_JMPABS
                    && instruction.mnemonic != ZYDIS_MNEMONIC_CALL;

                if (is_conditional_branch)
                {
                    uintptr_t target = 0;
                    if (TryResolveDirectTarget(cursor, instruction, operands, target)
                        && IsAddressInsidePreviewRange(range, target)
                        && IsInsidePreviewWindow(entry, target)
                        && queued_entries.insert(target).second)
                    {
                        worklist.push_back(target);
                    }

                    const uintptr_t fallthrough = cursor + instruction.length;
                    if (IsAddressInsidePreviewRange(range, fallthrough)
                        && IsInsidePreviewWindow(entry, fallthrough)
                        && queued_entries.insert(fallthrough).second)
                    {
                        worklist.push_back(fallthrough);
                    }
                    break;
                }

                if (instruction.mnemonic == ZYDIS_MNEMONIC_JMP || instruction.mnemonic == ZYDIS_MNEMONIC_JMPABS)
                {
                    uintptr_t target = 0;
                    if (TryResolveDirectTarget(cursor, instruction, operands, target)
                        && IsAddressInsidePreviewRange(range, target)
                        && IsInsidePreviewWindow(entry, target)
                        && queued_entries.insert(target).second)
                    {
                        worklist.push_back(target);
                    }
                    break;
                }

                if (IsPreviewTerminator(instruction))
                {
                    break;
                }

                cursor += instruction.length;
                if (instructions.size() >= kMaxPreviewInstructions)
                {
                    break;
                }
            }
        }

        if (instructions.empty())
        {
            return {};
        }

        std::sort(
            instructions.begin(),
            instructions.end(),
            [](const PreviewInstruction &left, const PreviewInstruction &right)
            {
                return left.address < right.address;
            });

        PreviewInlineSuffixMap inline_suffixes;
        BuildPreviewInlineSuffixes(event, instructions, heap_peek, inline_suffixes);

        std::ostringstream out;
        out << "[fn] " << ResolveAddressLabelText(entry, address_label_cache, async_probe_map)
            << " range=0x" << std::hex << min_address
            << "-0x" << std::hex << (max_end == 0 ? 0 : max_end - 1)
            << " size=0x" << std::hex << (max_end - min_address)
            << "\n";

        out << "    [bytes]\n";
        const size_t dump_size = std::min<size_t>(max_end - min_address, kMaxPreviewDumpBytes);
        std::vector<uint8_t> dump_buffer(dump_size);
        if (dump_size != 0 && SafeReadMemoryBytes(min_address, dump_buffer.data(), dump_size))
        {
            for (size_t offset = 0; offset < dump_size; offset += 16)
            {
                const size_t line_size = std::min<size_t>(16, dump_size - offset);
                out << "        0x" << std::hex << (min_address + offset)
                    << "  " << std::left << std::setw(47)
                    << FormatInstructionBytes(dump_buffer.data() + offset, static_cast<uint8_t>(line_size))
                    << "\n";
            }
        }

        out << "    [disasm]\n";
        for (const PreviewInstruction &instruction : instructions)
        {
            out << "        0x" << std::hex << instruction.address
                << "  " << std::left << std::setw(47) << FormatInstructionBytes(instruction.bytes, instruction.size)
                << "  " << instruction.text;
            const auto suffix_it = inline_suffixes.find(instruction.address);
            if (suffix_it != inline_suffixes.end())
            {
                for (const std::string &suffix : suffix_it->second)
                {
                    out << suffix;
                }
            }
            out << "\n";
        }

        return out.str();
    }
}
