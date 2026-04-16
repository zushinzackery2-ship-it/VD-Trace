#include "pch.h"

namespace
{
    volatile LONG64 g_helper_sink = 0;

    __declspec(noinline) uint32_t HelperStageA(uint32_t value)
    {
        return (value * 0x45D9F3Bu) ^ (value >> 3);
    }

    __declspec(noinline) uint32_t HelperStageB(uint32_t value)
    {
        if ((value & 1u) != 0)
        {
            return value + 0x12345u;
        }

        return value ^ 0x5A5A5A5Au;
    }

    __declspec(noinline) uint32_t HelperTriggerPoint(uint32_t value)
    {
        return HelperStageB(HelperStageA(value));
    }

    __declspec(noinline) uint32_t HelperDeepDispatch(uint32_t depth, uint32_t value)
    {
        if (depth == 0)
        {
            return HelperTriggerPoint(value);
        }

        const uint32_t nested = HelperDeepDispatch(depth - 1, value + depth * 17u);
        return HelperStageB(nested ^ depth);
    }
}

extern "C" __declspec(dllexport) uintptr_t __stdcall GetTriggerWaitHelperTriggerAddress(void)
{
    return reinterpret_cast<uintptr_t>(&HelperTriggerPoint);
}

extern "C" __declspec(dllexport) uint32_t __stdcall TriggerWaitHelperWork(uint32_t rounds)
{
    const uint32_t seed = 0x13572468u ^ (rounds * 0x1021u);
    uint32_t value = HelperDeepDispatch(160u, seed);

    InterlockedExchange64(&g_helper_sink, static_cast<LONG64>(value));
    return value;
}
