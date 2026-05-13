/*
 * SubmissionUploader.cpp
 */

#include "SubmissionUploader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QMimeDatabase>
#include <QMimeType>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QtDebug>


SubmissionUploader::SubmissionUploader(
    QNetworkAccessManager* nam,
    const QString& serverUrl,
    const QString& token,
    const QString& hostname,
    QObject* parent ) :
    QObject( parent ),
    m_nam( nam ),
    m_serverUrl( serverUrl ),
    m_token( token ),
    m_hostname( hostname )
{
}


SubmissionUploader::~SubmissionUploader()
{
    // QHttpMultiPart takes ownership of the parts and the file, so deleting
    // the multipart in the right order matters. Both are reparented to
    // m_reply once it starts; if we never got that far, clean up here.
    if( m_multipart != nullptr && (m_reply == nullptr || !m_finished) )
    {
        // If reply ran, it owns the multipart; otherwise we own it.
        if( m_reply == nullptr )
        {
            delete m_multipart;
        }
    }
}


void SubmissionUploader::start( const PendingSubmission& info )
{
    m_info = info;

    // Open the file
    m_file = new QFile( m_info.filePath, this );
    if( !m_file->open( QIODevice::ReadOnly ) )
    {
        fail( QStringLiteral("Cannot open file: ") + m_file->errorString() );
        return;
    }

    // Build the multipart payload
    m_multipart = new QHttpMultiPart( QHttpMultiPart::FormDataType );

    QHttpPart filePart;
    const QFileInfo info_fi( m_info.filePath );
    const QString mime =
        QMimeDatabase().mimeTypeForFile( info_fi ).name();

    filePart.setHeader( QNetworkRequest::ContentTypeHeader, mime );
    filePart.setHeader(
        QNetworkRequest::ContentDispositionHeader,
        QStringLiteral("form-data; name=\"file\"; filename=\"%1\"")
            .arg( info_fi.fileName() )
    );
    // The QFile is reparented to the multipart, which is reparented to the reply.
    filePart.setBodyDevice( m_file );
    m_file->setParent( m_multipart );

    m_multipart->append( filePart );

    // Build the URL and request
    const QUrl url( m_serverUrl
        + QStringLiteral("/api/v1/folders/")
        + QString::number( m_info.folderId )
        + QStringLiteral("/submit") );

    QNetworkRequest req{ url };
    req.setRawHeader( "Authorization",
                      QByteArray("Bearer ") + m_token.toUtf8() );
    req.setRawHeader( "X-Veyon-Hostname", m_hostname.toUtf8() );

    m_reply = m_nam->post( req, m_multipart );
    m_multipart->setParent( m_reply );      // multipart owned by the reply now

    connect( m_reply, &QNetworkReply::finished,
             this, &SubmissionUploader::onFinished );

    qInfo().noquote() << "[submit] uploading"
                      << info_fi.fileName()
                      << "->" << m_info.folderName;
}


void SubmissionUploader::onFinished()
{
    if( m_finished )
    {
        return;
    }

    const int httpCode =
        m_reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
    const auto netError = m_reply->error();
    const QByteArray body = m_reply->readAll();

    m_reply->deleteLater();
    m_reply = nullptr;

    if( netError != QNetworkReply::NoError || httpCode < 200 || httpCode >= 300 )
    {
        // Try to extract a useful detail from the JSON response
        QString detail = QString::fromUtf8( body ).left( 300 );
        fail( QStringLiteral("HTTP %1: %2").arg( httpCode ).arg( detail ) );
        return;
    }

    succeed();
}


void SubmissionUploader::succeed()
{
    // Rename file -> file.submitted so we don't re-upload it.
    const QString newPath = m_info.filePath + QStringLiteral(".submitted");

    // If a .submitted file with that exact name already exists (rare:
    // student dropped two identically-named files), make the name unique.
    QString finalPath = newPath;
    int suffix = 1;
    while( QFile::exists( finalPath ) )
    {
        finalPath = newPath + QStringLiteral(".%1").arg( suffix );
        suffix++;
    }

    if( !QFile::rename( m_info.filePath, finalPath ) )
    {
        // Upload succeeded but rename failed - bad state. We'd re-upload
        // on the next tick. Log loudly so the operator can intervene.
        qWarning().noquote()
            << "[submit] Uploaded but could not rename"
            << m_info.filePath
            << "to" << finalPath
            << "- will retry uploading on next tick.";

        // We treat this as a failure so the caller doesn't think we're
        // safe. The retry will be idempotent on the server side because
        // we deduplicate by SHA there if we wanted to - currently we don't,
        // so this could create a duplicate submission. Worth noting.
        Q_EMIT done( false, m_info, QStringLiteral("rename failed") );
        m_finished = true;
        return;
    }

    qInfo().noquote() << "[submit] done:"
                      << QFileInfo( m_info.filePath ).fileName()
                      << "renamed to" << QFileInfo( finalPath ).fileName();

    m_finished = true;
    Q_EMIT done( true, m_info, QString() );
}


void SubmissionUploader::fail( const QString& message )
{
    qWarning().noquote() << "[submit] FAILED"
                         << QFileInfo( m_info.filePath ).fileName()
                         << ":" << message;
    m_finished = true;
    Q_EMIT done( false, m_info, message );
}
