/*
 * BlocklistLoader.cpp - reads the Focus Mode blocklist from disk
 */

#include <QFile>
#include <QSet>
#include <QTextStream>

#include "BlocklistLoader.h"


QStringList BlocklistLoader::load( const QString& path )
{
    QFile file( path );
    if( file.open( QIODevice::ReadOnly | QIODevice::Text ) == false )
    {
        return {};
    }

    QSet<QString> seen;
    QStringList result;
    QTextStream in( &file );

    while( in.atEnd() == false )
    {
        QString line = in.readLine().trimmed();

        // Skip empty lines and comments
        if( line.isEmpty() || line.startsWith( QLatin1Char( '#' ) ) )
        {
            continue;
        }

        // Reject anything with inline comment markers, whitespace, or schemes
        const int hashPos = line.indexOf( QLatin1Char( '#' ) );
        if( hashPos >= 0 )
        {
            line = line.left( hashPos ).trimmed();
            if( line.isEmpty() )
            {
                continue;
            }
        }

        if( isValidDomain( line ) == false )
        {
            continue;
        }

        // Normalize to lowercase
        line = line.toLower();

        if( seen.contains( line ) == false )
        {
            seen.insert( line );
            result.append( line );
        }
    }

    return result;
}


bool BlocklistLoader::isValidDomain( const QString& domain )
{
    if( domain.isEmpty() )                          return false;
    if( domain.contains( QLatin1Char( ' ' ) ) )     return false;
    if( domain.contains( QLatin1Char( '\t' ) ) )    return false;
    if( domain.contains( QStringLiteral("://") ) )  return false;
    if( domain.contains( QLatin1Char( '/' ) ) )     return false;
    if( domain.length() > 253 )                     return false;   // RFC 1035 max

    // Must contain at least one dot (e.g., reject "localhost" or "test")
    if( domain.contains( QLatin1Char( '.' ) ) == false ) return false;

    return true;
}
