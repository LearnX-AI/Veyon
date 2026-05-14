/*
 * ConfigLoader.cpp
 */

#include "ConfigLoader.h"

#include <QFile>
#include <QHostInfo>
#include <QTextStream>


bool ConfigLoader::load( const QString& path )
{
    QFile file( path );
    if( !file.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        errorString = QStringLiteral("Cannot open config file: ") + path;
        return false;
    }

    QTextStream in( &file );
    while( !in.atEnd() )
    {
        QString line = in.readLine().trimmed();

        // Skip blank lines and comments
        if( line.isEmpty() || line.startsWith( QLatin1Char('#') ) )
        {
            continue;
        }

        const int eq = line.indexOf( QLatin1Char('=') );
        if( eq <= 0 )
        {
            continue;       // malformed line, ignore silently
        }

        const QString key   = line.left( eq ).trimmed().toLower();
        const QString value = line.mid( eq + 1 ).trimmed();

        if( key == QStringLiteral("server_url") )
        {
            serverUrl = value;
        }
        else if( key == QStringLiteral("admin_token") )
        {
            adminToken = value;
        }
        else if( key == QStringLiteral("hostname") )
        {
            hostname = value;
        }
        else if( key == QStringLiteral("heartbeat_interval_seconds") )
        {
            bool ok = false;
            const int v = value.toInt( &ok );
            if( ok && v >= 5 )
            {
                heartbeatIntervalSeconds = v;
            }
        }
        else if( key == QStringLiteral("hosts_file") )
        {
            hostsFile = value;
        }
        else if( key == QStringLiteral("file_destination_dir") )
        {
            fileDestinationDir = value;
        }
        else if( key == QStringLiteral("file_check_interval_seconds") )
        {
            bool ok = false;
            const int v = value.toInt( &ok );
            if( ok && v >= 2 )
            {
                fileCheckIntervalSeconds = v;
            }
        }
        else if( key == QStringLiteral("submissions_root_dir") )
        {
            submissionsRootDir = value;
        }
        else if( key == QStringLiteral("folder_sync_interval_seconds") )
        {
            bool ok = false;
            const int v = value.toInt( &ok );
            if( ok && v >= 5 )
            {
                folderSyncIntervalSeconds = v;
            }
        }
        else if( key == QStringLiteral("session_check_interval_seconds") )
        {
            bool ok = false;
            const int v = value.toInt( &ok );
            if( ok && v >= 2 )
            {
                sessionCheckIntervalSeconds = v;
            }
        }
    }

    // Defaults
    if( hostname.isEmpty() )
    {
        hostname = QHostInfo::localHostName();
    }

    // Validate required fields
    if( serverUrl.isEmpty() )
    {
        errorString = QStringLiteral("Missing required setting: server_url");
        return false;
    }
    if( adminToken.isEmpty() )
    {
        errorString = QStringLiteral("Missing required setting: admin_token");
        return false;
    }

    return true;
}
