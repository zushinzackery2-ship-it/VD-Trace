#include "pch.h"
#include "VDTraceStaticRefsInternal.h"

namespace vdtrace::static_refs_detail
{
    std::string ResolveCachedAddressLabel(
        uintptr_t address,
        std::unordered_map<uintptr_t, std::string> &address_label_cache)
    {
        const auto cached = address_label_cache.find(address);
        if (cached != address_label_cache.end())
        {
            return cached->second;
        }

        AddressLabel label = {};
        std::string label_text;
        if (ResolveAddressLabel(address, label))
        {
            std::ostringstream label_out;
            label_out << Narrow(label.module_name);
            if (!label.symbol_name.empty())
            {
                label_out << "!" << Narrow(label.symbol_name);
                if (label.symbol_offset != 0)
                {
                    label_out << "+0x" << std::hex << label.symbol_offset;
                }
            }
            else
            {
                label_out << "+0x" << std::hex << label.relative;
            }
            label_text = label_out.str();
        }
        else
        {
            std::ostringstream label_out;
            label_out << "0x" << std::hex << address;
            label_text = label_out.str();
        }

        address_label_cache[address] = label_text;
        return label_text;
    }

    std::string BuildStaticReferenceEntryKey(
        const StaticReferenceHit &hit,
        const std::string &slot_label,
        const std::string &dereference_label)
    {
        std::ostringstream out;
        out << Narrow(std::wstring(hit.section_name))
            << "|" << slot_label
            << "|slot_mem=" << FormatBytes(hit.bytes, hit.size);
        if (hit.has_dereference)
        {
            out << "|ptr=" << dereference_label
                << "|ptr_mem=" << FormatBytes(hit.dereference_bytes, hit.dereference_size);
            if (!hit.dereference_guess.empty())
            {
                out << "|guess=" << hit.dereference_guess;
            }
            if (!hit.dereference_detail.empty())
            {
                out << "|detail=" << hit.dereference_detail;
            }
        }
        return out.str();
    }

    void AppendStaticReferenceExport(
        const RecorderQueuedEvent &event,
        const StaticReferenceHit &hit,
        const std::string &slot_label,
        const std::string &dereference_label,
        std::unordered_map<std::string, StaticReferenceExportEntry> &export_entries,
        std::unordered_map<uintptr_t, std::string> &address_label_cache)
    {
        StaticReferenceCodeLocation location = {};
        location.sequence = event.sequence;
        location.thread_id = event.thread_id;
        location.call_depth = event.call_depth;
        location.instruction = event.instruction;
        location.block_begin = event.block_begin;
        location.block_end = event.block_end;
        location.instruction_label = ResolveCachedAddressLabel(event.instruction, address_label_cache);

        const std::string key = BuildStaticReferenceEntryKey(hit, slot_label, dereference_label);
        auto [iterator, inserted] = export_entries.try_emplace(key);
        StaticReferenceExportEntry &entry = iterator->second;
        if (inserted)
        {
            entry.key = key;
            entry.section_name = Narrow(std::wstring(hit.section_name));
            entry.slot_address = hit.address;
            entry.slot_label = slot_label;
            entry.slot_size = hit.size;
            std::memcpy(entry.slot_bytes, hit.bytes, hit.size);
            entry.has_dereference = hit.has_dereference;
            entry.dereference_address = hit.dereference_address;
            entry.dereference_label = dereference_label;
            entry.dereference_size = hit.dereference_size;
            if (hit.dereference_size != 0)
            {
                std::memcpy(entry.dereference_bytes, hit.dereference_bytes, hit.dereference_size);
            }
            entry.dereference_guess = hit.dereference_guess;
            entry.dereference_detail = hit.dereference_detail;
        }
        entry.references.push_back(location);
    }
}
