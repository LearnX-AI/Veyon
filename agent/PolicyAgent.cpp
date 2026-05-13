/*
 * PolicyAgent.cpp
 */

#include "PolicyAgent.h"

#include "FolderSyncer.h"

#include "../plugins/centralpolicy/HttpClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QUrl>
#include <QtDebug>


PolicyAgent::PolicyAgent( QObject* parent ) :
    QObject( parent ),
    m_http( new HttpClient( this ) ),
    m_nam( new QNetworkAccessManager( this ) ),
    m_hosts( QStringLiteral("/etc/hosts") ),
    m_heartbeatTimer( new QTimer( this ) ),
    m_fileCheckTimer( new QTimer( this ) ),
    m_folderSyncTimer( new QTimer( this ) ),
    m_folderSyncer( nullptr ),
    m_fileCheckInFlight( false ),
    m_localVersion( 0 ),
    m_localFocusActive( false ),
    m_registered( false )
{
    connect( m_heartbeatTimer, &QTimer::timeout,
             this, &PolicyAgent::onHeartbeatTick );
    connect( m_fileCheckTimer, &QTimer::timeout,
             this, &PolicyAgent::onFileCheckTick );
    connect( m_folderSyncTimer, &QTimer::timeout,
             this, &PolicyAgent::onFolderSyncTick );
}


PolicyAgent::~PolicyAgent() = default;


bool PolicyAgent::start()
{
    if( !m_config.load() )
    {
        qCritical().noquote() << "Config error:" << m_config.errorString;
        return false;
    }

    qInfo().noquote() << "Loaded config:";
    qInfo().noquote() << "  server_url        =" << m_config.serverUrl;
    qInfo().noquote() << "  hostname          =" << m_config.hostname;
    qInfo().noquote() << "  heartbeat_seconds =" << m_config.heartbeatIntervalSeconds;
    qInfo().noquote() << "  hosts_file        =" << m_config.hostsFile;

    // Reconfigure HostsWriter to honor non-default hosts_file from config
    m_hosts = HostsWriter( m_config.hostsFile );

    m_http->configure( m_config.serverUrl, m_config.adminToken );

    registerWithServer();

    m_heartbeatTimer->start( m_config.heartbeatIntervalSeconds * 1000 );
    qInfo().noquote() << "Heartbeat timer started (" << m_config.heartbeatIntervalSeconds << "s).";

    m_fileCheckTimer->start( m_config.fileCheckIntervalSeconds * 1000 );
    qInfo().noquote() << "File check timer started (" << m_config.fileCheckIntervalSeconds << "s).";

    m_folderSyncer = new FolderSyncer(
        m_nam, m_config.serverUrl, m_config.adminToken,
        m_config.hostname, m_config.submissionsRootDir, this );

    m_folderSyncTimer->start( m_config.folderSyncIntervalSeconds * 1000 );
    qInfo().noquote() << "Folder sync timer started (" << m_config.folderSyncIntervalSeconds << "s).";
    return true;
}


void PolicyAgent::stop()
{
    m_heartbeatTimer->stop();
    m_fileCheckTimer->stop();
    m_folderSyncTimer->stop();
    qInfo().noquote() << "Agent stopped.";
}


void PolicyAgent::registerWithServer()
{
    QJsonObject body;
    body[QStringLiteral("hostname")] = m_config.hostname;

    m_http->post( QStringLiteral("/api/v1/machines/register"), body,
                  [this]( const HttpClient::Response& r ) {
        if( !r.ok )
        {
            qWarning().noquote() << "Registration failed (HTTP" << r.statusCode
                                 << "):" << r.errorString;
            return;
        }

        m_registered = true;
        if( r.json.isObject() )
        {
            const auto obj = r.json.toObject();
            m_localVersion     = obj.value(QStringLiteral("blocklist_version")).toInt();
            m_localFocusActive = obj.value(QStringLiteral("focus_mode_active")).toBool();
        }
        qInfo().noquote() << "Registered with server. Initial version =" << m_localVersion;

        // Pull initial blocklist immediately so the machine is in sync.
        fetchBlocklist( m_localVersion );
    });
}


void PolicyAgent::onHeartbeatTick()
{
    if( !m_registered )
    {
        registerWithServer();      // re-attempt registration if it failed before
        return;
    }
    sendHeartbeat();
}


void PolicyAgent::sendHeartbeat()
{
    QJsonObject body;
    body[QStringLiteral("hostname")] = m_config.hostname;
    body[QStringLiteral("current_blocklist_version")] = m_localVersion;

    m_http->post( QStringLiteral("/api/v1/machines/heartbeat"), body,
                  [this]( const HttpClient::Response& r ) {
        if( !r.ok )
        {
            qWarning().noquote() << "Heartbeat failed (HTTP" << r.statusCode
                                 << "):" << r.errorString;
            return;
        }

        if( !r.json.isObject() )
        {
            return;
        }

        const auto obj = r.json.toObject();
        const int  serverVersion     = obj.value(QStringLiteral("blocklist_version")).toInt();
        const bool serverFocusActive = obj.value(QStringLiteral("focus_mode_active")).toBool();

        if( serverVersion != m_localVersion )
        {
            qInfo().noquote() << "Blocklist changed:"
                              << m_localVersion << "->" << serverVersion;
            fetchBlocklist( serverVersion );
        }

        if( serverFocusActive != m_localFocusActive )
        {
            qInfo().noquote() << "Focus mode toggled:"
                              << m_localFocusActive << "->" << serverFocusActive;
            m_localFocusActive = serverFocusActive;
            // For now we always apply blocklist regardless of focus state -
            // a future revision could clear /etc/hosts when focus is disabled.
        }
    });
}


