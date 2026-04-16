#include "session_smoke_cli.h"

#include <cstdio>
#include <cstring>
#include <cwchar>
namespace session_smoke
{
    namespace
    {
        constexpr const char *kStabilityGroupCaseName = "stability-rounds";

        const char *const kCoreCaseNames[] = {
            "same-level",
            "repeat",
            "helper-edge-only",
            "helper-follow",
            "outside-depth-filter",
            "outside-tf-filter",
            "module-depth-filter",
            "module-tf-filter",
            "anonymous-depth-filter",
            "anonymous-tf-filter",
            "system-module",
            "all-events",
            "heap-extend",
            "heap-peek-size",
            "static-window-sample",
            "static-refs",
            "value-probe",
            "observer-step",
            "observer-write",
            "hot-loop-bypass",
            "hot-loop-bypass-disabled",
            "scene-hot-loop-bypass",
            "scene-hot-loop-rootstop",
            "max-events",
            "write-accounting",
            "auto-thread-capture",
            "auto-thread-capture-delayed",
            "auto-thread-delayed-all-events",
            "main-thread-competition",
            "block-main-thread",
            "unity-worker-precreated",
            "unity-worker-follow-helper",
            "trigger-queue-rotation",
            "probe-queue-rotation",
        };

        const char *const kDefaultCoreCaseNames[] = {
            "same-level",
            "outside-depth-filter",
            "anonymous-depth-filter",
            "heap-extend",
            "heap-peek-size",
            "static-window-sample",
            "static-refs",
            "hot-loop-bypass",
            "auto-thread-capture",
        };

        const char *const kStabilityCaseNames[] = {
            "delayed-auto-thread",
            "unity-worker",
            "unity-worker-follow",
            "probe",
            "probe-queue",
            "outside-depth-filter",
            "outside-tf-filter",
            "module-depth-filter",
            "module-tf-filter",
            "anonymous-depth-filter",
            "anonymous-tf-filter",
        };

        std::string NarrowAscii(const wchar_t *text)
        {
            std::string result;
            if (text == nullptr)
            {
                return result;
            }

            for (const wchar_t *cursor = text; *cursor != L'\0'; ++cursor)
            {
                if (*cursor < 0 || *cursor > 0x7f)
                {
                    Require(false, "session smoke option must be ASCII");
                }

                result.push_back(static_cast<char>(*cursor));
            }

            return result;
        }

        bool ContainsName(const char *const *names, size_t count, const std::string &value)
        {
            for (size_t index = 0; index < count; ++index)
            {
                if (value == names[index])
                {
                    return true;
                }
            }

            return false;
        }

    }

    SessionSmokeSelection ParseSessionSmokeSelection(int argc, wchar_t **argv)
    {
        SessionSmokeSelection selection = {};
        for (int index = 1; index < argc; ++index)
        {
            const std::wstring option = argv[index];
            if (option == L"--list-cases")
            {
                selection.list_cases = true;
                continue;
            }

            if (option == L"--full-core")
            {
                selection.full_core = true;
                continue;
            }

            if (option == L"--case")
            {
                Require(index + 1 < argc, "missing value for --case");
                const std::string value = NarrowAscii(argv[++index]);
                Require(
                    value == kStabilityGroupCaseName || ContainsName(kCoreCaseNames, _countof(kCoreCaseNames), value),
                    "unknown --case value");
                selection.cases.insert(value);
                continue;
            }

            if (option == L"--stability-case")
            {
                Require(index + 1 < argc, "missing value for --stability-case");
                const std::string value = NarrowAscii(argv[++index]);
                Require(ContainsName(kStabilityCaseNames, _countof(kStabilityCaseNames), value), "unknown --stability-case value");
                selection.stability_cases.insert(value);
                selection.cases.insert(kStabilityGroupCaseName);
                continue;
            }

            if (option == L"--rounds")
            {
                Require(index + 1 < argc, "missing value for --rounds");
                const unsigned long parsed = std::wcstoul(argv[++index], nullptr, 10);
                Require(parsed != 0, "round count must be positive");
                selection.rounds = static_cast<uint32_t>(parsed);
                continue;
            }

            Require(false, "unknown session smoke option");
        }

        return selection;
    }

    void PrintSessionSmokeCases()
    {
        std::printf("[cases] core\n");
        for (const char *name : kCoreCaseNames)
        {
            std::printf("  %s\n", name);
        }

        std::printf("[cases] default-core\n");
        for (const char *name : kDefaultCoreCaseNames)
        {
            std::printf("  %s\n", name);
        }

        std::printf("[cases] group\n");
        std::printf("  %s\n", kStabilityGroupCaseName);

        std::printf("[cases] stability-subcase\n");
        for (const char *name : kStabilityCaseNames)
        {
            std::printf("  %s\n", name);
        }

        std::fflush(stdout);
    }

    bool ShouldRunCase(const SessionSmokeSelection &selection, const char *name)
    {
        if (selection.cases.empty())
        {
            return std::strcmp(name, kStabilityGroupCaseName) != 0
                && (selection.full_core || ContainsName(kDefaultCoreCaseNames, _countof(kDefaultCoreCaseNames), name));
        }

        return selection.cases.find(name) != selection.cases.end();
    }

    bool ShouldRunStabilityCase(const SessionSmokeSelection &selection, const char *name)
    {
        return selection.stability_cases.empty()
            || selection.stability_cases.find(name) != selection.stability_cases.end();
    }

    const char *StabilityGroupCaseName()
    {
        return kStabilityGroupCaseName;
    }

    void AnnounceSessionSmokeCase(const char *name)
    {
        std::printf("[case] %s\n", name);
        std::fflush(stdout);
    }

    std::wstring RoundLog(const wchar_t *prefix, int round)
    {
        return std::wstring(prefix) + std::to_wstring(round) + L".log";
    }
}
