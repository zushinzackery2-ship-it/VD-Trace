#include "pch.h"
#include "tests/decrypt_smoke/VDTraceDecryptSmokeHelperInternal.h"

#include <array>
#include <mutex>
#include <vector>

namespace decrypt_smoke_helper
{
    namespace
    {
        using jit_detail::DynamicStageFn;

        volatile LONG64 g_decrypt_smoke_sink = 0;
        std::once_flag g_dynamic_stage_once;
        uint8_t *g_dynamic_stage_base = nullptr;
        DynamicStageFn g_dynamic_stage = nullptr;

        template <typename T>
        T RotateLeft(T value, unsigned int shift)
        {
            constexpr unsigned int bits = sizeof(T) * 8u;
            shift %= bits;
            return static_cast<T>((value << shift) | (value >> (bits - shift)));
        }

        template <typename T>
        T RotateRight(T value, unsigned int shift)
        {
            constexpr unsigned int bits = sizeof(T) * 8u;
            shift %= bits;
            return static_cast<T>((value >> shift) | (value << (bits - shift)));
        }

        __declspec(noinline) uint32_t SeedKeyPartA()
        {
            return 0x13579BDFu;
        }

        __declspec(noinline) uint32_t SeedKeyPartB()
        {
            return 0x2468ACE0u;
        }

        __declspec(noinline) uint32_t SeedKeyPartC()
        {
            return 0x89ABCDEFu;
        }

        __declspec(noinline) uint32_t SeedKeyPartD()
        {
            return 0x10213254u;
        }

        __declspec(noinline) uint32_t FoldMasterSeed(uint32_t nonce)
        {
            const uint32_t a = SeedKeyPartA();
            const uint32_t b = SeedKeyPartB();
            const uint32_t c = SeedKeyPartC();
            const uint32_t d = SeedKeyPartD();
            return RotateLeft<uint32_t>(a ^ c ^ nonce, 5u)
                ^ RotateLeft<uint32_t>(b + d + 0x7F4A7C15u, 11u);
        }

        __declspec(noinline) void ExpandRoundKeys(uint32_t nonce, uint32_t *round_keys, size_t count)
        {
            const uint32_t seed = FoldMasterSeed(nonce);
            for (size_t index = 0; index < count; ++index)
            {
                uint32_t value = seed ^ static_cast<uint32_t>(index * 0x9E3779B9u);
                value = RotateLeft<uint32_t>(value + SeedKeyPartA(), static_cast<unsigned int>((index & 7u) + 1u));
                value ^= RotateRight<uint32_t>(SeedKeyPartB() + static_cast<uint32_t>(index * 0x1021u), static_cast<unsigned int>((index & 3u) + 3u));
                value += RotateLeft<uint32_t>(SeedKeyPartC() ^ SeedKeyPartD(), static_cast<unsigned int>((index & 7u) + 5u));
                round_keys[index] = value;
            }
        }

        __declspec(noinline) void PreWhitenStage(std::vector<uint8_t> &bytes, const uint32_t *round_keys, size_t key_count, uint32_t nonce, bool encrypt)
        {
            for (size_t index = 0; index < bytes.size(); ++index)
            {
                const uint32_t key = round_keys[(index + nonce) % key_count];
                const uint8_t mask = static_cast<uint8_t>((key >> ((index & 3u) * 8u)) ^ (index * 13u + 0x5Au));
                const unsigned int shift = static_cast<unsigned int>(((index ^ nonce) & 3u) + 1u);
                bytes[index] = encrypt
                    ? RotateLeft<uint8_t>(static_cast<uint8_t>(bytes[index] ^ mask), shift)
                    : static_cast<uint8_t>(RotateRight<uint8_t>(bytes[index], shift) ^ mask);
            }
        }

        __declspec(noinline) void ChunkMirrorStage(std::vector<uint8_t> &bytes, const uint32_t *round_keys, size_t key_count)
        {
            for (size_t offset = 0; offset < bytes.size(); offset += 8u)
            {
                const size_t remaining = std::min<size_t>(8u, bytes.size() - offset);
                if ((round_keys[(offset / 8u) % key_count] & 1u) == 0 || remaining < 2u)
                {
                    continue;
                }

                for (size_t index = 0; index < remaining / 2u; ++index)
                {
                    std::swap(bytes[offset + index], bytes[offset + remaining - 1u - index]);
                }
            }
        }

        bool EnsureDynamicStage()
        {
            std::call_once(
                g_dynamic_stage_once,
                []
                {
                    jit_detail::BuildDynamicStage(g_dynamic_stage_base, g_dynamic_stage);
                });
            return g_dynamic_stage != nullptr;
        }

