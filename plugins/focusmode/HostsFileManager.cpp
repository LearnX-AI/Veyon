/*
 * HostsFileManager.cpp - safe atomic management of /etc/hosts
 */

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

#include "HostsFileManager.h"


HostsFileManager::HostsFileManager( const QString& hostsPath ) :
    m_hostsPath( hostsPath )
{
}


bool HostsFileManager::isFocusModeActive() const
{
    QStringList lines;
    if( readFile( lines ) == false )
    {
        return false;
    }

    for( const QString& line : lines )
    {
        if( line.trimmed() == QLatin1String( BeginMarker ) )
        {
            return true;
        }
    }
    return false;
}


bool HostsFileManager::applyBlocklist( const QStringList& domains )
{
    QStringList lines;
    if( readFile( lines ) == false )
    {
        return false;
    }

    if( createBackup() == false )
    {
        // backup failure is logged but not fatal — proceed with caution
    }

    // Strip any existing Veyon section
    QStringList cleaned;
    bool inVeyonSection = false;
    for( const QString& line : lines )
    {
        const QString t = line.trimmed();
        if( t == QLatin1String( BeginMarker ) )
        {
            inVeyonSection = true;
            continue;
        }
        if( t == QLatin1String( EndMarker ) )
        {
            inVeyonSection = false;
            continue;
        }
        if( inVeyonSection == false )
        {
            cleaned.append( line );
        }
    }

    // Build the new Veyon section
    QStringList veyonSection;
    veyonSection.append( QString() );   // blank line for readability
    veyonSection.append( QLatin1String( BeginMarker ) );
    veyonSection.append( QStringLiteral( "# Managed by Veyon Focus Mode plugin. Do not edit." ) );
    for( const QString& domain : domains )
    {
        veyonSection.append( QStringLiteral( "0.0.0.0  %1" ).arg( domain ) );
    }
    veyonSection.append( QLatin1String( EndMarker ) );

    // Trim trailing blanks from cleaned, then append our section
    while( cleaned.isEmpty() == false && cleaned.last().trimmed().isEmpty() )
    {
        cleaned.removeLast();
    }
    cleaned.append( veyonSection );

    return writeFileAtomic( cleaned );
}


bool HostsFileManager::clearBlocklist()
{
    QStringList lines;
    if( readFile( lines ) == false )
    {
        return false;
    }

    if( isFocusModeActive() == false )
    {
        return true;   // nothing to do
    }

    if( createBackup() == false )
    {
        // non-fatal — proceed
    }

    QStringList cleaned;
    bool inVeyonSection = false;
    for( const QString& line : lines )
    {
        const QString t = line.trimmed();
        if( t == QLatin1String( BeginMarker ) )
        {
            inVeyonSection = true;
            continue;
        }
        if( t == QLatin1String( EndMarker ) )
        {
            inVeyonSection = false;
            continue;
        }
        if( inVeyonSection == false )
        {
            cleaned.append( line );
        }
    }

    // Trim trailing blanks
    while( cleaned.isEmpty() == false && cleaned.last().trimmed().isEmpty() )
    {
        cleaned.removeLast();
    }

    return writeFileAtomic( cleaned );
}


// ---- private helpers ----

bool HostsFileManager::readFile( QStringList& outLines ) const
{
    QFile file( m_hostsPath );
    if( file.open( QIODevice::ReadOnly | QIODevice::Text ) == false )
    {
        m_lastError = QStringLiteral( "Cannot read %1: %2" ).arg( m_hostsPath, file.errorString() );
        return false;
    }

    outLines.clear();
    QTextStream in( &file );
    while( in.atEnd() == false )
    {
        outLines.append( in.readLine() );
    }
    return true;
}


bool HostsFileManager::writeFileAtomic( const QStringList& lines )
{
    // QSaveFile writes to a temp file, then renames atomically on commit.
    QSaveFile out( m_hostsPath );
    if( out.open( QIODevice::WriteOnly | QIODevice::Text ) == false )
    {
        m_lastError = QStringLiteral( "Cannot write %1: %2" ).arg( m_hostsPath, out.errorString() );
        return false;
    }

    QTextStream stream( &out );
    for( const QString& line : lines )
    {
        stream << line << '\n';
    }
    stream.flush();

    if( out.commit() == false )
    {
        m_lastError = QStringLiteral( "Commit failed: %1" ).arg( out.errorString() );
        return false;
    }

    return true;
}


bool HostsFileManager::createBackup() const
{
    const QString backupPath = m_hostsPath + QLatin1String( BackupSuffix );
    QFile::remove( backupPath );   // remove old backup if present
    return QFile::copy( m_hostsPath, backupPath );
}
