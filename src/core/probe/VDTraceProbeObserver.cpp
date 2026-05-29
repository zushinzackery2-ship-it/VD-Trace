#include "pch.h"
#include "core/probe/VDTraceProbeInternal.h"

namespace vdtrace::probe_detail
{
    uintptr_t ReadProbeRegisterValue(const CONTEXT &context, ProbeRegister reg)
    {
        switch (reg)
        {
        case ProbeRegister::Rax: return static_cast<uintptr_t>(context.Rax);
        case ProbeRegister::Rbx: return static_cast<uintptr_t>(context.Rbx);
        case ProbeRegister::Rcx: return static_cast<uintptr_t>(context.Rcx);
        case ProbeRegister::Rdx: return static_cast<uintptr_t>(context.Rdx);
        case ProbeRegister::Rsi: return static_cast<uintptr_t>(context.Rsi);
        case ProbeRegister::Rdi: return static_cast<uintptr_t>(context.Rdi);
        case ProbeRegister::Rbp: return static_cast<uintptr_t>(context.Rbp);
        case ProbeRegister::Rsp: return static_cast<uintptr_t>(context.Rsp);
        case ProbeRegister::Rip: return static_cast<uintptr_t>(context.Rip);
        case ProbeRegister::R8: return static_cast<uintptr_t>(context.R8);
        case ProbeRegister::R9: return static_cast<uintptr_t>(context.R9);
        case ProbeRegister::R10: return static_cast<uintptr_t>(context.R10);
        case ProbeRegister::R11: return static_cast<uintptr_t>(context.R11);
        case ProbeRegister::R12: return static_cast<uintptr_t>(context.R12);
        case ProbeRegister::R13: return static_cast<uintptr_t>(context.R13);
        case ProbeRegister::R14: return static_cast<uintptr_t>(context.R14);
        case ProbeRegister::R15: return static_cast<uintptr_t>(context.R15);
        case ProbeRegister::None:
        default:
            return 0;
        }
    }

    const ResolvedValueProbe *FindValueProbeByAddress(const std::vector<ResolvedValueProbe> &probes, uintptr_t address)
    {
        const auto it = std::lower_bound(
            probes.begin(),
            probes.end(),
            address,
            [](const ResolvedValueProbe &probe, uintptr_t value)
            {
                return probe.address < value;
            });
        if (it == probes.end() || it->address != address)
        {
            return nullptr;
        }

        return &(*it);
    }

    void FillProbeCapture(const ResolvedProbeCapture &resolved, const CONTEXT &context, ProbeCapture &capture)
    {
        capture = {};
        capture.valid = true;
        CopyProbeLabel(resolved.label, capture.label, std::size(capture.label));

        uintptr_t value = 0;
        switch (resolved.kind)
        {
        case ProbeSourceKind::RegisterValue:
            capture.value = ReadProbeRegisterValue(context, resolved.reg);
            return;
        case ProbeSourceKind::AbsoluteMemory:
            value = resolved.absolute_address;
            break;
        case ProbeSourceKind::RegisterMemory:
            value = ReadProbeRegisterValue(context, resolved.reg) + resolved.offset;
            break;
        }

        capture.value = value;
        capture.size = resolved.size;
        capture.has_bytes = value != 0 && resolved.size != 0 && SafeReadMemoryBytes(value, capture.bytes, resolved.size);
    }

    bool ParseProbeExitKind(const std::wstring &text, ProbeExitKind &exit_kind)
    {
        const std::wstring lowered = ToLowerCopy(text);
        if (lowered == L"return")
        {
            exit_kind = ProbeExitKind::Return;
            return true;
        }
        if (lowered == L"leave")
        {
            exit_kind = ProbeExitKind::LeaveRegion;
            return true;
        }
        if (lowered == L"return-or-leave" || lowered == L"return_or_leave")
        {
            exit_kind = ProbeExitKind::ReturnOrLeaveRegion;
            return true;
        }
        return false;
    }

    bool ParseProbeWatchToken(const std::wstring &text, ResolvedProbeWatch &watch, std::wstring &error)
    {
        watch = {};
        const std::vector<std::wstring> parts = SplitText(text, L':');
        if (parts.size() < 2 || parts.size() > 3)
        {
            error = L"write watch 语法无效。使用 addr:size[:label]。";
            return false;
        }

        if (!ResolveConfiguredAddressText(parts[0], watch.address, error)
            || !ParseProbeSize(parts[1], watch.size))
        {
            if (error.empty())
            {
                error = L"write watch 语法无效。";
            }
            return false;
        }

        const std::wstring label = parts.size() >= 3 ? parts[2] : parts[0];
        CopyProbeLabel(label, watch.label, std::size(watch.label));
        return true;
    }

