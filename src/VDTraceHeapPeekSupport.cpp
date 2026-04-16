#include "pch.h"
#include "VDTraceHeapPeekSupportInternal.h"

namespace vdtrace
{
    namespace heap_peek
    {
        bool IsHeapLikeAddress(uintptr_t address, uintptr_t stack_pointer)
        {
            MEMORY_BASIC_INFORMATION info = {};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0
                || info.State != MEM_COMMIT
                || info.Type != MEM_PRIVATE
                || (info.Protect & PAGE_GUARD) != 0
                || (info.Protect & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            if (stack_pointer == 0)
            {
                return true;
            }

            MEMORY_BASIC_INFORMATION stack_info = {};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(stack_pointer), &stack_info, sizeof(stack_info)) != 0)
            {
                const uintptr_t region_base = reinterpret_cast<uintptr_t>(
                    info.AllocationBase != nullptr ? info.AllocationBase : info.BaseAddress);
                const uintptr_t stack_base = reinterpret_cast<uintptr_t>(
                    stack_info.AllocationBase != nullptr ? stack_info.AllocationBase : stack_info.BaseAddress);
                if (region_base == stack_base)
                {
                    return false;
                }
            }

            return true;
        }

        uint8_t ResolveHeapPeekSize(
            const ZydisDecodedInstruction &instruction,
            const ZydisDecodedOperand *operands,
            uint8_t operand_index,
            bool address_only)
        {
            if (address_only)
            {
                return 16;
            }

            const ZydisDecodedOperand &operand = operands[operand_index];
            uint8_t size = detail::ClampPeekSizeBits(operand.size);
            if (size != 0)
            {
                return size;
            }

            for (uint8_t index = 0; index < instruction.operand_count_visible; ++index)
            {
                if (index == operand_index)
                {
                    continue;
                }

                const ZydisDecodedOperand &candidate = operands[index];
                if (candidate.type != ZYDIS_OPERAND_TYPE_REGISTER)
                {
                    continue;
                }

                size = std::max(size, detail::RegisterPeekSizeBytes(candidate.reg.value));
            }

            if (size != 0)
            {
                return size;
            }

            size = detail::ClampPeekSizeBits(instruction.operand_width);
            if (size != 0)
            {
                return size;
            }

            return 16;
        }

        bool DecodeHeapPeekInstruction(
            const uint8_t *bytes,
            size_t size,
            ZydisDecodedInstruction &instruction,
            ZydisDecodedOperand *operands)
        {
            return ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &detail::GetHeapPeekDecoder(),
                bytes,
                size,
                &instruction,
                operands));
        }

        std::string BuildDirectEventHeapPeekInlineSuffix(const RecorderQueuedEvent &event)
        {
            HeapPeekRequest request = {};
            std::string operand_text;
            if (!detail::TryBuildHeapPeekRequestCore(event, request, &operand_text))
            {
                return {};
            }

            return BuildHeapPeekInlineSuffix(request, operand_text);
        }

        std::string BuildHeapPeekInlineSuffix(const HeapPeekRequest &request, const std::string &operand_text)
        {
            if (request.memory_address == 0 || !IsHeapLikeAddress(request.memory_address, request.stack_pointer))
            {
                return {};
            }

            std::string target = operand_text;
            if (target.empty())
            {
                std::ostringstream address_out;
                address_out << "0x" << std::hex << request.memory_address;
                target = address_out.str();
            }

            std::ostringstream resolved_address;
            resolved_address << "0x" << std::hex << request.memory_address;
            uint8_t bytes[kEnhancedSampleMaxBytes] = {};
            uint8_t size = 0;
            if (!detail::TryReadPeekBytes(request, bytes, size))
            {
                std::ostringstream out;
                out << " --- [HeapPeekFail]->" << target
                    << "=[" << resolved_address.str() << "].reason=read";
                if (const char *origin_text = detail::OriginText(request.origin); origin_text != nullptr)
                {
                    out << ".origin=" << origin_text;
                }
                return out.str();
            }

            std::ostringstream out;
            out << " --- [HeapPeek]->" << target
                << "=[" << resolved_address.str() << "]"
                << ".bytes=" << detail::FormatBytes(bytes, size)
                << " ascii=\"" << detail::FormatAscii(bytes, size) << "\""
                << " hex=" << FormatLittleEndianIntegerHex(bytes, size);
            if (const char *origin_text = detail::OriginText(request.origin); origin_text != nullptr)
            {
                out << " origin=" << origin_text;
            }
            return out.str();
        }
    }
}
