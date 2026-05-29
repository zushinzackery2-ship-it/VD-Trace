#include "tests/decrypt_smoke/vdtrace_decrypt_smoke_support.h"
#include "VDTrace/VDTrace.h"

#include <Windows.h>

#include <iostream>

namespace
{
    using decrypt_smoke_test::CryptBufferFn;
    using decrypt_smoke_test::HelperApi;
    using decrypt_smoke_test::Require;

    volatile LONG g_ready_flag = 0;
    volatile LONG g_start_flag = 0;
    volatile LONG g_decrypt_ok = 0;

    CryptBufferFn g_decrypt_buffer = nullptr;
    uint8_t g_cipher_buffer[512] = {};
    size_t g_cipher_size = 0;

    __declspec(noinline) void AfterDecryptShouldNotTrace()
    {
        InterlockedExchangeAdd(&g_decrypt_ok, 0);
    }

    __declspec(noinline) void RootDecryptEntry()
    {
        const BOOL ok = g_decrypt_buffer != nullptr ? g_decrypt_buffer(g_cipher_buffer, g_cipher_size) : FALSE;
        InterlockedExchange(&g_decrypt_ok, ok != FALSE ? 1 : 0);
        AfterDecryptShouldNotTrace();
    }

    DWORD WINAPI WorkerThreadMain(LPVOID)
    {
        InterlockedExchange(&g_ready_flag, 1);
        while (InterlockedCompareExchange(&g_start_flag, 0, 0) == 0)
        {
            YieldProcessor();
        }

        RootDecryptEntry();
        return 0;
    }

    void ValidateLogText(const std::wstring &log_text, const HelperApi &api, const std::vector<uint8_t> &encrypted, uintptr_t forbidden_rel)
    {
        const uintptr_t dynamic_stage_base = api.get_dynamic_stage_base();
        Require(!log_text.empty(), L"解密 smoke 日志为空。");
        Require(log_text.find(L"VDTraceDecryptSmokeHelper.dll!DecryptSmokeDecryptBuffer") != std::wstring::npos, L"日志里没有 helper 解密入口。");
        Require(log_text.find(L"target_abs=" + decrypt_smoke_test::FormatHexNeedle(api.get_pipeline())) != std::wstring::npos, L"日志里没有 helper 解密主 pipeline。");
        Require(log_text.find(L"target_abs=" + decrypt_smoke_test::FormatHexNeedle(api.get_expand_round_keys())) != std::wstring::npos, L"日志里没有 round key 展开函数。");
        Require(log_text.find(L"target_abs=" + decrypt_smoke_test::FormatHexNeedle(api.get_pre_whiten_stage())) != std::wstring::npos, L"日志里没有预处理阶段。");
        Require(log_text.find(L"target_abs=" + decrypt_smoke_test::FormatHexNeedle(api.get_dispatch_dynamic_stage())) != std::wstring::npos, L"日志里没有匿名页调度阶段。");
        Require(log_text.find(L"target_abs=" + decrypt_smoke_test::FormatHexNeedle(api.get_chunk_mirror_stage())) != std::wstring::npos, L"日志里没有块镜像阶段。");
        Require(log_text.find(L"target_abs=" + decrypt_smoke_test::FormatHexNeedle(api.get_tail_whiten_stage())) != std::wstring::npos, L"日志里没有尾部收束阶段。");
        Require(log_text.find(L"[ENTER_DYNAMIC_MEMORY_") != std::wstring::npos, L"日志里没有进入动态执行页标记。");
        Require(log_text.find(L"[LEAVE_DYNAMIC_MEMORY_") != std::wstring::npos, L"日志里没有离开动态执行页标记。");
        Require(log_text.find(L"anon-exec@" + decrypt_smoke_test::FormatHexNeedle(dynamic_stage_base)) != std::wstring::npos, L"日志里没有匿名执行页地址。");
        Require(log_text.find(L"[ctx] rip=0x") != std::wstring::npos, L"日志里没有线程上下文块。");
        Require(log_text.find(decrypt_smoke_test::FormatPreviewNeedle(encrypted, 8u)) != std::wstring::npos, L"日志里没有密文缓冲区预览。");
        Require(log_text.find(L"[sample] arg0@0x") != std::wstring::npos, L"日志里没有跨模块参数增强采样。");
        Require(log_text.find(L"ascii_after=\"VDTrace-Dynamic-Decrypt-Smoke-An") != std::wstring::npos, L"日志里没有匿名页解密后的明文快照。");

        wchar_t forbidden_pattern[64] = {};
        swprintf_s(forbidden_pattern, L"rel=0x%llx", static_cast<unsigned long long>(forbidden_rel));
        Require(log_text.find(forbidden_pattern) == std::wstring::npos, L"rootstop 之后仍然 trace 到了解密后的噪音函数。");
    }
}

