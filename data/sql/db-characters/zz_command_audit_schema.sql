-- mod-command-audit: future DB storage (DISABLED in code until CommandAudit.LogToDatabase=1)
-- Target database: acore_characters
-- worldserver auto-updater will apply this as CREATE TABLE IF NOT EXISTS (idempotent).

CREATE TABLE IF NOT EXISTS `command_audit_log` (
  `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  `logged_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `source` VARCHAR(32) NOT NULL DEFAULT 'game' COMMENT 'game | console/soap',
  `security` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `account_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `account_name` VARCHAR(32) NOT NULL DEFAULT '',
  `character_guid` INT UNSIGNED NOT NULL DEFAULT 0,
  `character_name` VARCHAR(12) NOT NULL DEFAULT '',
  `client_ip` VARCHAR(64) NOT NULL DEFAULT '',
  `target_name` VARCHAR(48) NOT NULL DEFAULT '',
  `target_guid` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `success` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `reason` VARCHAR(64) NOT NULL DEFAULT '',
  `command` VARCHAR(512) NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `idx_logged_at` (`logged_at`),
  KEY `idx_account_id` (`account_id`),
  KEY `idx_character_guid` (`character_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='GM command audit (mod-command-audit); writes not enabled yet';