/*
 * FileDownloader.cpp
 */

#include "FileDownloader.h"

#include "../plugins/centralpolicy/HttpClient.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QtDebug>


FileDownloader::FileDownloader(
    HttpClient* http,
    QNetworkAccessManager* nam,
    const QString& serverUrl,
    const QString& token,
    const QString& hostname,
    const QString& destDir,
    QObject* parent ) :
    QObject( parent ),
    m_http( http ),
    m_nam( nam ),
    m_serverUrl( serverUrl ),
    m_token( token ),
    m_hostname( hostname ),
    m_destDir( destDir )
{
}


FileDownloader::~FileDownloader()
{
    if( m_hash != nullptr )
    {
        delete m_hash;
    }
    if( m_tempFile != nullptr )
    {
        if( m_tempFile->isOpen() )
        {
            m_tempFile->close();
        }
        // Best-effort cleanup of any leftover temp file
        if( !m_tempPath.isEmpty() && QFile::exists( m_tempPath ) && !m_finished )
        {
            QFile::remove( m_tempPath );
        }
        delete m_tempFile;
    }
}


void FileDownloader::start( const PendingFile& info )
{
    m_info = info;

    // Ensure destination directory exists.
    QDir dir;
    if( !dir.mkpath( m_destDir ) )
    {
        fail( QStringLiteral("Cannot create destination dir: ") + m_destDir );
        return;
    }

    // Build paths. Final file gets the original filename;
    // temp file uses a .partial suffix in the same dir for atomic rename.
    m_finalPath = m_destDir + QLatin1Char('/') + m_info.filename;
    m_tempPath  = m_finalPath + QStringLiteral(".partial");

    m_tempFile = new QFile( m_tempPath );
    if( !m_tempFile->open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
        fail( QStringLiteral("Cannot open temp file: ") + m_tempPath
              + QStringLiteral(" (") + m_tempFile->errorString() + QStringLiteral(")") );
        return;
    }

    m_hash = new QCryptographicHash( QCryptographicHash::Sha256 );

    const QUrl url( m_serverUrl + QStringLiteral("/api/v1/files/")
                    + QString::number( m_info.distributionId )
                    + QStringLiteral("/download") );

    QNetworkRequest request{ url };
    request.setRawHeader( "Authorization",
                          QByteArray("Bearer ") + m_token.toUtf8() );
    request.setRawHeader( "X-Veyon-Hostname", m_hostname.toUtf8() );
    request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );

    m_reply = m_nam->get( request );

    connect( m_reply, &QNetworkReply::readyRead,
             this, &FileDownloader::onReadyRead );
    connect( m_reply, &QNetworkReply::finished,
             this, &FileDownloader::onDownloadFinished );

    qInfo().noquote() << "[file]" << "Downloading"
                      << m_info.filename << "(" << m_info.sizeBytes << "bytes)";
}


void FileDownloader::onReadyRead()
{
    // Stream bytes from the network straight to disk + hash. We never
    // buffer the whole file in memory.
    while( m_reply != nullptr && m_reply->bytesAvailable() > 0 )
    {
        const QByteArray chunk = m_reply->read( 64 * 1024 );
        if( chunk.isEmpty() )
        {
            break;
        }
        m_hash->addData( chunk );
        const qint64 written = m_tempFile->write( chunk );
        if( written != chunk.size() )
        {
            // Disk full or other write failure. Abort.
            if( m_reply != nullptr )
            {
                m_reply->abort();
            }
            fail( QStringLiteral("Disk write failed: ") + m_tempFile->errorString() );
            return;
        }
        m_bytesWritten += written;
    }
}


