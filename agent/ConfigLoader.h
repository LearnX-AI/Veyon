/*
 * ConfigLoader.h - Reads /etc/veyon-policy-agent/config.conf
 *
 * Format:
 *   # comment
 *   key = value
 *
 * Required keys: server_url, admin_token
 * Optional keys: hostname (defaults to OS hostname),
 *                heartbeat_interval_seconds (defaults to 30),
 *                hosts_file (defaults to /etc/hosts),
 *                file_destination_dir (defaults to /var/lib/veyon/files/distributed),
 *                file_check_interval_seconds (defaults to 10),
 *                submissions_root_dir (defaults to /var/lib/veyon/submissions),
 *                folder_sync_interval_seconds (defaults to 15),
 *                session_check_interval_seconds (defaults to 10)
 */

#pragma once

#include <QString>


class ConfigLoader
{
public:
    static constexpr const char* DefaultPath = "/etc/veyon-policy-agent/config.conf";

    QString serverUrl;
    QString adminToken;
    QString hostname;
    int heartbeatIntervalSeconds = 30;
    QString hostsFile = QStringLiteral("/etc/hosts");

    // ---- File distribution ----
    QString fileDestinationDir = QStringLiteral("/var/lib/veyon/files/distributed");
    int fileCheckIntervalSeconds = 10;

    // ---- Shared folders / submissions ----
    QString submissionsRootDir = QStringLiteral("/var/lib/veyon/submissions");
    int folderSyncIntervalSeconds = 15;

    // ---- Time-limited sessions ----
    int sessionCheckIntervalSeconds = 10;

    /// Load config. Returns false (and populates errorString) on failure.
    bool load( const QString& path = QString::fromLatin1( DefaultPath ) );

    QString errorString;
};
