/*
 * HostsWriter.h - Safely manages a managed block in /etc/hosts.
 *
 * The "managed block" is bracketed by marker comments:
 *
 *   # === BEGIN VEYON POLICY ===
 *   0.0.0.0 facebook.com
 *   0.0.0.0 tiktok.com
 *   # === END VEYON POLICY ===
 *
 * Anything outside the markers is left untouched. The block is
 * written atomically via QSaveFile (write-temp-then-rename), so a
 * crash mid-write can never corrupt the existing file.
 *
 * On first write, a one-time backup is created at
 * /etc/hosts.veyon-policy.bak so the original can always be restored.
 */

#pragma once

#include <QString>
#include <QStringList>


class HostsWriter
{
public:
    explicit HostsWriter( const QString& path = QStringLiteral("/etc/hosts") );

    /// Replace the managed block with entries blocking the given domains.
    /// Pass an empty list to remove the managed block entirely.
    /// Returns true on success or if no change was needed.
    bool apply( const QStringList& domains );

    QString errorString() const { return m_errorString; }

private:
    bool readCurrent( QString& nonManaged, QStringList& currentDomains );
    bool writeAtomic( const QString& nonManaged, const QStringList& domains );
    bool ensureBackup();

    static constexpr const char* BeginMarker = "# === BEGIN VEYON POLICY ===";
    static constexpr const char* EndMarker   = "# === END VEYON POLICY ===";

    QString m_path;
    QString m_errorString;
};