void PolicyAgent::fetchBlocklist( int newVersion )
{
    m_http->get( QStringLiteral("/api/v1/blocklist"),
                 [this, newVersion]( const HttpClient::Response& r ) {
        if( !r.ok )
        {
            qWarning().noquote() << "Blocklist fetch failed (HTTP" << r.statusCode
                                 << "):" << r.errorString;
            return;
        }

        QStringList domains;
        if( r.json.isArray() )
        {
            for( const QJsonValue& v : r.json.toArray() )
            {
                const auto obj = v.toObject();
                const QString d = obj.value(QStringLiteral("domain")).toString();
                if( !d.isEmpty() )
                {
                    domains.append( d );
                }
            }
        }

        if( !m_hosts.apply( domains ) )
        {
            qCritical().noquote() << "Failed to update /etc/hosts:"
                                  << m_hosts.errorString();
            return;
        }

        m_localVersion = newVersion;
        qInfo().noquote() << "Applied" << domains.size()
                          << "blocked domain(s) to" << m_config.hostsFile
                          << "(version" << newVersion << ")";
    });
}



// =============================================================
// File distribution
// =============================================================

void PolicyAgent::onFileCheckTick()
{
    if( !m_registered )
    {
        return;        // wait until heartbeat loop has us registered
    }
    if( m_fileCheckInFlight )
    {
        return;        // don't pile up requests while a download is in progress
    }
    checkPendingFiles();
}


void PolicyAgent::checkPendingFiles()
{
    m_fileCheckInFlight = true;

    // /api/v1/files/pending - GET with X-Veyon-Hostname header.
    // HttpClient doesn't expose per-request raw headers, so we do this directly.
    const QUrl url( m_config.serverUrl + QStringLiteral("/api/v1/files/pending") );
    QNetworkRequest req{ url };
    req.setRawHeader( "Authorization",
                      QByteArray("Bearer ") + m_config.adminToken.toUtf8() );
    req.setRawHeader( "X-Veyon-Hostname", m_config.hostname.toUtf8() );

    QNetworkReply* reply = m_nam->get( req );
    connect( reply, &QNetworkReply::finished, this, [this, reply]() {
        const int hc = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute ).toInt();

        if( reply->error() != QNetworkReply::NoError || hc < 200 || hc >= 300 )
        {
            qWarning().noquote() << "[file] /pending failed (HTTP" << hc
                                 << "):" << reply->errorString();
            reply->deleteLater();
            m_fileCheckInFlight = false;
            return;
        }

        const QByteArray body = reply->readAll();
        reply->deleteLater();

        const QJsonDocument doc = QJsonDocument::fromJson( body );
        if( !doc.isArray() )
        {
            m_fileCheckInFlight = false;
            return;
        }

        const QJsonArray arr = doc.array();
        if( arr.isEmpty() )
        {
            m_fileCheckInFlight = false;
            return;       // nothing to do
        }

        // Take the FIRST pending file only. After it finishes, the next
        // tick will pick up the next one. This avoids running parallel
        // downloads which would compete for disk and bandwidth.
        const QJsonObject obj = arr.first().toObject();
        FileDownloader::PendingFile info;
        info.distributionId = obj.value(QStringLiteral("distribution_id")).toInt();
        info.fileId         = obj.value(QStringLiteral("file_id")).toInt();
        info.storageId      = obj.value(QStringLiteral("storage_id")).toString();
        info.filename       = obj.value(QStringLiteral("filename")).toString();
        info.sha256         = obj.value(QStringLiteral("sha256")).toString();
        info.sizeBytes      = static_cast<qint64>(
            obj.value(QStringLiteral("size_bytes")).toDouble() );

        auto* dl = new FileDownloader(
            m_http, m_nam,
            m_config.serverUrl, m_config.adminToken, m_config.hostname,
            m_config.fileDestinationDir,
            this );
        connect( dl, &FileDownloader::done,
                 this, &PolicyAgent::onFileDownloadDone );
        dl->start( info );
    });
}


void PolicyAgent::onFileDownloadDone(
    bool success,
    FileDownloader::PendingFile info,
    QString message )
{
    Q_UNUSED( info )
    Q_UNUSED( message )
    Q_UNUSED( success )

    // Reset the flag and let the FileDownloader self-destruct.
    sender()->deleteLater();
    m_fileCheckInFlight = false;
}



void PolicyAgent::onFolderSyncTick()
{
    if( !m_registered || m_folderSyncer == nullptr )
    {
        return;
    }
    m_folderSyncer->runOneTick();
}