void FileDownloader::onDownloadFinished()
{
    if( m_finished )
    {
        return;
    }

    // Flush any final bytes
    onReadyRead();

    const int httpCode =
        m_reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
    const auto netError = m_reply->error();

    m_tempFile->close();

    if( netError != QNetworkReply::NoError || httpCode < 200 || httpCode >= 300 )
    {
        fail( QStringLiteral("Download failed (HTTP %1): %2")
              .arg( httpCode ).arg( m_reply->errorString() ) );
        m_reply->deleteLater();
        m_reply = nullptr;
        return;
    }

    // Verify size
    if( m_info.sizeBytes > 0 && m_bytesWritten != m_info.sizeBytes )
    {
        fail( QStringLiteral("Size mismatch: expected %1, got %2")
              .arg( m_info.sizeBytes ).arg( m_bytesWritten ) );
        m_reply->deleteLater();
        m_reply = nullptr;
        return;
    }

    // Verify SHA-256
    const QString computed = QString::fromLatin1( m_hash->result().toHex() );
    if( computed != m_info.sha256.toLower() )
    {
        fail( QStringLiteral("Checksum mismatch (got %1, expected %2)")
              .arg( computed ).arg( m_info.sha256 ) );
        m_reply->deleteLater();
        m_reply = nullptr;
        return;
    }

    // Atomic move: temp -> final. If final exists, replace it.
    if( QFile::exists( m_finalPath ) )
    {
        QFile::remove( m_finalPath );
    }
    if( !QFile::rename( m_tempPath, m_finalPath ) )
    {
        fail( QStringLiteral("Rename failed: ") + m_tempPath
              + QStringLiteral(" -> ") + m_finalPath );
        m_reply->deleteLater();
        m_reply = nullptr;
        return;
    }

    m_reply->deleteLater();
    m_reply = nullptr;

    succeed();
}


void FileDownloader::succeed()
{
    qInfo().noquote() << "[file]" << "Delivered" << m_info.filename
                      << "->" << m_finalPath;
    postAck( true, m_bytesWritten, QString() );
    m_finished = true;
}


void FileDownloader::fail( const QString& message )
{
    qWarning().noquote() << "[file]" << m_info.filename
                         << "FAILED:" << message;
    // Clean up partial file
    if( m_tempFile && m_tempFile->isOpen() )
    {
        m_tempFile->close();
    }
    if( !m_tempPath.isEmpty() && QFile::exists( m_tempPath ) )
    {
        QFile::remove( m_tempPath );
    }
    postAck( false, m_bytesWritten, message );
    m_finished = true;
}


void FileDownloader::postAck( bool success, qint64 bytes, const QString& message )
{
    QJsonObject body;
    body[QStringLiteral("success")]        = success;
    body[QStringLiteral("bytes_received")] = bytes;
    if( !message.isEmpty() )
    {
        body[QStringLiteral("error_message")] = message;
    }

    // Path: /api/v1/files/{id}/ack
    const QString path = QStringLiteral("/api/v1/files/")
                       + QString::number( m_info.distributionId )
                       + QStringLiteral("/ack");

    // HttpClient::post doesn't accept extra headers, so we add the hostname
    // via an inline raw header through QNetworkAccessManager.
    // Simplest approach: use the http client and rely on the agent already
    // having set base url + token; for the hostname header we use a
    // direct raw POST.
    const QUrl url( m_serverUrl + path );
    QNetworkRequest req{ url };
    req.setHeader( QNetworkRequest::ContentTypeHeader,
                   QStringLiteral("application/json") );
    req.setRawHeader( "Authorization",
                      QByteArray("Bearer ") + m_token.toUtf8() );
    req.setRawHeader( "X-Veyon-Hostname", m_hostname.toUtf8() );

    const QByteArray payload =
        QJsonDocument( body ).toJson( QJsonDocument::Compact );

    QNetworkReply* ackReply = m_nam->post( req, payload );

    connect( ackReply, &QNetworkReply::finished, this, [this, ackReply, success, message]() {
        const int hc = ackReply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute ).toInt();
        if( ackReply->error() != QNetworkReply::NoError || hc < 200 || hc >= 300 )
        {
            qWarning().noquote() << "[file] ACK failed (HTTP" << hc
                                 << "):" << ackReply->errorString();
        }
        ackReply->deleteLater();

        Q_EMIT done( success, m_info, message );
    });
}
