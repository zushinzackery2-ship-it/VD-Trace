#include "pch.h"
#include "core/threading/VDTraceThreadContextInternal.h"

namespace vdtrace
{
    std::wstring FormatWin32Error(const wchar_t *prefix, DWORD error)
    {
        wchar_t system_buffer[256] = {};
        FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            0,
            system_buffer,
            static_cast<DWORD>(sizeof(system_buffer) / sizeof(system_buffer[0])),
            nullptr);

        std::wostringstream out;
        out << (prefix != nullptr ? prefix : L"error")
            << L" (error=" << error << L")";
        if (system_buffer[0] != L'\0')
        {
            out << L" " << system_buffer;
        }
        return out.str();
    }

    bool ResolveModuleRange(const std::wstring &module_name, ModuleRange &range, std::wstring &error)
    {
        HMODULE module = GetModuleHandleW(module_name.c_str());
        if (module == nullptr)
        {
            error = L"模块未加载: " + module_name;
            return false;
        }

        MODULEINFO info = {};
        if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
        {
            error = FormatWin32Error(L"读取模块信息失败。", GetLastError());
            return false;
        }

        range.name = module_name;
        range.base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
        range.size = static_cast<size_t>(info.SizeOfImage);
        return true;
    }

    void EnumerateSystemModuleRanges(std::vector<ModuleRange> &ranges)
    {
        ranges.clear();

        DWORD needed = 0;
        std::vector<HMODULE> modules(256);
        if (!EnumProcessModules(GetCurrentProcess(), modules.data(), static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &needed))
        {
            return;
        }

        if (needed > modules.size() * sizeof(HMODULE))
        {
            modules.resize(needed / sizeof(HMODULE));
            if (!EnumProcessModules(GetCurrentProcess(), modules.data(), static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &needed))
            {
                return;
            }
        }

        modules.resize(needed / sizeof(HMODULE));
        for (HMODULE module : modules)
        {
            const std::wstring path = thread_context_detail::GetModuleFilePath(module);
            if (!thread_context_detail::IsSystemModulePath(path))
            {
                continue;
            }

            MODULEINFO info = {};
            if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info)))
            {
                continue;
            }

            ModuleRange range = {};
            range.name = std::filesystem::path(path).filename().wstring();
            range.base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
            range.size = static_cast<size_t>(info.SizeOfImage);
            ranges.push_back(std::move(range));
        }

        std::sort(
            ranges.begin(),
            ranges.end(),
            [](const ModuleRange &left, const ModuleRange &right)
            {
                return left.base < right.base;
            });
    }

    bool SetSingleStepFlag(HANDLE thread_handle, bool enabled, std::wstring &error)
    {
        return thread_context_detail::UpdateThreadContext(
            thread_handle,
            CONTEXT_CONTROL,
            [enabled](CONTEXT &context)
            {
                if (enabled)
                {
                    context.EFlags |= 0x100u;
                }
                else
                {
                    context.EFlags &= ~0x100u;
                }
                return true;
            },
            error);
    }

    bool ClearThreadTraceState(HANDLE thread_handle, std::wstring &error)
    {
        return thread_context_detail::UpdateThreadContext(
            thread_handle,
            CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS,
            [](CONTEXT &context)
            {
                uintptr_t empty[4] = {};
                thread_context_detail::ApplyExecutionControls(context, empty, 0, false);
                return true;
            },
            error);
    }

    bool ReadThreadInstructionPointer(HANDLE thread_handle, uintptr_t &rip, std::wstring &error)
    {
        rip = 0;
        return thread_context_detail::UpdateThreadContext(
            thread_handle,
            CONTEXT_CONTROL,
            [&rip](CONTEXT &context)
            {
                rip = static_cast<uintptr_t>(context.Rip);
                return true;
            },
            error);
    }

    bool ReadThreadControlState(HANDLE thread_handle, uintptr_t &rip, uintptr_t &rsp, std::wstring &error)
    {
        rip = 0;
        rsp = 0;
        return thread_context_detail::UpdateThreadContext(
            thread_handle,
            CONTEXT_CONTROL,
            [&rip, &rsp](CONTEXT &context)
            {
                rip = static_cast<uintptr_t>(context.Rip);
                rsp = static_cast<uintptr_t>(context.Rsp);
                return true;
            },
            error);
    }

    bool ArmHardwareExecution(HANDLE thread_handle, const uintptr_t *addresses, uint32_t count, std::wstring &error)
    {
        if (count > 4)
        {
            error = L"硬件断点数量超过 4。";
            return false;
        }

        return thread_context_detail::UpdateThreadContext(
            thread_handle,
            CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS,
            [addresses, count](CONTEXT &context)
            {
                thread_context_detail::ApplyExecutionControls(context, addresses, count, false);
                return true;
            },
            error);
    }

    bool ArmSingleStep(HANDLE thread_handle, std::wstring &error)
    {
        return thread_context_detail::UpdateThreadContext(
            thread_handle,
            CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS,
            [](CONTEXT &context)
            {
                uintptr_t empty[4] = {};
                thread_context_detail::ApplyExecutionControls(context, empty, 0, true);
                return true;
            },
            error);
    }

    bool TryFindInsideReturnAddress(const Session::Impl &impl, uintptr_t stack_pointer, uintptr_t &address)
    {
        address = 0;
        if (stack_pointer == 0)
        {
            return false;
        }

        for (size_t index = 0; index < 64; index++)
        {
            const uintptr_t slot = stack_pointer + index * sizeof(uintptr_t);
            uintptr_t candidate = 0;
            if (!thread_context_detail::TryReadPointerValue(slot, candidate))
            {
                continue;
            }

            if (FindModuleRange(impl.module_ranges, candidate) != nullptr)
            {
                address = candidate;
                return true;
            }
        }

        return false;
    }

    const ModuleRange *FindModuleRange(const std::vector<ModuleRange> &ranges, uintptr_t instruction)
    {
        const auto it = std::upper_bound(
            ranges.begin(),
            ranges.end(),
            instruction,
            [](uintptr_t value, const ModuleRange &range)
            {
                return value < range.base;
            });
        if (it == ranges.begin())
        {
            return nullptr;
        }

        const ModuleRange &range = *(it - 1);
        const uintptr_t end = range.base + range.size;
        return instruction >= range.base && instruction < end ? &range : nullptr;
    }
}
