#include "pch.h"
#include "VDTraceAsyncSupportInternal.h"

namespace vdtrace
{
    const std::vector<ResolvedAsyncProbe> &KnownAsyncProbes()
    {
        static std::once_flag init_once;
        static std::vector<ResolvedAsyncProbe> probes;
        std::call_once(
            init_once,
            []()
            {
                std::unordered_set<uintptr_t> seen_addresses;
                probes.reserve(async_support_detail::kKnownAsyncProbeSpecCount);
                for (size_t spec_index = 0; spec_index < async_support_detail::kKnownAsyncProbeSpecCount; ++spec_index)
                {
                    const auto &spec = async_support_detail::kKnownAsyncProbeSpecs[spec_index];
                    HMODULE module = GetModuleHandleW(spec.module_name);
                    if (module == nullptr)
                    {
                        continue;
                    }

                    FARPROC symbol = GetProcAddress(module, spec.symbol_name);
                    if (symbol == nullptr)
                    {
                        continue;
                    }

                    const uintptr_t address = reinterpret_cast<uintptr_t>(symbol);
                    if (!seen_addresses.insert(address).second)
                    {
                        continue;
                    }

                    ResolvedAsyncProbe probe = {};
                    probe.address = address;
                    probe.kind = spec.kind;
                    probe.argument_count = spec.argument_count;
                    for (uint8_t index = 0; index < spec.argument_count && index < 4; index++)
                    {
                        probe.argument_indices[index] = spec.argument_indices[index];
                        probe.argument_is_pointer[index] = spec.argument_is_pointer[index];
                        probe.argument_names[index] = async_support_detail::WideText(spec.argument_names[index]);
                    }
                    probe.thread_handle_source = spec.thread_handle_source;
                    probe.thread_handle_argument_index = spec.thread_handle_argument_index;
                    probe.thread_id_argument_index = spec.thread_id_argument_index;
                    probe.handoff_entry_argument_index = spec.handoff_entry_argument_index;
                    probe.module_name = async_support_detail::ResolveModuleFilename(module);
                    if (probe.module_name.empty())
                    {
                        probe.module_name = spec.module_name;
                    }
                    probe.symbol_name = async_support_detail::WideText(spec.symbol_name);
                    probes.push_back(std::move(probe));
                }
            });
        return probes;
    }

    const ResolvedAsyncProbe *FindAsyncProbe(const std::vector<ResolvedAsyncProbe> &probes, uintptr_t address)
    {
        for (const auto &probe : probes)
        {
            if (probe.address == address)
            {
                return &probe;
            }
        }

        return nullptr;
    }

    void CaptureCallArguments(const CONTEXT &context, uintptr_t *arguments, uint8_t &count)
    {
        count = 0;
        if (arguments == nullptr)
        {
            return;
        }

        std::memset(arguments, 0, sizeof(uintptr_t) * 8);
        arguments[0] = static_cast<uintptr_t>(context.Rcx);
        arguments[1] = static_cast<uintptr_t>(context.Rdx);
        arguments[2] = static_cast<uintptr_t>(context.R8);
        arguments[3] = static_cast<uintptr_t>(context.R9);
        for (uint8_t index = 0; index < 4; index++)
        {
            const uintptr_t slot = static_cast<uintptr_t>(context.Rsp) + 0x28u + static_cast<uintptr_t>(index) * sizeof(uintptr_t);
            async_support_detail::TryReadPointerValue(slot, arguments[4 + index]);
        }

        count = 8;
    }

    void CaptureThreadContext(const CONTEXT &context, ThreadContextSnapshot &snapshot)
    {
        snapshot = {};
        snapshot.valid = true;
        snapshot.rip = static_cast<uintptr_t>(context.Rip);
        snapshot.rsp = static_cast<uintptr_t>(context.Rsp);
        snapshot.rbp = static_cast<uintptr_t>(context.Rbp);
        snapshot.rax = static_cast<uintptr_t>(context.Rax);
        snapshot.rbx = static_cast<uintptr_t>(context.Rbx);
        snapshot.rcx = static_cast<uintptr_t>(context.Rcx);
        snapshot.rdx = static_cast<uintptr_t>(context.Rdx);
        snapshot.rsi = static_cast<uintptr_t>(context.Rsi);
        snapshot.rdi = static_cast<uintptr_t>(context.Rdi);
        snapshot.r8 = static_cast<uintptr_t>(context.R8);
        snapshot.r9 = static_cast<uintptr_t>(context.R9);
        snapshot.r10 = static_cast<uintptr_t>(context.R10);
        snapshot.r11 = static_cast<uintptr_t>(context.R11);
        snapshot.r12 = static_cast<uintptr_t>(context.R12);
        snapshot.r13 = static_cast<uintptr_t>(context.R13);
        snapshot.r14 = static_cast<uintptr_t>(context.R14);
        snapshot.r15 = static_cast<uintptr_t>(context.R15);
        snapshot.rflags = static_cast<uint64_t>(context.EFlags);
    }

    bool ResolveAddressLabel(uintptr_t address, AddressLabel &label)
    {
        label = {};
        if (address == 0)
        {
            return false;
        }

        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address),
                &module))
        {
            return false;
        }

        MODULEINFO info = {};
        if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        {
            return false;
        }

        label.valid = true;
        label.module_base = reinterpret_cast<uintptr_t>(module);
        label.module_size = static_cast<size_t>(info.SizeOfImage);
        label.relative = address - label.module_base;
        label.module_name = async_support_detail::ResolveModuleFilename(module);
        if (label.module_name.empty())
        {
            label.module_name = L"inside";
        }
        async_support_detail::TryResolveExportSymbol(module, address, label.symbol_name, label.symbol_offset);
        return true;
    }

    const wchar_t *AsyncDispatchKindName(AsyncDispatchKind kind)
    {
        switch (kind)
        {
        case AsyncDispatchKind::ThreadStart:
            return L"thread";
        case AsyncDispatchKind::WorkItem:
            return L"work";
        case AsyncDispatchKind::ThreadPool:
            return L"threadpool";
        case AsyncDispatchKind::Apc:
            return L"apc";
        case AsyncDispatchKind::None:
        default:
            return L"none";
        }
    }
}
