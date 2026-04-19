#include "session_smoke_cli.h"

#include <cstdio>

int wmain(int argc, wchar_t **argv)
{
    const session_smoke::SessionSmokeSelection selection = session_smoke::ParseSessionSmokeSelection(argc, argv);
    if (selection.list_cases)
    {
        session_smoke::PrintSessionSmokeCases();
        return 0;
    }

    session_smoke::InitializeEnvironment();

    const session_smoke::SessionSmokeConfig config = session_smoke::BuildSessionSmokeConfig();
    session_smoke::RunSessionSmokeTraceBasicCases(selection, config);
    session_smoke::RunSessionSmokeTraceFeatureCases(selection, config);
    session_smoke::RunSessionSmokeRecorderCases(selection);
    session_smoke::RunSessionSmokeThreadCases(selection, config);

    session_smoke::ShutdownEnvironment();
    std::printf("[ok] session smoke passed\n");
    return 0;
}
