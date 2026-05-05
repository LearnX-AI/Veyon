/*
 * HostsFileManager.h - safely manages Veyon Focus Mode entries in /etc/hosts
 *
 * Strategy:
 *   - Our entries live between two marker lines:
 *       # === BEGIN VEYON FOCUS MODE ===
 *       # === END VEYON FOCUS MODE ===
 *   - We only ever modify content between those markers.
 *   - Other lines in /etc/hosts are preserved exactly.
 *   - Writes are atomic (write to temp file, then rename).
 *   - Backup is created on every modification.
 *
 * Requires root privileges to write /etc/hosts.
 */

#pragma once

#include <QString>
#include <QStringList>


class HostsFileManager
{
public:
    static constexpr const char* DefaultPath  = "/etc/hosts";
    static constexpr const char* BackupSuffix = ".veyon.bak";
    static constexpr const char* BeginMarker  = "# === BEGIN VEYON FOCUS MODE ===";
    static constexpr const char* EndMarker    = "# === END VEYON FOCUS MODE ===";

    explicit HostsFileManager( const QString& hostsPath = QString::fromLatin1( DefaultPath ) );

    /// Apply the blocklist by inserting/replacing the Veyon section.
    /// Each domain becomes a line: "0.0.0.0  domain"
    /// @return true on success.
    bool applyBlocklist( const QStringList& domains );

    /// Remove the Veyon section from /etc/hosts entirely.
    /// @return true on success (also true if section was absent).
    bool clearBlocklist();

    /// @return true if the Veyon section currently exists in /etc/hosts.
    bool isFocusModeActive() const;

    QString lastError() const { return m_lastError; }

private:
    bool readFile( QStringList& outLines ) const;
    bool writeFileAtomic( const QStringList& lines );
    bool createBackup() const;

    QString m_hostsPath;
    mutable QString m_lastError;
};
