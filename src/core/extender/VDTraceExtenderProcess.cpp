#include "pch.h"
#include "core/extender/VDTraceExtenderInternal.h"

namespace vdtrace
{
    TextFileRecorderExtender::Impl::Impl(const TextFileRecorderHeapPeek &peek)
        : heap_peek(peek)
    {
    }

    bool TextFileRecorderExtender::Impl::TryBuildStaticWindowCandidate(
        const extender::ExtendedMemoryAccess &access,
        extender_detail::StaticWindowCandidate &candidate) const
    {
        candidate = {};
        const bool direct_static_read = access.access_kind == extender::ExtendedAccessKind::Memory
            && access.origin == heap_peek::HeapPeekOrigin::BlockExtendRead
            && (access.peek_size == 1 || access.peek_size == 2 || access.peek_size == 4);
        const bool static_base_address = access.access_kind == extender::ExtendedAccessKind::Address;
        if (!direct_static_read && !static_base_address)
        {
            return false;
        }

        AddressLabel label = {};
        if (!ResolveAddressLabel(access.memory_address, label) || label.module_base == 0)
        {
            return false;
        }

        uintptr_t section_begin = 0;
        uintptr_t section_end = 0;
        if (!extender_detail::ResolveSectionRange(
                label.module_base,
                access.memory_address,
                candidate.section_name,
                std::size(candidate.section_name),
                section_begin,
                section_end))
        {
            return false;
        }

        candidate.instruction = access.instruction;
        candidate.address = access.memory_address;
        candidate.width = direct_static_read ? access.peek_size : 0;
        candidate.from_address_only = static_base_address;
        candidate.window_base = access.memory_address & extender_detail::kStaticWindowMask;
        candidate.capture_begin = std::max(candidate.window_base, section_begin);
        const uintptr_t capture_end = std::min(candidate.window_base + extender_detail::kStaticWindowSize, section_end);
        if (capture_end <= candidate.capture_begin)
        {
            return false;
        }

        candidate.capture_size = static_cast<size_t>(capture_end - candidate.capture_begin);
        candidate.operand_text = access.operand_text;
        return true;
    }

    std::string TextFileRecorderExtender::Impl::ProcessEvent(const RecorderQueuedEvent &event)
    {
        std::string batch;
        std::vector<extender::ExtendedMemoryAccess> accesses;
        const auto slot_it = entry_slots.find(event.thread_id);
        const bool analyzed = slot_it != entry_slots.end()
            && slot_it->second.valid
            && slot_it->second.rip == event.block_begin
            && extender::AnalyzeBlockMemoryAccesses(event, slot_it->second.context, accesses);

        if (analyzed)
        {
            for (const auto &access : accesses)
            {
                const bool heap_like = heap_peek::IsHeapLikeAddress(access.memory_address, access.stack_pointer);
                extender_detail::StaticWindowCandidate candidate = {};
                const bool static_candidate = TryBuildStaticWindowCandidate(access, candidate);
                if (!heap_like && !static_candidate)
                {
                    continue;
                }

                batch += extender::FormatExtendedMemoryAccessLine(access);
                if (heap_like && access.origin == heap_peek::HeapPeekOrigin::BlockExtendWrite)
                {
                    batch += extender_detail::BuildExtendWriteSuffix(access);
                }
                else if (heap_like)
                {
                    heap_peek::HeapPeekRequest request = {};
                    request.sequence = access.sequence;
                    request.thread_id = access.thread_id;
                    request.kind = access.kind;
                    request.instruction = access.instruction;
                    request.memory_address = access.memory_address;
                    request.stack_pointer = access.stack_pointer;
                    request.peek_size = access.peek_size;
                    request.origin = access.origin;
                    batch += heap_peek.BuildInlineSuffix(request, access.operand_text);
                }
                batch += "\n";

                if (static_candidate)
                {
                    extender_detail::StaticWindowKey key = {};
                    key.instruction = candidate.instruction;
                    key.window_base = candidate.window_base;
                    uint32_t &count = per_instruction_static_counts[candidate.instruction];
                    if (count < extender_detail::kMaxStaticWindowsPerInstruction
                        && emitted_static_windows.insert(key).second)
                    {
                        ++count;
                        batch += extender_detail::BuildStaticWindowBlock(candidate);
                    }
                }
            }
        }

        auto &slot = entry_slots[event.thread_id];
        slot = {};
        if (event.thread_context.valid && event.thread_context.rip != 0)
        {
            slot.valid = true;
            slot.rip = event.thread_context.rip;
            slot.context = event.thread_context;
        }
        return batch;
    }
}
