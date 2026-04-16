#ifndef VDTRACE_EXTENDER_ANALYZE_INTERNAL_H
#define VDTRACE_EXTENDER_ANALYZE_INTERNAL_H

#include "VDTraceExtenderSupport.h"

#include "third_party/zydis/Zydis.h"

namespace vdtrace
{
    namespace extender
    {
        namespace detail
        {
            constexpr uintptr_t kMaxExtendedBlockSpan = 4096;
            constexpr uint32_t kMaxExtendedInstructions = 256;
            constexpr size_t kSimRegisterCount = 17;

            enum SimRegisterIndex : size_t
            {
                SimRax = 0,
                SimRbx,
                SimRcx,
                SimRdx,
                SimRsi,
                SimRdi,
                SimRbp,
                SimRsp,
                SimRip,
                SimR8,
                SimR9,
                SimR10,
                SimR11,
                SimR12,
                SimR13,
                SimR14,
                SimR15,
            };

            struct SimRegisterValue
            {
                uint64_t value = 0;
                bool known = false;
            };

            struct SimContext
            {
                SimRegisterValue regs[kSimRegisterCount] = {};
                uint32_t live_private_reads_remaining = 1;
            };

            struct SimMemoryCell
            {
                uintptr_t address = 0;
                uint64_t value = 0;
                uint32_t size = 0;
            };

            ZydisDecoder &GetDecoder();
            ZydisFormatter &GetFormatter();
            uint64_t BitMask(uint32_t width);
            uint64_t SignExtend(uint64_t value, uint32_t width);
            int RegisterIndex(ZydisRegister reg);
            bool IsHigh8Register(ZydisRegister reg);
            void InitializeContext(const ThreadContextSnapshot &snapshot, SimContext &context);
            bool ReadRegister(const SimContext &context, ZydisRegister reg, uint64_t &value);
            void WriteRegister(SimContext &context, ZydisRegister reg, uint64_t value);
            void MarkOperandRegisterUnknown(SimContext &context, const ZydisDecodedOperand &operand);
            bool TryComputeOperandAddress(
                const SimContext &context,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand &operand,
                uintptr_t instruction_address,
                uintptr_t &address);
            uint32_t ResolveOperandBits(
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uint8_t operand_index);
            bool DecodeFullInstruction(uintptr_t address, ZydisDecodedInstruction &instruction, ZydisDecodedOperand *operands);
            std::string FormatInstruction(uintptr_t address, const ZydisDecodedInstruction &instruction, const ZydisDecodedOperand *operands);
            std::string FormatOperandText(uintptr_t address, const ZydisDecodedInstruction &instruction, const ZydisDecodedOperand &operand);
            bool IsAddressComputationInstruction(const ZydisDecodedInstruction &instruction);
            bool IsPseudoMemoryInstruction(const ZydisDecodedInstruction &instruction);
            std::string FormatInstructionBytes(const uint8_t *bytes, uint8_t size);
            bool ReadScalarMemory(
                const std::vector<SimMemoryCell> &overlay,
                uintptr_t address,
                uintptr_t stack_pointer,
                uint32_t &live_private_reads_remaining,
                uint32_t bits,
                uint64_t &value);
            bool TryResolveOperandValue(
                SimContext &context,
                const std::vector<SimMemoryCell> &overlay,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uint8_t operand_index,
                const ZydisDecodedOperand &operand,
                uintptr_t address,
                uint64_t &value);
            bool QueueMemoryOperand(
                const RecorderQueuedEvent &event,
                const SimContext &context,
                const std::vector<SimMemoryCell> &overlay,
                uintptr_t instruction_address,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uint8_t operand_index,
                const ZydisDecodedOperand &operand,
                const std::string &disasm,
                std::vector<ExtendedMemoryAccess> &accesses);
            void ApplyInstructionEffects(
                SimContext &context,
                std::vector<SimMemoryCell> &overlay,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uintptr_t instruction_address);
        }
    }
}

#endif
