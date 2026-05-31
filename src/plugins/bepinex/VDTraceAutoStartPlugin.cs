using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using BepInEx;
using BepInEx.Unity.IL2CPP;

namespace VDTraceAutoStartPlugin;

[BepInPlugin("vdtrace.autostart.plugin", "VDTrace AutoStart Plugin", "1.0.0")]
public sealed class VdTraceAutoStartPlugin : BasePlugin
{
    private static bool _activated;

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern nint LoadLibraryW(string fileName);

    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern nint GetProcAddress(nint module, string procName);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate bool BootstrapDelegate();

    public override void Load()
    {
        if (_activated)
        {
            return;
        }

        string activationPath = Path.Combine(AppContext.BaseDirectory, "VDTraceAutoStart.activate.ini");
        if (!File.Exists(activationPath))
        {
            return;
        }

        Dictionary<string, string> values = ParseActivationFile(activationPath);
        if (!values.TryGetValue("helper_path", out string? helperPath) || string.IsNullOrWhiteSpace(helperPath))
        {
            Log.LogError("activation file missing helper_path");
            return;
        }

        if (values.TryGetValue("config_path", out string? configPath) && !string.IsNullOrWhiteSpace(configPath))
        {
            Environment.SetEnvironmentVariable("VDTRACE_AUTOSTART_CONFIG", configPath);
        }

        if (values.TryGetValue("log_path", out string? logPath) && !string.IsNullOrWhiteSpace(logPath))
        {
            Environment.SetEnvironmentVariable("VDTRACE_AUTOSTART_LOG", logPath);
        }

        IntPtr helper = LoadLibraryW(helperPath);
        if (helper == IntPtr.Zero)
        {
            Log.LogError($"LoadLibraryW failed: {helperPath}");
            return;
        }

        IntPtr bootstrapPtr = GetProcAddress(helper, "vdtrace_loader_bootstrap_immediate");
        if (bootstrapPtr == IntPtr.Zero)
        {
            Log.LogError("vdtrace_loader_bootstrap_immediate export not found");
            return;
        }

        BootstrapDelegate bootstrap = Marshal.GetDelegateForFunctionPointer<BootstrapDelegate>(bootstrapPtr);
        if (!bootstrap())
        {
            Log.LogError("vdtrace_loader_bootstrap_immediate returned false");
            return;
        }

        _activated = true;
        Log.LogMessage("VDTrace autostart helper loaded");
    }

    private static Dictionary<string, string> ParseActivationFile(string path)
    {
        Dictionary<string, string> values = new(StringComparer.OrdinalIgnoreCase);
        foreach (string rawLine in File.ReadAllLines(path))
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith(";") || line.StartsWith("#") || line.StartsWith("["))
            {
                continue;
            }

            int split = line.IndexOf('=');
            if (split <= 0)
            {
                continue;
            }

            string key = line[..split].Trim();
            string value = line[(split + 1)..].Trim();
            values[key] = value;
        }

        return values;
    }
}
