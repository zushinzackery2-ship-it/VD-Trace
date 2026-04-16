#include "vdtrace_ctl_internal.h"

#include <string>
#include <vector>

int wmain(int argc, wchar_t **argv)
{
    using namespace vdtrace::tools;
    using namespace vdtrace::tools::cli;

    if (argc < 3)
    {
        PrintUsage();
        return 1;
    }

    const std::wstring command_name = argv[1];
    DWORD pid = 0;
    if (!ParseDword(argv[2], pid))
    {
        PrintUsage();
        return 1;
    }

    if (_wcsicmp(command_name.c_str(), L"ping") == 0)
    {
        PrintResult(SendCommand(pid, BuildSimpleCommand(vdtrace::IpcCommandType::Ping), 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"status") == 0)
    {
        PrintResult(SendCommand(pid, BuildSimpleCommand(vdtrace::IpcCommandType::Status), 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"start") == 0)
    {
        PrintResult(SendCommand(pid, BuildSimpleCommand(vdtrace::IpcCommandType::Start), 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"modules") == 0)
    {
        const bool include_system_modules = argc >= 4 && _wcsicmp(argv[3], L"all") == 0;
        PrintResult(ListModules(pid, include_system_modules, 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"dump") == 0)
    {
        if (argc < 4)
        {
            PrintUsage();
            return 1;
        }

        const std::wstring output_directory = argc >= 5 ? argv[4] : BuildDefaultDumpOutputDirectory();
        PrintResult(DumpModule(pid, argv[3], output_directory, 10000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"read") == 0)
    {
        if (argc < 4)
        {
            PrintUsage();
            return 1;
        }

        DWORD size = 64;
        if (argc >= 5 && !ParseDword(argv[4], size))
        {
            PrintUsage();
            return 1;
        }

        PrintResult(ReadMemory(pid, argv[3], size, 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"write") == 0)
    {
        if (argc < 5)
        {
            PrintUsage();
            return 1;
        }

        std::vector<uint8_t> bytes;
        if (!ParseHexBytes(argv[4], bytes))
        {
            PrintUsage();
            return 1;
        }

        PrintResult(WriteMemory(pid, argv[3], bytes.data(), static_cast<uint32_t>(bytes.size()), 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"stop") == 0)
    {
        PrintResult(SendCommand(pid, BuildSimpleCommand(vdtrace::IpcCommandType::Stop), 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"inject") == 0)
    {
        if (argc < 4)
        {
            PrintUsage();
            return 1;
        }

        PrintResult(InjectAgent(pid, argv[3], 5000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"configure") == 0)
    {
        vdtrace::IpcCommand configure = {};
        if (!BuildConfigureCommand(argc, argv, 3, configure))
        {
            PrintUsage();
            return 1;
        }

        PrintResult(SendCommand(pid, configure, 3000));
        return 0;
    }

    if (_wcsicmp(command_name.c_str(), L"quickstart") == 0)
    {
        if (argc < 7)
        {
            PrintUsage();
            return 1;
        }

        const auto inject_result = InjectAgent(pid, argv[3], 5000);
        PrintResult(inject_result);
        if (!inject_result.success)
        {
            return 1;
        }

        vdtrace::IpcCommand configure = {};
        if (!BuildConfigureCommand(argc, argv, 4, configure))
        {
            PrintUsage();
            return 1;
        }

        const auto configure_result = SendCommand(pid, configure, 3000);
        PrintResult(configure_result);
        if (!configure_result.success)
        {
            return 1;
        }

        const auto start_result = SendCommand(pid, BuildSimpleCommand(vdtrace::IpcCommandType::Start), 3000);
        PrintResult(start_result);
        return start_result.success ? 0 : 1;
    }

    PrintUsage();
    return 1;
}
