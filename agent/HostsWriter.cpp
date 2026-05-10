/*
 * HostsWriter.cpp
 */

#include "HostsWriter.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringConverter>
#include <QTextStream>


HostsWriter::HostsWriter( const QString& path ) :
    m_path( path )
{
}


bool HostsWriter::apply( const QStringList& domains )
{
    // Normalize: lowercase, dedupe, sort - so we can compare reliably
    // and produce stable output between runs.
    QStringList normalized;
    {
        QSet<QString> seen;
        for( const QString& d : domains )
        {
            const QString clean = d.trimmed().toLower();
            if( !clean.isEmpty() && !seen.contains( clean ) )
            {
                seen.insert( clean );
                normalized.append( clean );
            }
        }
        normalized.sort();
    }

    QString nonManaged;
    QStringList currentDomains;
    if( !readCurrent( nonManaged, currentDomains ) )
    {
        return false;
    }

    // Idempotency: if managed block already matches, skip the write
    // entirely (avoids needlessly bumping mtime + reduces wear on /etc).
    currentDomains.sort();
    if( currentDomains == normalized )
    {
        return true;
    }

    if( !ensureBackup() )
    {
        return false;
    }

    return writeAtomic( nonManaged, normalized );
}


bool HostsWriter::readCurrent( QString& nonManaged, QStringList& currentDomains )
{
    QFile file( m_path );
    if( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        m_errorString = QStringLiteral("Cannot read ") + m_path
                        + QStringLiteral(": ") + file.errorString();
        return false;
    }

    QTextStream in( &file );
    QString outNonManaged;
    bool inBlock = false;

    while( !in.atEnd() )
    {
        const QString line = in.readLine();
        const QString trimmed = line.trimmed();

        if( trimmed == QLatin1String( BeginMarker ) )
        {
            inBlock = true;
            continue;
        }
        if( trimmed == QLatin1String( EndMarker ) )
        {
            inBlock = false;
            continue;
        }

        if( inBlock )
        {
            // Inside our block: collect blocked domains.
            // Format expected: "0.0.0.0  domain.tld"
            const QStringList parts = trimmed.split( QRegularExpression( QStringLiteral("\\s+") ),
                                                     Qt::SkipEmptyParts );
            if( parts.size() >= 2 && parts[0] == QStringLiteral("0.0.0.0") )
            {
                currentDomains.append( parts[1].toLower() );
            }
        }
        else
        {
            outNonManaged.append( line );
            outNonManaged.append( QLatin1Char('\n') );
        }
    }

    nonManaged = outNonManaged;
    return true;
}


bool HostsWriter::writeAtomic( const QString& nonManaged, const QStringList& domains )
{
    QSaveFile out( m_path );
    if( !out.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        m_errorString = QStringLiteral("Cannot open ") + m_path
                        + QStringLiteral(" for write: ") + out.errorString();
        return false;
    }

    QTextStream stream( &out );
    stream.setEncoding( QStringConverter::Utf8 );

    // Preserve everything that isn't the managed block.
    stream << nonManaged;

    // If we have anything to block, write a fresh managed block.
    if( !domains.isEmpty() )
    {
        // Ensure separation from preceding content
        if( !nonManaged.endsWith( QLatin1Char('\n') ) )
        {
            stream << '\n';
        }
        stream << BeginMarker << '\n';
        for( const QString& d : domains )
        {
            stream << "0.0.0.0\t" << d << '\n';
        }
        stream << EndMarker << '\n';
    }

    if( !out.commit() )
    {
        m_errorString = QStringLiteral("Atomic write failed: ") + out.errorString();
        return false;
    }
    return true;
}


bool HostsWriter::ensureBackup()
{
    const QString backupPath = m_path + QStringLiteral(".veyon-policy.bak");

    // Already backed up - leave it alone (we want the ORIGINAL, not most recent)
    if( QFileInfo::exists( backupPath ) )
    {
        return true;
    }

    QFile src( m_path );
    if( !src.open( QIODevice::ReadOnly ) )
    {
        m_errorString = QStringLiteral("Cannot read ") + m_path
                        + QStringLiteral(" for backup");
        return false;
    }

    QSaveFile dst( backupPath );
    if( !dst.open( QIODevice::WriteOnly ) )
    {
        m_errorString = QStringLiteral("Cannot create backup ") + backupPath;
        return false;
    }

    dst.write( src.readAll() );
    if( !dst.commit() )
    {
        m_errorString = QStringLiteral("Backup commit failed");
        return false;
    }
    return true;
}
