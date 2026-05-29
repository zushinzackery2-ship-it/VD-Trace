#ifndef VDTRACE_INTEGER_HEX_H
#define VDTRACE_INTEGER_HEX_H

#include <iomanip>
#include <sstream>
#include <string>

namespace vdtrace
{
    inline std::string FormatLittleEndianIntegerHex(const uint8_t *bytes, uint8_t size)
    {
        if (bytes == nullptr || size == 0)
        {
            return "0x0";
        }

        std::ostringstream out;
        out << "0x";

        bool emitted = false;
        for (uint8_t index = 0; index < size; ++index)
        {
            const uint8_t value = bytes[size - 1 - index];
            if (!emitted)
            {
                if (value == 0 && index + 1 < size)
                {
                    continue;
                }

                out << std::hex << static_cast<unsigned>(value);
                emitted = true;
                continue;
            }

            out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(value);
        }

        if (!emitted)
        {
            out << '0';
        }

        return out.str();
    }
}

#endif
