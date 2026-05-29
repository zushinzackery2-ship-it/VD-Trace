#include "pch.h"
#include "core/static_refs/VDTraceStaticRefsInternal.h"

namespace vdtrace::static_refs_detail
{
    void ExpandStaticPointerHit(StaticReferenceHit &hit)
    {
        if (hit.size < sizeof(uintptr_t))
        {
            return;
        }

        uintptr_t pointer_value = 0;
        std::memcpy(&pointer_value, hit.bytes, sizeof(pointer_value));
        if (!LooksLikePointerValue(pointer_value))
        {
            return;
        }

        MEMORY_BASIC_INFORMATION information = {};
        if (!IsReadableAddress(pointer_value, information))
        {
            return;
        }

        const uintptr_t region_base = reinterpret_cast<uintptr_t>(information.BaseAddress);
        const size_t region_offset = pointer_value >= region_base ? static_cast<size_t>(pointer_value - region_base) : 0;
        const size_t available = information.RegionSize > region_offset ? information.RegionSize - region_offset : 0;
        if (available == 0)
        {
            return;
        }

        hit.has_dereference = true;
        hit.dereference_address = pointer_value;
        hit.dereference_size = static_cast<uint8_t>(std::min<size_t>(available, kEnhancedSampleMaxBytes));
        if (!SafeReadMemoryBytes(pointer_value, hit.dereference_bytes, hit.dereference_size))
        {
            hit.has_dereference = false;
            hit.dereference_address = 0;
            hit.dereference_size = 0;
            return;
        }

        if (IsExecutableAddress(pointer_value))
        {
            hit.dereference_guess = "code_ptr";
            return;
        }

        std::string text;
        if (TryDecodeAsciiString(hit.dereference_bytes, hit.dereference_size, text))
        {
            hit.dereference_guess = "ascii_string";
            hit.dereference_detail = "text=\"" + EscapePreviewText(text) + "\"";
            return;
        }

        if (TryDecodeUtf16String(hit.dereference_bytes, hit.dereference_size, text))
        {
            hit.dereference_guess = "utf16_string";
            hit.dereference_detail = "text=\"" + EscapePreviewText(text) + "\"";
            return;
        }

        if (hit.dereference_size >= 2 && hit.dereference_bytes[0] == 'M' && hit.dereference_bytes[1] == 'Z')
        {
            hit.dereference_guess = "pe_image";
            return;
        }

        std::string detail;
        if (TryGuessObjectWithVftable(hit.dereference_bytes, hit.dereference_size, detail))
        {
            hit.dereference_guess = "object_with_vftable";
            hit.dereference_detail = detail;
            return;
        }

        if (TryGuessPointerArray(hit.dereference_bytes, hit.dereference_size, detail))
        {
            hit.dereference_guess = "pointer_array";
            hit.dereference_detail = detail;
            return;
        }

        hit.dereference_guess = "blob";
    }

    bool CaptureStaticReference(uintptr_t address, uint8_t operand_size, StaticReferenceHit &hit)
    {
        hit = {};

        AddressLabel label = {};
        if (!ResolveAddressLabel(address, label))
        {
            return false;
        }

        if (!ResolveSectionName(label.module_base, address, hit.section_name, std::size(hit.section_name)))
        {
            return false;
        }

        hit.address = address;
        hit.size = static_cast<uint8_t>(std::min<uint8_t>(std::max<uint8_t>(operand_size, 16), kEnhancedSampleMaxBytes));
        if (!SafeReadMemoryBytes(address, hit.bytes, hit.size))
        {
            return false;
        }

        if (LooksLikeString(hit.bytes, hit.size) || LooksLikeCodePointerTable(hit.bytes, hit.size))
        {
            return false;
        }

        ExpandStaticPointerHit(hit);
        return true;
    }
}
