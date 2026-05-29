#include "pch.h"
#include "core/extender/VDTraceExtenderAnalyzeInternal.h"

namespace vdtrace
{
    namespace extender
    {
        namespace detail
        {
            uint64_t BitMask(uint32_t width)
            {
                return width >= 64 ? ~0ull : ((1ull << width) - 1ull);
            }

            uint64_t SignExtend(uint64_t value, uint32_t width)
            {
                if (width == 0 || width >= 64)
                {
                    return value;
                }

                const uint64_t sign_bit = 1ull << (width - 1);
                const uint64_t mask = BitMask(width);
                value &= mask;
                return (value & sign_bit) != 0 ? (value | ~mask) : value;
            }

            int RegisterIndex(ZydisRegister reg)
            {
                switch (ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg))
                {
                case ZYDIS_REGISTER_RAX:
                    return SimRax;
                case ZYDIS_REGISTER_RBX:
                    return SimRbx;
                case ZYDIS_REGISTER_RCX:
                    return SimRcx;
                case ZYDIS_REGISTER_RDX:
                    return SimRdx;
                case ZYDIS_REGISTER_RSI:
                    return SimRsi;
                case ZYDIS_REGISTER_RDI:
                    return SimRdi;
                case ZYDIS_REGISTER_RBP:
                    return SimRbp;
                case ZYDIS_REGISTER_RSP:
                    return SimRsp;
                case ZYDIS_REGISTER_RIP:
                    return SimRip;
                case ZYDIS_REGISTER_R8:
                    return SimR8;
                case ZYDIS_REGISTER_R9:
                    return SimR9;
                case ZYDIS_REGISTER_R10:
                    return SimR10;
                case ZYDIS_REGISTER_R11:
                    return SimR11;
                case ZYDIS_REGISTER_R12:
                    return SimR12;
                case ZYDIS_REGISTER_R13:
                    return SimR13;
                case ZYDIS_REGISTER_R14:
                    return SimR14;
                case ZYDIS_REGISTER_R15:
                    return SimR15;
                default:
                    return -1;
                }
            }

            bool IsHigh8Register(ZydisRegister reg)
            {
                return reg == ZYDIS_REGISTER_AH
                    || reg == ZYDIS_REGISTER_BH
                    || reg == ZYDIS_REGISTER_CH
                    || reg == ZYDIS_REGISTER_DH;
            }

            void InitializeContext(const ThreadContextSnapshot &snapshot, SimContext &context)
            {
                context.regs[SimRax] = {snapshot.rax, true};
                context.regs[SimRbx] = {snapshot.rbx, true};
                context.regs[SimRcx] = {snapshot.rcx, true};
                context.regs[SimRdx] = {snapshot.rdx, true};
                context.regs[SimRsi] = {snapshot.rsi, true};
                context.regs[SimRdi] = {snapshot.rdi, true};
                context.regs[SimRbp] = {snapshot.rbp, true};
                context.regs[SimRsp] = {snapshot.rsp, true};
                context.regs[SimRip] = {snapshot.rip, true};
                context.regs[SimR8] = {snapshot.r8, true};
                context.regs[SimR9] = {snapshot.r9, true};
                context.regs[SimR10] = {snapshot.r10, true};
                context.regs[SimR11] = {snapshot.r11, true};
                context.regs[SimR12] = {snapshot.r12, true};
                context.regs[SimR13] = {snapshot.r13, true};
                context.regs[SimR14] = {snapshot.r14, true};
                context.regs[SimR15] = {snapshot.r15, true};
            }

            bool ReadRegister(const SimContext &context, ZydisRegister reg, uint64_t &value)
            {
                const int index = RegisterIndex(reg);
                if (index < 0 || !context.regs[index].known)
                {
                    return false;
                }

                const uint32_t width = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, reg);
                const uint64_t full = context.regs[index].value;
                if (IsHigh8Register(reg))
                {
                    value = (full >> 8) & 0xFFu;
                    return true;
                }

                value = full & BitMask(width);
                return true;
            }

            void WriteRegister(SimContext &context, ZydisRegister reg, uint64_t value)
            {
                const int index = RegisterIndex(reg);
                if (index < 0)
                {
                    return;
                }

                const uint32_t width = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, reg);
                uint64_t full = context.regs[index].known ? context.regs[index].value : 0;
                if (width >= 64)
                {
                    full = value;
                }
                else if (width == 32)
                {
                    full = static_cast<uint32_t>(value);
                }
                else if (IsHigh8Register(reg))
                {
                    full = (full & ~0xFF00ull) | ((value & 0xFFull) << 8);
                }
                else
                {
                    const uint64_t mask = BitMask(width);
                    full = (full & ~mask) | (value & mask);
                }

                context.regs[index].value = full;
                context.regs[index].known = true;
            }

            void MarkOperandRegisterUnknown(SimContext &context, const ZydisDecodedOperand &operand)
            {
                if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER)
                {
                    const int index = RegisterIndex(operand.reg.value);
                    if (index >= 0)
                    {
                        context.regs[index].known = false;
                    }
                }
            }
        }
    }
}
