#include "pch.h"
#include "core/runtime/VDTraceInternal.h"
#include "core/support/VDTraceIntegerHex.h"

#include <filesystem>

namespace vdtrace
{
    namespace
    {
        std::string EscapeJson(const std::string &text)
        {
            std::ostringstream out;
            for (unsigned char value : text)
            {
                switch (value)
                {
                case '\\':
                    out << "\\\\";
                    break;

                case '"':
                    out << "\\\"";
                    break;

                case '\b':
                    out << "\\b";
                    break;

                case '\f':
                    out << "\\f";
                    break;

                case '\n':
                    out << "\\n";
                    break;

                case '\r':
                    out << "\\r";
                    break;

                case '\t':
                    out << "\\t";
                    break;

                default:
                    if (value < 0x20)
                    {
                        out << "\\u"
                            << std::hex
                            << std::setw(4)
                            << std::setfill('0')
                            << static_cast<unsigned>(value)
                            << std::dec;
                    }
                    else
                    {
                        out << static_cast<char>(value);
                    }
                    break;
                }
            }
            return out.str();
        }

        std::string FormatBytes(const uint8_t *bytes, uint8_t size)
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

        std::string FormatAscii(const uint8_t *bytes, uint8_t size)
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

        std::string HexValue(uintptr_t value)
        {
            std::ostringstream out;
            out << "0x" << std::hex << value;
            return out.str();
        }
    }

    std::wstring BuildStaticReferenceJsonPath(const std::wstring &trace_path)
    {
        const std::filesystem::path path(trace_path);
        return (path.parent_path() / (path.stem().wstring() + L".static_refs.json")).wstring();
    }

    std::string RenderStaticReferenceJson(const std::unordered_map<std::string, StaticReferenceExportEntry> &entries)
    {
        std::vector<const StaticReferenceExportEntry *> ordered_entries;
        ordered_entries.reserve(entries.size());
        for (const auto &item : entries)
        {
            ordered_entries.push_back(&item.second);
        }

        std::sort(
            ordered_entries.begin(),
            ordered_entries.end(),
            [](const StaticReferenceExportEntry *left, const StaticReferenceExportEntry *right)
            {
                if (left->slot_address != right->slot_address)
                {
                    return left->slot_address < right->slot_address;
                }
                return left->key < right->key;
            });

        std::ostringstream out;
        out << "{\n";
        for (size_t index = 0; index < ordered_entries.size(); ++index)
        {
            const StaticReferenceExportEntry &entry = *ordered_entries[index];
            out << "  \"" << EscapeJson(entry.key) << "\": {\n";
            out << "    \"section\": \"" << EscapeJson(entry.section_name) << "\",\n";
            out << "    \"slot_address\": \"" << EscapeJson(HexValue(entry.slot_address)) << "\",\n";
            out << "    \"slot_label\": \"" << EscapeJson(entry.slot_label) << "\",\n";
            out << "    \"slot_bytes\": \"" << EscapeJson(FormatBytes(entry.slot_bytes, entry.slot_size)) << "\",\n";
            out << "    \"slot_ascii\": \"" << EscapeJson(FormatAscii(entry.slot_bytes, entry.slot_size)) << "\",\n";
            out << "    \"slot_hex\": \"" << EscapeJson(FormatLittleEndianIntegerHex(entry.slot_bytes, entry.slot_size)) << "\",\n";
            out << "    \"has_dereference\": " << (entry.has_dereference ? "true" : "false") << ",\n";
            if (entry.has_dereference)
            {
                out << "    \"dereference_address\": \"" << EscapeJson(HexValue(entry.dereference_address)) << "\",\n";
                out << "    \"dereference_label\": \"" << EscapeJson(entry.dereference_label) << "\",\n";
                out << "    \"dereference_bytes\": \"" << EscapeJson(FormatBytes(entry.dereference_bytes, entry.dereference_size)) << "\",\n";
                out << "    \"dereference_ascii\": \"" << EscapeJson(FormatAscii(entry.dereference_bytes, entry.dereference_size)) << "\",\n";
                out << "    \"dereference_hex\": \"" << EscapeJson(FormatLittleEndianIntegerHex(entry.dereference_bytes, entry.dereference_size)) << "\",\n";
                out << "    \"guess\": \"" << EscapeJson(entry.dereference_guess) << "\",\n";
                out << "    \"detail\": \"" << EscapeJson(entry.dereference_detail) << "\",\n";
            }
            out << "    \"references\": [\n";
            for (size_t reference_index = 0; reference_index < entry.references.size(); ++reference_index)
            {
                const StaticReferenceCodeLocation &reference = entry.references[reference_index];
                out << "      {\n";
                out << "        \"sequence\": " << reference.sequence << ",\n";
                out << "        \"thread_id\": " << reference.thread_id << ",\n";
                out << "        \"call_depth\": " << reference.call_depth << ",\n";
                out << "        \"instruction\": \"" << EscapeJson(HexValue(reference.instruction)) << "\",\n";
                out << "        \"instruction_label\": \"" << EscapeJson(reference.instruction_label) << "\",\n";
                out << "        \"block_begin\": \"" << EscapeJson(HexValue(reference.block_begin)) << "\",\n";
                out << "        \"block_end\": \"" << EscapeJson(HexValue(reference.block_end)) << "\"\n";
                out << "      }";
                if (reference_index + 1 != entry.references.size())
                {
                    out << ",";
                }
                out << "\n";
            }
            out << "    ]\n";
            out << "  }";
            if (index + 1 != ordered_entries.size())
            {
                out << ",";
            }
            out << "\n";
        }
        out << "}\n";
        return out.str();
    }
}