int wmain()
{
    HelperApi api = {};
    Require(decrypt_smoke_test::LoadHelperApi(api), L"加载或解析解密 smoke helper 失败。");
    g_decrypt_buffer = api.decrypt_buffer;

    std::filesystem::remove(decrypt_smoke_test::BuildPlainPath());
    std::filesystem::remove(decrypt_smoke_test::BuildEncryptedPath());
    std::filesystem::remove(decrypt_smoke_test::BuildDecryptedPath());
    const std::vector<uint8_t> plain = decrypt_smoke_test::BuildPlainPayload();
    Require(plain.size() <= sizeof(g_cipher_buffer), L"测试明文超出缓冲区。");
    Require(decrypt_smoke_test::WriteBinaryFile(decrypt_smoke_test::BuildPlainPath(), plain), L"写入明文文件失败。");

    g_cipher_size = plain.size();
    std::memcpy(g_cipher_buffer, plain.data(), g_cipher_size);
    Require(api.encrypt_buffer(g_cipher_buffer, g_cipher_size) != FALSE, L"生成密文缓冲失败。");
    const std::vector<uint8_t> encrypted(g_cipher_buffer, g_cipher_buffer + g_cipher_size);
    Require(encrypted != plain, L"密文文件没有正确生成。");
    Require(decrypt_smoke_test::WriteBinaryFile(decrypt_smoke_test::BuildEncryptedPath(), encrypted), L"写入密文文件失败。");

    std::filesystem::remove(decrypt_smoke_test::BuildLogPath());

    HANDLE worker_thread = CreateThread(nullptr, 0, WorkerThreadMain, nullptr, 0, nullptr);
    Require(worker_thread != nullptr, L"创建解密 smoke 线程失败。");
    while (InterlockedCompareExchange(&g_ready_flag, 0, 0) == 0)
    {
        Sleep(0);
    }

    const std::wstring exe_name = decrypt_smoke_test::GetFilenameOnly(decrypt_smoke_test::GetExecutablePath());
    const uintptr_t module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const uintptr_t trigger_address = reinterpret_cast<uintptr_t>(&RootDecryptEntry) - module_base;
    const uintptr_t forbidden_rel = reinterpret_cast<uintptr_t>(&AfterDecryptShouldNotTrace) - module_base;

    std::wstring error;
    uint64_t event_count = 0;
    {
        vdtrace::TextFileRecorder recorder(decrypt_smoke_test::BuildLogPath());
        Require(recorder.IsOpen(), L"打开解密 smoke 日志失败。");

        vdtrace::Session session;
        vdtrace::Options options = {};
        options.thread_id = GetThreadId(worker_thread);
        options.module_names = {exe_name};
        options.max_events = 40000;
        options.trace_outside_modules = true;
        options.backend = vdtrace::TraceBackend::TfFullTrace;
        options.control_flow_only = true;
        options.max_call_depth = 10;
        options.hit_policy = vdtrace::FlowHitPolicy::FirstSeen;
        options.hot_bypass_threshold = 4;
        options.enhanced_sampling = true;
        options.trigger_module_name = exe_name;
        options.trigger_address = trigger_address;
        options.stop_on_root_return = true;
        options.callback = vdtrace::TextFileRecorder::Callback;
        options.callback_context = &recorder;

        Require(session.Configure(options, error), L"Configure 失败。");
        Require(session.Start(error), L"Start 失败。");
        InterlockedExchange(&g_start_flag, 1);
        Require(WaitForSingleObject(worker_thread, 3000) == WAIT_OBJECT_0, L"解密 smoke 工作线程没有退出。");
        CloseHandle(worker_thread);
        Require(InterlockedCompareExchange(&g_decrypt_ok, 0, 0) != 0, L"helper 解密返回失败。");

        const ULONGLONG deadline = GetTickCount64() + 3000u;
        while (session.IsRunning() && GetTickCount64() < deadline)
        {
            Sleep(1);
        }
        session.Stop(error);
        Require(!session.IsRunning(), L"解密 smoke 没有自动停。");
        event_count = session.EventCount();
    }

    const std::vector<uint8_t> decrypted(g_cipher_buffer, g_cipher_buffer + g_cipher_size);
    Require(decrypted == plain, L"解密结果与明文不一致。");
    Require(decrypt_smoke_test::WriteBinaryFile(decrypt_smoke_test::BuildDecryptedPath(), decrypted), L"写入解密文件失败。");

    decrypt_smoke_test::WaitForStableFile(decrypt_smoke_test::BuildLogPath());
    ValidateLogText(decrypt_smoke_test::ReadAllText(decrypt_smoke_test::BuildLogPath()), api, encrypted, forbidden_rel);

    std::wcout << L"Decrypt smoke passed, events=" << event_count << L"\n";
    decrypt_smoke_test::FreeHelperApi(api);
    return 0;
}
