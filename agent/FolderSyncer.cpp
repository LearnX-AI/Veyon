/*
 * FolderSyncer.cpp
 */

#include "FolderSyncer.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>
#include <QtDebug>


FolderSyncer::FolderSyncer(
    QNetworkAccessManager* nam,
    const QString& serverUrl,
    const QString& token,
    const QString& hostname,
    const QString& submissionsRootDir,
    QObject* parent ) :
    QObject( parent ),
    m_nam( nam ),
    m_serverUrl( serverUrl ),
    m_token( token ),
    m_hostname( hostname ),
    m_submissionsRootDir( submissionsRootDir )
{
}


// static
QString FolderSyncer::safeFolderName( const QString& raw )
{
    // Allow: letters, digits, dot, dash, underscore. Everything else -> _
    QString out = raw.trimmed();
    static const QRegularExpression bad( QStringLiteral("[^A-Za-z0-9._\\-]+") );
    out.replace( bad, QStringLiteral("_") );
    // Trim leading/trailing underscores and collapse repeats
    while( out.startsWith( QLatin1Char('_') ) )
    {
        out.remove( 0, 1 );
    }
    while( out.endsWith( QLatin1Char('_') ) )
    {
        out.chop( 1 );
    }
    if( out.isEmpty() )
    {
        out = QStringLiteral("untitled");
    }
    return out.left( 100 );      // bound length
}


void FolderSyncer::runOneTick()
{
    if( m_tickInFlight )
    {
        // A previous tick is still finishing - skip.
        return;
    }
    if( m_uploadInFlight )
    {
        // An upload is in progress. Don't start a new scan; we'll catch up
        // on the next tick after the upload completes.
        return;
    }

    m_tickInFlight = true;

    const QUrl url( m_serverUrl + QStringLiteral("/api/v1/folders/agent-sync") );
    QNetworkRequest req{ url };
    req.setRawHeader( "Authorization",
                      QByteArray("Bearer ") + m_token.toUtf8() );
    req.setRawHeader( "X-Veyon-Hostname", m_hostname.toUtf8() );

    QNetworkReply* reply = m_nam->get( req );
    connect( reply, &QNetworkReply::finished,
             this, &FolderSyncer::onSyncResponseFinished );
}


void FolderSyncer::onSyncResponseFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>( sender() );
    if( reply == nullptr )
    {
        m_tickInFlight = false;
        return;
    }

    const int httpCode =
        reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
    const QByteArray body = reply->readAll();
    const auto netError = reply->error();
    reply->deleteLater();

    if( netError != QNetworkReply::NoError || httpCode < 200 || httpCode >= 300 )
    {
        qWarning().noquote() << "[folder-sync] HTTP" << httpCode
                             << "error:" << reply->errorString();
        m_tickInFlight = false;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson( body );
    if( !doc.isArray() )
    {
        m_tickInFlight = false;
        return;
    }

    // Iterate through each folder, ensure its directory exists, scan for
    // pending submissions. We accumulate everything into m_queue, then
    // fire off one upload at a time via processNextPending().
    const QJsonArray arr = doc.array();
    for( const QJsonValue& v : arr )
    {
        const QJsonObject obj = v.toObject();
        const int     folderId = obj.value( QStringLiteral("folder_id") ).toInt();
        const QString name     = obj.value( QStringLiteral("name") ).toString();
        const QString status   = obj.value( QStringLiteral("status") ).toString();

        const QString safeName = safeFolderName( name );
        const QString dirPath = m_submissionsRootDir
                              + QLatin1Char('/') + safeName;

        // Always ensure the dir exists - even if closed - so students can see it.
        QDir().mkpath( dirPath );

        // No new submissions accepted into closed folders.
        if( status != QStringLiteral("open") )
        {
            continue;
        }

        // Scan for files awaiting submission.
        QDir dir( dirPath );
        const QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot,
            QDir::Name
        );

        for( const QFileInfo& fi : entries )
        {
            const QString filename = fi.fileName();
            // Skip dotfiles, .submitted, .partial, and any submitted variants
            if( filename.startsWith( QLatin1Char('.') ) )
            {
                continue;
            }
            if( filename.endsWith( QStringLiteral(".submitted") ) )
            {
                continue;
            }
            if( filename.contains( QStringLiteral(".submitted.") ) )
            {
                continue;
            }
            if( filename.endsWith( QStringLiteral(".partial") ) )
            {
                continue;
            }

            SubmissionUploader::PendingSubmission s;
            s.folderId   = folderId;
            s.folderName = name;
            s.filePath   = fi.absoluteFilePath();
            m_queue.enqueue( s );
        }
    }

    m_tickInFlight = false;
    processNextPending();
}


void FolderSyncer::processNextPending()
{
    if( m_uploadInFlight || m_queue.isEmpty() )
    {
        if( !m_uploadInFlight )
        {
            Q_EMIT tickDone();
        }
        return;
    }

    const SubmissionUploader::PendingSubmission info = m_queue.dequeue();

    auto* up = new SubmissionUploader(
        m_nam, m_serverUrl, m_token, m_hostname, this );
    connect( up, &SubmissionUploader::done,
             this, &FolderSyncer::onUploadDone );

    m_uploadInFlight = true;
    up->start( info );
}


void FolderSyncer::onUploadDone(
    bool success,
    SubmissionUploader::PendingSubmission info,
    QString message )
{
    Q_UNUSED( success )
    Q_UNUSED( info )
    Q_UNUSED( message )

    sender()->deleteLater();
    m_uploadInFlight = false;

    // Continue draining the queue.
    processNextPending();
}
