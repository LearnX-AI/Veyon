/*
 * HttpClient.cpp
 */

#include "HttpClient.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>


HttpClient::HttpClient( QObject* parent ) :
    QObject( parent ),
    m_nam( new QNetworkAccessManager( this ) ),
    m_timeoutMs( 10000 )
{
}


HttpClient::~HttpClient() = default;


void HttpClient::configure( const QString& baseUrl, const QString& bearerToken )
{
    // Strip trailing slash so callers don't need to worry about it.
    m_baseUrl = baseUrl.endsWith( QLatin1Char('/') )
                ? baseUrl.left( baseUrl.length() - 1 )
                : baseUrl;
    m_bearerToken = bearerToken;
}


void HttpClient::get( const QString& path, Callback callback )
{
    send( "GET", path, nullptr, std::move( callback ) );
}


void HttpClient::post( const QString& path, const QJsonObject& body, Callback callback )
{
    send( "POST", path, &body, std::move( callback ) );
}


void HttpClient::send( const QByteArray& method,
                       const QString& path,
                       const QJsonObject* body,
                       Callback callback )
{
    if( m_baseUrl.isEmpty() )
    {
        Response r{ false, 0, QStringLiteral("HttpClient not configured"), {} };
        callback( r );
        return;
    }

    QNetworkRequest request{ QUrl( m_baseUrl + path ) };
    request.setHeader( QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json") );
    if( !m_bearerToken.isEmpty() )
    {
        request.setRawHeader( "Authorization",
                              QByteArray("Bearer ") + m_bearerToken.toUtf8() );
    }

    QByteArray payload;
    if( body != nullptr )
    {
        payload = QJsonDocument( *body ).toJson( QJsonDocument::Compact );
    }

    QNetworkReply* reply = m_nam->sendCustomRequest( request, method, payload );

    // Per-request timeout
    QTimer* timeoutTimer = new QTimer( reply );
    timeoutTimer->setSingleShot( true );
    connect( timeoutTimer, &QTimer::timeout, reply, [reply]() {
        if( reply->isRunning() )
        {
            reply->abort();
        }
    });
    timeoutTimer->start( m_timeoutMs );

    connect( reply, &QNetworkReply::finished, this, [reply, callback]() {
        Response r;
        r.statusCode = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
        r.ok = ( r.statusCode >= 200 && r.statusCode < 300 );

        const QByteArray bodyBytes = reply->readAll();
        if( !bodyBytes.isEmpty() )
        {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson( bodyBytes, &parseError );
            if( parseError.error == QJsonParseError::NoError )
            {
                r.json = doc.isObject() ? QJsonValue( doc.object() ) : QJsonValue( doc.array() );
            }
        }

        if( !r.ok )
        {
            r.errorString = reply->errorString();
            if( r.statusCode == 0 )
            {
                r.errorString = QStringLiteral("Network error: ") + r.errorString;
            }
        }

        callback( r );
        reply->deleteLater();
    });
}
