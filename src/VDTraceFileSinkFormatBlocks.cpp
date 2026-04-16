#include "pch.h"
#include "VDTraceFileSinkFormatInternal.h"

namespace vdtrace::file_sink_format_detail
{
    std::string FormatProbeBlock(const RecorderQueuedEvent &event, const std::string &indent)
    {
        std::ostringstream out;
        out << indent << "[probe] hit=0x" << std::hex << event.instruction << "\n";
        const std::string disasm = FormatSingleProbeDisasm(event);
        if (!disasm.empty())
        {
            out << indent << "   [disasm] " << disasm << "\n";
        }
        for (uint8_t index = 0; index < event.probe_capture_count && index < kProbeCaptureMaxCount; ++index)
        {
            const ProbeCapture &capture = event.probe_captures[index];
            if (!capture.valid)
            {
                continue;
            }

            out << indent << "   [probe] " << Narrow(std::wstring(capture.label)) << "=";
            if (capture.has_bytes && capture.size != 0)
            {
                out << "0x" << std::hex << capture.value
                    << " mem=" << FormatProbeBytes(capture.bytes, capture.size)
                    << " ascii=\"" << FormatProbeAscii(capture.bytes, capture.size) << "\"";
            }
            else
            {
                out << "0x" << std::hex << capture.value;
            }
            out << "\n";
        }
        return out.str();
    }

    std::string FormatThreadContextBlock(const RecorderQueuedEvent &event, const char *label)
    {
        if (!event.thread_context.valid)
        {
            return {};
        }

        std::ostringstream out;
        out << "   " << label << " "
            << "rip=0x" << std::hex << event.thread_context.rip
            << " rsp=0x" << event.thread_context.rsp
            << " rbp=0x" << event.thread_context.rbp
            << " rflags=0x" << event.thread_context.rflags
            << " rax=0x" << event.thread_context.rax
            << " rbx=0x" << event.thread_context.rbx
            << " rcx=0x" << event.thread_context.rcx
            << " rdx=0x" << event.thread_context.rdx
            << " rsi=0x" << event.thread_context.rsi
            << " rdi=0x" << event.thread_context.rdi
            << " r8=0x" << event.thread_context.r8
            << " r9=0x" << event.thread_context.r9
            << " r10=0x" << event.thread_context.r10
            << " r11=0x" << event.thread_context.r11
            << " r12=0x" << event.thread_context.r12
            << " r13=0x" << event.thread_context.r13
            << " r14=0x" << event.thread_context.r14
            << " r15=0x" << event.thread_context.r15
            << "\n";
        return out.str();
    }
}
