#include "pch.h"
#include "core/preview/VDTraceFunctionPreviewInternal.h"

namespace vdtrace::function_preview_detail
{
    void BuildPreviewInlineSuffixes(
        const RecorderQueuedEvent &event,
        const std::vector<PreviewInstruction> &instructions,
        const TextFileRecorderHeapPeek *heap_peek,
        PreviewInlineSuffixMap &suffixes)
    {
        suffixes.clear();
        if (heap_peek == nullptr || instructions.empty())
        {
            return;
        }

        ThreadContextSnapshot entry_context = {};
        if (!TryBuildPreviewEntryContext(event, entry_context))
        {
            return;
        }

        const uintptr_t block_end = FindPreviewEntryBlockEnd(event.target, instructions);
        if (block_end <= event.target)
        {
            return;
        }

        RecorderQueuedEvent preview_event = {};
        preview_event.sequence = event.sequence;
        preview_event.thread_id = event.thread_id;
        preview_event.kind = event.kind;
        preview_event.block_begin = event.target;
        preview_event.block_end = block_end;
        preview_event.stack_pointer = entry_context.rsp;

        std::vector<extender::ExtendedMemoryAccess> accesses;
        if (!extender::AnalyzeBlockMemoryAccesses(preview_event, entry_context, accesses))
        {
            return;
        }

        for (const auto &access : accesses)
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

            const std::string suffix = heap_peek->BuildInlineSuffix(request, access.operand_text);
            if (!suffix.empty())
            {
                suffixes[access.instruction].push_back(suffix);
            }
        }
    }
}
