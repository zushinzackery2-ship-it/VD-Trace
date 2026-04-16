#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        std::string FormatSampleBytes(const uint8_t *bytes, uint8_t size)
        {
            std::ostringstream out;
            for (uint8_t index = 0; index < size; ++index)
            {
                if (index != 0)
                {
                    out << ' ';
                }
                out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(bytes[index]);
            }
            return out.str();
        }

        std::string FormatSampleAscii(const uint8_t *bytes, uint8_t size)
        {
            std::string text;
            text.reserve(size);
            for (uint8_t index = 0; index < size; ++index)
            {
                const uint8_t value = bytes[index];
                text.push_back(value >= 0x20 && value <= 0x7e ? static_cast<char>(value) : '.');
            }
            return text;
        }

        std::string SampleName(uint8_t source_index)
        {
            if (source_index == kEnhancedSampleSourceReturnValue)
            {
                return "retval";
            }

            return "arg" + std::to_string(source_index);
        }

        const MemorySample *FindFrameSample(const ActiveCallFrame *frame, const MemorySample &current)
        {
            if (frame == nullptr)
            {
                return nullptr;
            }

            for (uint8_t index = 0; index < frame->memory_sample_count; ++index)
            {
                const MemorySample &candidate = frame->memory_samples[index];
                if (!candidate.valid)
                {
                    continue;
                }

                if (candidate.source_index == current.source_index || candidate.address == current.address)
                {
                    return &candidate;
                }
            }

            return nullptr;
        }
    }

    void RememberEnhancedSamplingFrame(ActiveCallFrame &frame, const RecorderQueuedEvent &event)
    {
        frame.memory_sample_count = event.memory_sample_count;
        std::memcpy(frame.memory_samples, event.memory_samples, sizeof(frame.memory_samples));
    }

    std::string FormatEnhancedSamplingEntryBlock(const RecorderQueuedEvent &event, const std::string &indent)
    {
        if (event.memory_sample_count == 0)
        {
            return {};
        }

        std::ostringstream out;
        for (uint8_t index = 0; index < event.memory_sample_count && index < kEnhancedSampleSlotCount; ++index)
        {
            const MemorySample &sample = event.memory_samples[index];
            if (!sample.valid || sample.size == 0)
            {
                continue;
            }

            out << indent
                << "[sample] " << SampleName(sample.source_index)
                << "@0x" << std::hex << sample.address
                << " pre=" << FormatSampleBytes(sample.bytes, sample.size)
                << " ascii=\"" << FormatSampleAscii(sample.bytes, sample.size) << "\"\n";
        }
        return out.str();
    }

    std::string FormatEnhancedSamplingReturnBlock(
        const RecorderQueuedEvent &event,
        const std::string &indent,
        const ActiveCallFrame *frame)
    {
        std::ostringstream out;
        for (uint8_t index = 0; index < event.memory_sample_count && index < kEnhancedSampleSlotCount; ++index)
        {
            const MemorySample &sample = event.memory_samples[index];
            if (!sample.valid || sample.size == 0)
            {
                continue;
            }

            out << indent
                << "[sample] " << SampleName(sample.source_index)
                << "@0x" << std::hex << sample.address;
            if (const auto *before = FindFrameSample(frame, sample); before != nullptr && before->size != 0)
            {
                out << " before=" << FormatSampleBytes(before->bytes, before->size)
                    << " after=" << FormatSampleBytes(sample.bytes, sample.size)
                    << " ascii_before=\"" << FormatSampleAscii(before->bytes, before->size) << "\""
                    << " ascii_after=\"" << FormatSampleAscii(sample.bytes, sample.size) << "\"";
            }
            else
            {
                out << " post=" << FormatSampleBytes(sample.bytes, sample.size)
                    << " ascii=\"" << FormatSampleAscii(sample.bytes, sample.size) << "\"";
            }
            out << "\n";
        }

        if (event.has_return_memory_sample && event.return_memory_sample.valid && event.return_memory_sample.size != 0)
        {
            const MemorySample &sample = event.return_memory_sample;
            out << indent
                << "[sample] retval@0x" << std::hex << sample.address
                << " mem=" << FormatSampleBytes(sample.bytes, sample.size)
                << " ascii=\"" << FormatSampleAscii(sample.bytes, sample.size) << "\"\n";
        }

        return out.str();
    }
}
