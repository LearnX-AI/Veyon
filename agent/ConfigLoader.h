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
 *                hosts_file (defaults to /etc/hosts)
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

    /// Load config. Returns false (and populates errorString) on failure.
    bool load( const QString& path = QString::fromLatin1( DefaultPath ) );

    QString errorString;
};
