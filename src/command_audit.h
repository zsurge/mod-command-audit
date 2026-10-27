/*
 * mod-command-audit — audit GM (and above) chat/console/SOAP commands
 * Independent AzerothCore module. Requires core hook OnAfterExecuteCommand.
 */

#ifndef MODULE_COMMAND_AUDIT_H
#define MODULE_COMMAND_AUDIT_H

#include "ScriptMgr.h"
#include "Chat.h"
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

class CommandAuditWorld : public WorldScript
{
public:
    CommandAuditWorld();

    void OnAfterConfigLoad(bool reload) override;
};

class CommandAuditCommandSC : public AllCommandScript
{
public:
    CommandAuditCommandSC();

    void OnAfterExecuteCommand(ChatHandler& handler, std::string_view cmdStr, bool success, std::string_view reason) override;
};

namespace CommandAudit
{
    struct Config
    {
        bool Enabled = true;
        uint32 MinSecurity = 2; // SEC_GAMEMASTER
        std::string LogFile = "CommandAudit.log";
        bool LogToDatabase = false; // reserved, not implemented
        std::unordered_set<std::string> Exclude; // lower-case prefixes
    };

    Config& GetConfig();
    void ReloadConfig();
    void WriteLine(std::string const& line);
    bool IsExcluded(std::string_view cmdStr);
    std::string ToLower(std::string_view s);
}

void AddSC_CommandAudit();

#endif