        __declspec(noinline) BOOL DispatchDynamicStage(std::vector<uint8_t> &bytes, const uint32_t *round_keys, uint32_t nonce)
        {
            if (!EnsureDynamicStage())
            {
                return FALSE;
            }

            if (bytes.empty())
            {
                return TRUE;
            }

            g_dynamic_stage(bytes.data(), bytes.size(), round_keys, nonce);
            if (bytes.size() > 24u)
            {
                const size_t offset = bytes.size() / 3u;
                g_dynamic_stage(bytes.data() + offset, bytes.size() - offset, round_keys, nonce ^ 0x6C8E9CF5u);
            }

            InterlockedExchangeAdd64(&g_decrypt_smoke_sink, static_cast<LONG64>(bytes[0]));
            return TRUE;
        }

        __declspec(noinline) void TailWhitenStage(std::vector<uint8_t> &bytes, const uint32_t *round_keys, size_t key_count, uint32_t nonce, bool encrypt)
        {
            for (size_t index = 0; index < bytes.size(); ++index)
            {
                const uint32_t key = round_keys[(index * 3u + 1u) % key_count] ^ RotateLeft<uint32_t>(nonce, static_cast<unsigned int>(index & 7u));
                const uint8_t mask = static_cast<uint8_t>((key >> (((index + 1u) & 3u) * 8u)) + 0x33u + (index & 0x1Fu));
                const unsigned int shift = static_cast<unsigned int>(((index + 2u) & 7u) + 1u);
                bytes[index] = encrypt
                    ? static_cast<uint8_t>(RotateRight<uint8_t>(bytes[index], shift) ^ mask)
                    : RotateLeft<uint8_t>(static_cast<uint8_t>(bytes[index] ^ mask), shift);
            }
        }

        __declspec(noinline) BOOL ProcessBufferPipeline(uint8_t *data, size_t size, uint32_t nonce, bool encrypt)
        {
            if (data == nullptr || size == 0u)
            {
                return FALSE;
            }

            std::array<uint32_t, 8u> round_keys = {};
            ExpandRoundKeys(nonce, round_keys.data(), round_keys.size());
            std::vector<uint8_t> bytes(data, data + size);

            if (encrypt)
            {
                PreWhitenStage(bytes, round_keys.data(), round_keys.size(), nonce, true);
                if (!DispatchDynamicStage(bytes, round_keys.data(), nonce ^ 0xA5315C29u))
                {
                    return FALSE;
                }
                ChunkMirrorStage(bytes, round_keys.data(), round_keys.size());
                if (!DispatchDynamicStage(bytes, round_keys.data(), nonce ^ 0x7F4A7C15u))
                {
                    return FALSE;
                }
                TailWhitenStage(bytes, round_keys.data(), round_keys.size(), nonce, true);
            }
            else
            {
                TailWhitenStage(bytes, round_keys.data(), round_keys.size(), nonce, false);
                if (!DispatchDynamicStage(bytes, round_keys.data(), nonce ^ 0x7F4A7C15u))
                {
                    return FALSE;
                }
                ChunkMirrorStage(bytes, round_keys.data(), round_keys.size());
                if (!DispatchDynamicStage(bytes, round_keys.data(), nonce ^ 0xA5315C29u))
                {
                    return FALSE;
                }
                PreWhitenStage(bytes, round_keys.data(), round_keys.size(), nonce, false);
            }

            std::memcpy(data, bytes.data(), size);
            InterlockedExchangeAdd64(&g_decrypt_smoke_sink, static_cast<LONG64>(bytes[0]));
            return TRUE;
        }
    }

    BOOL EncryptBuffer(uint8_t *data, size_t size)
    {
        return ProcessBufferPipeline(data, size, 0x31415926u, true);
    }

    BOOL DecryptBuffer(uint8_t *data, size_t size)
    {
        return ProcessBufferPipeline(data, size, 0x31415926u, false);
    }

    uintptr_t GetPipelineAddress()
    {
        return reinterpret_cast<uintptr_t>(&ProcessBufferPipeline);
    }

    uintptr_t GetExpandRoundKeysAddress()
    {
        return reinterpret_cast<uintptr_t>(&ExpandRoundKeys);
    }

    uintptr_t GetPreWhitenStageAddress()
    {
        return reinterpret_cast<uintptr_t>(&PreWhitenStage);
    }

    uintptr_t GetDispatchDynamicStageAddress()
    {
        return reinterpret_cast<uintptr_t>(&DispatchDynamicStage);
    }

    uintptr_t GetChunkMirrorStageAddress()
    {
        return reinterpret_cast<uintptr_t>(&ChunkMirrorStage);
    }

    uintptr_t GetTailWhitenStageAddress()
    {
        return reinterpret_cast<uintptr_t>(&TailWhitenStage);
    }

    uintptr_t GetDynamicStageBaseAddress()
    {
        return EnsureDynamicStage() ? reinterpret_cast<uintptr_t>(g_dynamic_stage_base) : 0u;
    }

    void CleanupDynamicStage()
    {
        if (g_dynamic_stage_base != nullptr)
        {
            VirtualFree(g_dynamic_stage_base, 0, MEM_RELEASE);
            g_dynamic_stage_base = nullptr;
            g_dynamic_stage = nullptr;
        }
    }
}
