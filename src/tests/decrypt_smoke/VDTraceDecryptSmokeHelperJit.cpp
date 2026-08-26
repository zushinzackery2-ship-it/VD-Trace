#include "pch.h"
#include "tests/decrypt_smoke/VDTraceDecryptSmokeHelperInternal.h"

namespace decrypt_smoke_helper::jit_detail
{
    namespace
    {
        class CodeWriter
        {
          public:
            explicit CodeWriter(uint8_t *buffer)
                : buffer_(buffer)
            {
            }

            size_t Position() const
            {
                return position_;
            }

            void Emit(std::initializer_list<uint8_t> bytes)
            {
                for (const uint8_t value : bytes)
                {
                    buffer_[position_++] = value;
                }
            }

            void EmitU32(uint32_t value)
            {
                std::memcpy(buffer_ + position_, &value, sizeof(value));
                position_ += sizeof(value);
            }

            size_t EmitRel32(std::initializer_list<uint8_t> prefix)
            {
                Emit(prefix);
                const size_t patch = position_;
                EmitU32(0);
                return patch;
            }

            void PatchRel32(size_t patch, size_t target)
            {
                const int32_t relative = static_cast<int32_t>(target - (patch + sizeof(int32_t)));
                std::memcpy(buffer_ + patch, &relative, sizeof(relative));
            }

          private:
            uint8_t *buffer_ = nullptr;
            size_t position_ = 0;
        };
    }

    bool BuildDynamicStage(uint8_t *&stage_base, DynamicStageFn &stage)
    {
        stage_base = static_cast<uint8_t *>(VirtualAlloc(nullptr, 256, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (stage_base == nullptr)
        {
            return false;
        }

        CodeWriter writer(stage_base);
        writer.Emit({0x45, 0x33, 0xD2});
        const size_t loop = writer.Position();
        writer.Emit({0x49, 0x3B, 0xD2});
        const size_t done_patch = writer.EmitRel32({0x0F, 0x83});
        writer.Emit({0x41, 0x8B, 0xC2, 0x83, 0xE0, 0x07, 0x41, 0x8B, 0x04, 0x80});
        writer.Emit({0x45, 0x8B, 0xDA, 0x45, 0x69, 0xDB});
        writer.EmitU32(0x045D9F3Bu);
        writer.Emit({0x44, 0x31, 0xC8, 0x05, 0x15, 0x7C, 0x4A, 0x7F, 0xC1, 0xC8, 0x03, 0x44, 0x01, 0xD8, 0xC1, 0xC0, 0x05});
        writer.Emit({0x42, 0x30, 0x04, 0x11, 0x49, 0xFF, 0xC2});
        const size_t loop_patch = writer.EmitRel32({0xE9});
        const size_t done = writer.Position();
        writer.Emit({0xC3});
        writer.PatchRel32(done_patch, done);
        writer.PatchRel32(loop_patch, loop);
        FlushInstructionCache(GetCurrentProcess(), stage_base, writer.Position());
        stage = reinterpret_cast<DynamicStageFn>(stage_base);
        return stage != nullptr;
    }
}
