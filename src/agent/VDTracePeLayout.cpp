#include "pch.h"
#include "agent/VDTracePeLayout.h"

namespace vdtrace::agent
{
    namespace
    {
        constexpr std::uint32_t kSecurityDirectoryIndex = 4;

        struct SectionPatch
        {
            std::size_t header_offset = 0;
            IMAGE_SECTION_HEADER header = {};
        };

        template <typename T>
        bool ReadAt(const std::vector<std::uint8_t> &bytes, std::size_t offset, T &value)
        {
            return offset <= bytes.size()
                && bytes.size() - offset >= sizeof(T)
                && (std::memcpy(&value, bytes.data() + offset, sizeof(T)), true);
        }

        template <typename T>
        bool WriteAt(std::vector<std::uint8_t> &bytes, std::size_t offset, const T &value)
        {
            return offset <= bytes.size()
                && bytes.size() - offset >= sizeof(T)
                && (std::memcpy(bytes.data() + offset, &value, sizeof(T)), true);
        }

        std::uint32_t AlignUp(std::uint32_t value, std::uint32_t alignment)
        {
            return alignment <= 1 ? value : ((value + alignment - 1) / alignment) * alignment;
        }
    }

    bool NormalizeMemoryDumpPeLayout(std::vector<std::uint8_t> &image_bytes)
    {
        IMAGE_DOS_HEADER dos = {};
        DWORD signature = 0;
        IMAGE_FILE_HEADER file_header = {};
        WORD magic = 0;
        std::size_t nt_offset = 0;
        std::size_t optional_offset = 0;
        std::size_t section_table_offset = 0;
        std::uint32_t file_alignment = 0x200;
        std::uint32_t size_of_image = 0;
        std::uint32_t number_of_rva_and_sizes = 0;
        std::size_t data_directory_offset = 0;
        std::vector<SectionPatch> sections;

        if (!ReadAt(image_bytes, 0, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0)
        {
            return false;
        }

        nt_offset = static_cast<std::size_t>(dos.e_lfanew);
        optional_offset = nt_offset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
        if (!ReadAt(image_bytes, nt_offset, signature)
            || !ReadAt(image_bytes, nt_offset + sizeof(DWORD), file_header)
            || signature != IMAGE_NT_SIGNATURE
            || !ReadAt(image_bytes, optional_offset, magic))
        {
            return false;
        }

        if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        {
            IMAGE_OPTIONAL_HEADER32 optional_header = {};
            if (!ReadAt(image_bytes, optional_offset, optional_header))
            {
                return false;
            }

            file_alignment = optional_header.FileAlignment;
            size_of_image = optional_header.SizeOfImage;
            number_of_rva_and_sizes = optional_header.NumberOfRvaAndSizes;
            data_directory_offset = optional_offset + offsetof(IMAGE_OPTIONAL_HEADER32, DataDirectory);
        }
        else if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            IMAGE_OPTIONAL_HEADER64 optional_header = {};
            if (!ReadAt(image_bytes, optional_offset, optional_header))
            {
                return false;
            }

            file_alignment = optional_header.FileAlignment;
            size_of_image = optional_header.SizeOfImage;
            number_of_rva_and_sizes = optional_header.NumberOfRvaAndSizes;
            data_directory_offset = optional_offset + offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory);
        }
        else
        {
            return false;
        }

        if (file_alignment == 0 || size_of_image == 0 || image_bytes.size() < size_of_image)
        {
            return false;
        }

        section_table_offset = optional_offset + file_header.SizeOfOptionalHeader;
        sections.reserve(file_header.NumberOfSections);
        for (WORD index = 0; index < file_header.NumberOfSections; ++index)
        {
            SectionPatch patch = {};
            patch.header_offset = section_table_offset + index * sizeof(IMAGE_SECTION_HEADER);
            if (!ReadAt(image_bytes, patch.header_offset, patch.header))
            {
                return false;
            }

            sections.push_back(patch);
        }

        std::sort(
            sections.begin(),
            sections.end(),
            [](const SectionPatch &left, const SectionPatch &right)
            {
                return left.header.VirtualAddress < right.header.VirtualAddress;
            });

        for (std::size_t index = 0; index < sections.size(); ++index)
        {
            SectionPatch patch = sections[index];
            IMAGE_SECTION_HEADER section = patch.header;
            std::uint32_t next_rva = size_of_image;
            std::uint32_t available_span = 0;
            std::uint32_t desired_raw_size = std::max(section.Misc.VirtualSize, section.SizeOfRawData);

            if (index + 1 < sections.size())
            {
                next_rva = sections[index + 1].header.VirtualAddress;
            }

            if (next_rva > section.VirtualAddress)
            {
                available_span = next_rva - section.VirtualAddress;
            }

            if (desired_raw_size == 0)
            {
                desired_raw_size = available_span;
            }

            desired_raw_size = AlignUp(desired_raw_size, file_alignment);
            if (available_span != 0)
            {
                desired_raw_size = std::min(desired_raw_size, available_span);
            }

            if (section.VirtualAddress >= image_bytes.size())
            {
                desired_raw_size = 0;
            }
            else
            {
                desired_raw_size = static_cast<std::uint32_t>(
                    std::min<std::size_t>(desired_raw_size, image_bytes.size() - section.VirtualAddress));
            }

            section.PointerToRawData = section.VirtualAddress;
            section.SizeOfRawData = desired_raw_size;
            if (!WriteAt(image_bytes, patch.header_offset, section))
            {
                return false;
            }
        }

        if (number_of_rva_and_sizes > kSecurityDirectoryIndex)
        {
            IMAGE_DATA_DIRECTORY empty_directory = {};
            if (!WriteAt(
                    image_bytes,
                    data_directory_offset + kSecurityDirectoryIndex * sizeof(IMAGE_DATA_DIRECTORY),
                    empty_directory))
            {
                return false;
            }
        }

        return true;
    }
}