    bool ParseObserverRule(const std::wstring &text, ResolvedValueProbe &probe, std::wstring &error)
    {
        probe = {};

        std::wistringstream stream(text);
        std::wstring head;
        if (!(stream >> head))
        {
            error = L"观测器规则为空。";
            return false;
        }

        const std::wstring lowered_head = ToLowerCopy(head);
        const bool is_step = lowered_head.rfind(L"step@", 0) == 0;
        const bool is_write = lowered_head.rfind(L"write@", 0) == 0;
        if (!is_step && !is_write)
        {
            error = L"观测器规则语法无效。使用 step@addr ... 或 write@addr ...。";
            return false;
        }

        probe.mode = is_step ? ProbeMode::SingleStep : ProbeMode::WriteTrace;
        probe.exit_kind = ProbeExitKind::ReturnOrLeaveRegion;

        if (!ResolveConfiguredAddressText(head.substr(head.find(L'@') + 1), probe.address, error))
        {
            return false;
        }

        bool has_step_limit = false;
        bool has_watch = false;
        for (std::wstring token; stream >> token;)
        {
            const size_t split = token.find(L'=');
            if (split == std::wstring::npos)
            {
                error = L"观测器规则语法无效。参数使用 key=value。";
                return false;
            }

            const std::wstring key = ToLowerCopy(token.substr(0, split));
            const std::wstring value = token.substr(split + 1);
            if (key == L"steps")
            {
                uintptr_t parsed = 0;
                if (!ParseUnsignedText(value, parsed) || parsed == 0 || parsed > 0xFFFFFFFFull)
                {
                    error = L"steps 参数无效。";
                    return false;
                }
                probe.step_limit = static_cast<uint32_t>(parsed);
                has_step_limit = true;
                continue;
            }

            if (key == L"exit")
            {
                if (!ParseProbeExitKind(value, probe.exit_kind))
                {
                    error = L"exit 参数无效。支持 return / leave / return-or-leave。";
                    return false;
                }
                continue;
            }

            if (key == L"watch")
            {
                if (!is_write)
                {
                    error = L"step 规则不接受 watch 参数。";
                    return false;
                }

                const std::vector<std::wstring> watch_tokens = SplitText(value, L'|');
                if (watch_tokens.empty() || watch_tokens.size() > kProbeCaptureMaxCount)
                {
                    error = L"write 规则需要 1 到 4 个 watch。";
                    return false;
                }

                for (const std::wstring &watch_token : watch_tokens)
                {
                    ResolvedProbeWatch watch = {};
                    if (!ParseProbeWatchToken(watch_token, watch, error))
                    {
                        return false;
                    }
                    probe.watches[probe.watch_count++] = watch;
                }
                has_watch = true;
                continue;
            }

            error = L"未知观测器参数。";
            return false;
        }

        if (!has_step_limit)
        {
            error = L"step/write 规则必须显式给出 steps=。";
            return false;
        }

        if (is_write && !has_watch)
        {
            error = L"write 规则必须显式给出 watch=。";
            return false;
        }

        return true;
    }

    bool ParseLegacyCaptureRule(const std::wstring &rule, std::vector<ResolvedValueProbe> &probes, std::wstring &error)
    {
        const size_t arrow = rule.find(L"->");
        if (arrow == std::wstring::npos)
        {
            error = L"capture 规则语法无效。使用 hit->capture|capture。";
            return false;
        }

        uintptr_t hit_address = 0;
        if (!ResolveConfiguredAddressText(rule.substr(0, arrow), hit_address, error))
        {
            return false;
        }

        const std::vector<std::wstring> capture_tokens = SplitText(rule.substr(arrow + 2), L'|');
        if (capture_tokens.empty() || capture_tokens.size() > kProbeCaptureMaxCount)
        {
            error = L"每个 capture 规则需要 1 到 4 个 capture。";
            return false;
        }

        auto existing = std::find_if(
            probes.begin(),
            probes.end(),
            [hit_address](const ResolvedValueProbe &probe)
            {
                return probe.address == hit_address;
            });
        if (existing == probes.end())
        {
            probes.push_back({});
            existing = probes.end() - 1;
            existing->address = hit_address;
            existing->mode = ProbeMode::Capture;
        }

        if (existing->mode != ProbeMode::Capture)
        {
            error = L"同一个命中点不能同时混用 capture 和 step/write 规则。";
            return false;
        }

        for (const std::wstring &capture_token : capture_tokens)
        {
            if (existing->capture_count >= kProbeCaptureMaxCount)
            {
                error = L"单个 capture 规则最多保留 4 个 capture。";
                return false;
            }

            ResolvedProbeCapture capture = {};
            if (!ParseProbeCaptureToken(capture_token, capture, error))
            {
                return false;
            }

            existing->captures[existing->capture_count++] = capture;
        }

        return true;
    }
}
