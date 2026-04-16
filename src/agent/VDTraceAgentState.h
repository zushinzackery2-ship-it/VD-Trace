#ifndef VDTRACE_AGENT_STATE_H
#define VDTRACE_AGENT_STATE_H

#include "VDTrace/VDTrace.h"
#include "VDTrace/VDTraceIpc.h"

namespace vdtrace::agent
{
    class State
    {
      public:
        static State &Instance();

        bool Configure(const IpcConfigurePayload &payload, std::string &message);
        bool Start(std::string &message);
        bool Stop(std::string &message);
        std::string Status() const;
        bool ListModules(bool include_system_modules, std::string &message) const;
        bool DumpModule(const char *module_name, const char *output_directory, std::string &message) const;
        bool ReadMemory(const char *address_text, uint32_t size, std::string &message) const;
        bool WriteMemory(const char *address_text, const uint8_t *bytes, uint32_t size, std::string &message) const;

      private:
        State() = default;

        static std::vector<std::wstring> ParseModuleNames(const char *text);
        static bool ParseTriggerPoint(const char *text, std::wstring &module_name, uintptr_t &address, std::string &message);
        static std::filesystem::path GetAgentModuleDirectory();
        static std::wstring NormalizeOutputPath(const std::wstring &text);
        static std::wstring WidenUtf8(const char *text);
        static std::string NarrowUtf8(const std::wstring &text);

        mutable std::mutex lock_;
        vdtrace::Session session_;
        std::unique_ptr<vdtrace::TextFileRecorder> recorder_;
        std::wstring output_path_;
        bool has_valid_configuration_ = false;
    };
}

#endif
