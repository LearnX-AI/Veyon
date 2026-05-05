/*
 * BlocklistLoader.h - reads the Focus Mode blocklist from disk
 *
 * Format: one domain per line. Lines starting with # are comments.
 * Empty lines are ignored. Whitespace is trimmed.
 *
 * Default location: /etc/veyon/focusmode-blocklist.txt
 */

#pragma once

#include <QString>
#include <QStringList>


class BlocklistLoader
{
public:
    static constexpr const char* DefaultPath = "/etc/veyon/focusmode-blocklist.txt";

    /// Load blocklist from the given path.
    /// @return list of domains (cleaned, deduplicated). Empty list on error.
    static QStringList load( const QString& path = QString::fromLatin1( DefaultPath ) );

    /// Validate a single domain entry.
    /// Rejects entries with whitespace, schemes (http://), or paths (/foo).
    static bool isValidDomain( const QString& domain );
};
