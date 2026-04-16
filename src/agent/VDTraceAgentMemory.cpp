#include "pch.h"
#include "agent/VDTraceAgentMemoryInternal.h"

namespace vdtrace::agent
{
    bool ReadMemoryText(const char *address_text, uint32_t size, std::string &message)
    {
        using namespace memory_detail;

        message.clear();
        if (size == 0 || size > kMaxMemoryTransferSize)
        {
            message = "size must be within 1..512";
            return false;
        }

        uintptr_t address = 0;
        if (!ResolveAddressText(WidenUtf8(address_text), address, message))
        {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
        {
            message = "VirtualQuery failed";
            return false;
        }

        uint8_t bytes[kMaxMemoryTransferSize] = {};
        if (!SafeReadMemoryBytes(address, bytes, size))
        {
            message = "memory is not readable";
            return false;
        }

        message = BuildReadMessage(address, bytes, size, mbi);
        return true;
    }

    bool WriteMemoryText(const char *address_text, const uint8_t *bytes, uint32_t size, std::string &message)
    {
        using namespace memory_detail;

        message.clear();
        if (bytes == nullptr || size == 0 || size > kMaxMemoryTransferSize)
        {
            message = "write size must be within 1..512";
            return false;
        }

        uintptr_t address = 0;
        if (!ResolveAddressText(WidenUtf8(address_text), address, message))
        {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
        {
            message = "VirtualQuery failed";
            return false;
        }

        uint8_t before[kMaxMemoryTransferSize] = {};
        if (!SafeReadMemoryBytes(address, before, size))
        {
            message = "memory is not readable before write";
            return false;
        }

        DWORD old_protect = 0;
        bool changed_protect = false;
        if (!IsWritableProtection(mbi.Protect))
        {
            if (!VirtualProtect(reinterpret_cast<void *>(address), size, PAGE_EXECUTE_READWRITE, &old_protect))
            {
                message = "VirtualProtect failed";
                return false;
            }
            changed_protect = true;
        }

        const bool write_ok = WriteBytes(address, bytes, size);
        if (changed_protect)
        {
            DWORD ignored = 0;
            VirtualProtect(reinterpret_cast<void *>(address), size, old_protect, &ignored);
        }
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(address), size);
        if (!write_ok)
        {
            message = "write failed";
            return false;
        }

        uint8_t after[kMaxMemoryTransferSize] = {};
        if (!SafeReadMemoryBytes(address, after, size))
        {
            message = "memory is not readable after write";
            return false;
        }

        message = BuildWriteMessage(address, before, after, size, mbi);
        return true;
    }
}
