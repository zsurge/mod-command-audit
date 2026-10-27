# mod-command-audit

AzerothCore module: audit `.` commands executed by GameMaster+ accounts (in-game, worldserver console, SOAP/remote).

## Requirements

- Core hook `ScriptMgr::OnAfterExecuteCommand` (shipped with this integration on the Playerbot / mine tree).
- Rebuild `worldserver` after adding the module.

## Config

Copy `conf/mod_command_audit.conf.dist` to the server `configs/modules/mod_command_audit.conf`.

Key options: `CommandAudit.Enable`, `MinSecurity` (default 2), `LogFile`, `ExcludeCommands`, `LogToDatabase` (reserved, keep 0).

## Verify

1. Start worldserver; look for `mod-command-audit: Enable=...` in the log.
2. As GM: `.additem 25 1` → a line appears in `LogsDir/CommandAudit.log`.
3. As player: run a normal `.` command → no new line.
4. Excluded cmds (e.g. `.gps`) → no line.

## License

AGPL-3.0 (AzerothCore).