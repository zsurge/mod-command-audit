/*
 * mod-command-audit — implementation
 */

#include "command_audit.h"

#include "AccountMgr.h"
#include "Common.h"
#include "Config.h"
#include "Log.h"
#include "Player.h"
#include "WorldSession.h"

#include <algorithm>
#include <ctime>
#include <cctype>
#include <fstream>
#include <sstream>

namespace
{
    std::mutex g_fileMutex;
    std::string g_resolvedLogPath;
}

namespace CommandAudit
{
    Config& GetConfig()
    {
        static Config cfg;
        return cfg;
    }

    std::string ToLower(std::string_view s)
    {
        std::string out(s);
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    void ReloadConfig()
    {
        Config& cfg = GetConfig();
        cfg.Enabled = sConfigMgr->GetOption<bool>("CommandAudit.Enable", true);
        cfg.MinSecurity = sConfigMgr->GetOption<uint32>("CommandAudit.MinSecurity", static_cast<uint32>(SEC_GAMEMASTER));
        cfg.LogFile = sConfigMgr->GetOption<std::string>("CommandAudit.LogFile", "CommandAudit.log");
        cfg.LogToDatabase = sConfigMgr->GetOption<bool>("CommandAudit.LogToDatabase", false);

        cfg.Exclude.clear();
        std::string excludeRaw = sConfigMgr->GetOption<std::string>(
            "CommandAudit.ExcludeCommands",
            "gps,lookup,help,server info,server motd,appear,summon,pname,goname,namego,recall,modify speed,gps");

        std::string token;
        std::stringstream ss(excludeRaw);
        while (std::getline(ss, token, ','))
        {
            // trim
            auto start = token.find_first_not_of(" \t\r\n");
            auto end = token.find_last_not_of(" \t\r\n");
            if (start == std::string::npos)
                continue;
            std::string trimmed = token.substr(start, end - start + 1);
            if (!trimmed.empty())
                cfg.Exclude.insert(ToLower(trimmed));
        }

        std::string logsDir = sConfigMgr->GetOption<std::string>("LogsDir", "");
        if (!logsDir.empty() && logsDir.back() != '/' && logsDir.back() != '\\')
            logsDir.push_back('/');
        g_resolvedLogPath = logsDir + cfg.LogFile;

        LOG_INFO("module", "mod-command-audit: Enable={} MinSecurity={} LogFile={} ExcludeCount={} LogToDatabase={} (DB writes not implemented)",
            cfg.Enabled, cfg.MinSecurity, g_resolvedLogPath, cfg.Exclude.size(), cfg.LogToDatabase);
    }

    bool IsExcluded(std::string_view cmdStr)
    {
        Config const& cfg = GetConfig();
        if (cfg.Exclude.empty())
            return false;

        std::string lower = ToLower(cmdStr);
        // strip leading dots/spaces
        while (!lower.empty() && (lower.front() == '.' || lower.front() == ' '))
            lower.erase(lower.begin());

        for (std::string const& prefix : cfg.Exclude)
        {
            if (lower == prefix)
                return true;
            if (lower.size() > prefix.size() && lower.compare(0, prefix.size(), prefix) == 0)
            {
                char next = lower[prefix.size()];
                if (next == ' ' || next == '\t')
                    return true;
            }
        }
        return false;
    }

    void WriteLine(std::string const& line)
    {
        // Always mirror to AC logger filter (wire Appender in worldserver.conf if desired)
        LOG_INFO("command.audit", "{}", line);

        if (g_resolvedLogPath.empty())
            return;

        std::lock_guard<std::mutex> lock(g_fileMutex);
        std::ofstream out(g_resolvedLogPath, std::ios::out | std::ios::app);
        if (!out.is_open())
        {
            LOG_ERROR("module", "mod-command-audit: cannot open log file '{}'", g_resolvedLogPath);
            return;
        }
        out << line << '\n';
    }
}

CommandAuditWorld::CommandAuditWorld()
    : WorldScript("CommandAuditWorld", { WORLDHOOK_ON_AFTER_CONFIG_LOAD })
{
}

void CommandAuditWorld::OnAfterConfigLoad(bool /*reload*/)
{
    CommandAudit::ReloadConfig();
}

CommandAuditCommandSC::CommandAuditCommandSC()
    : AllCommandScript("CommandAuditCommandSC", {
          ALLCOMMANDHOOK_ON_AFTER_EXECUTE_COMMAND
      })
{
}

void CommandAuditCommandSC::OnAfterExecuteCommand(ChatHandler& handler, std::string_view cmdStr, bool success, std::string_view reason)
{
    CommandAudit::Config const& cfg = CommandAudit::GetConfig();
    if (!cfg.Enabled)
        return;

    if (CommandAudit::IsExcluded(cmdStr))
        return;

    uint32 security = 0;
    uint32 accountId = 0;
    std::string accountName = "-";
    std::string charName = "-";
    std::string charGuid = "-";
    std::string ip = "-";
    std::string target = "-";
    std::string source = "game";

    if (handler.IsConsole())
    {
        security = static_cast<uint32>(SEC_CONSOLE);
        accountName = "console";
        source = "console/soap";
    }
    else if (WorldSession* session = handler.GetSession())
    {
        security = static_cast<uint32>(session->GetSecurity());
        accountId = session->GetAccountId();
        ip = session->GetRemoteAddress();
        AccountMgr::GetName(accountId, accountName);
        if (accountName.empty())
            accountName = "-";

        if (Player* player = session->GetPlayer())
        {
            charName = player->GetName();
            charGuid = player->GetGUID().ToString();
            if (Unit* selected = player->GetSelectedUnit())
                target = std::string(selected->GetName()) + " (" + selected->GetGUID().ToString() + ")";
        }
    }
    else
    {
        security = static_cast<uint32>(SEC_CONSOLE);
        accountName = "console";
        source = "console/soap";
    }

    if (security < cfg.MinSecurity)
        return;

    // Timestamp: use game time string
    std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#if defined(_WIN32) || defined(WIN32)
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmBuf);

    std::ostringstream oss;
    oss << timeBuf
        << " | src=" << source
        << " | sec=" << security
        << " | acct=" << accountName << "(" << accountId << ")"
        << " | char=" << charName << "(" << charGuid << ")"
        << " | ip=" << ip
        << " | target=" << target
        << " | result=" << (success ? "OK" : "FAIL")
        << " | reason=" << reason
        << " | cmd=." << cmdStr;

    CommandAudit::WriteLine(oss.str());

    // Reserved for future: cfg.LogToDatabase — do not write yet
    (void)cfg.LogToDatabase;
}

void AddSC_CommandAudit()
{
    new CommandAuditWorld();
    new CommandAuditCommandSC();
}