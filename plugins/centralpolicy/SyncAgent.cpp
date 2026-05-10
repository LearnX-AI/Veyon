/*
 * SyncAgent.cpp
 */

#include "SyncAgent.h"
#include "CentralPolicyHooks.h"
#include "CentralPolicyConfiguration.h"
#include "HttpClient.h"

#include "VeyonConfiguration.h"

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QtDebug>


SyncAgent::SyncAgent( QObject* parent ) :
    QObject( parent ),
    m_http( new HttpClient( this ) ),
    m_heartbeatTimer( new QTimer( this ) ),
    m_localBlocklistVersion( 0 ),
    m_localFocusModeActive( false ),
    m_running( false ),
    m_registered( false )
{
    connect( m_heartbeatTimer, &QTimer::timeout,
             this, &SyncAgent::onHeartbeatTick );
}


SyncAgent::~SyncAgent() = default;


void SyncAgent::start()
{
    if( m_running )
    {
        return;
    }

    loadConfiguration();

    m_running = true;
    qInfo() << "[CentralPolicy] SyncAgent starting for host:" << m_hostname;

    registerSelf();

    // Heartbeat begins after the (async) registration completes.
    // We start the timer here too so heartbeats don't stall if registration
    // initially fails (e.g. server not yet up). 30s default.
    CentralPolicyConfiguration cfg( &VeyonCore::config() );
    const int intervalSeconds = qMax( 5, cfg.heartbeatIntervalSeconds() );
    m_heartbeatTimer->start( intervalSeconds * 1000 );
}


void SyncAgent::stop()
{
    m_heartbeatTimer->stop();
    m_running = false;
    qInfo() << "[CentralPolicy] SyncAgent stopped.";
}


void SyncAgent::loadConfiguration()
{
    CentralPolicyConfiguration cfg( &VeyonCore::config() );

    // Configure the HTTP client
    m_http->configure( cfg.serverUrl(), cfg.adminToken() );

    // Hostname: configured value wins, otherwise use OS hostname.
    m_hostname = cfg.machineHostname();
    if( m_hostname.isEmpty() )
    {
        m_hostname = QHostInfo::localHostName();
    }
}


void SyncAgent::registerSelf()
{
    QJsonObject body;
    body[QStringLiteral("hostname")] = m_hostname;

    m_http->post( QStringLiteral("/api/v1/machines/register"), body,
                  [this]( const HttpClient::Response& r ) {
        if( r.ok )
        {
            qInfo() << "[CentralPolicy] Registered with central server.";
            m_registered = true;

            if( r.json.isObject() )
            {
                const auto obj = r.json.toObject();
                m_localBlocklistVersion = obj.value(QStringLiteral("blocklist_version")).toInt();
                m_localFocusModeActive  = obj.value(QStringLiteral("focus_mode_active")).toBool();
            }
        }
        else
        {
            qWarning() << "[CentralPolicy] Registration failed (HTTP" << r.statusCode
                       << "):" << r.errorString;
        }
    });
}


void SyncAgent::onHeartbeatTick()
{
    if( !m_registered )
    {
        // Re-attempt registration; server may have been down on first try.
        registerSelf();
        return;
    }
    sendHeartbeat();
}


void SyncAgent::sendHeartbeat()
{
    QJsonObject body;
    body[QStringLiteral("hostname")] = m_hostname;
    body[QStringLiteral("current_blocklist_version")] = m_localBlocklistVersion;

    m_http->post( QStringLiteral("/api/v1/machines/heartbeat"), body,
                  [this]( const HttpClient::Response& r ) {
        if( !r.ok )
        {
            qWarning() << "[CentralPolicy] Heartbeat failed (HTTP" << r.statusCode
                       << "):" << r.errorString;
            return;
        }

        if( !r.json.isObject() )
        {
            return;
        }

        const auto obj = r.json.toObject();
        const int serverVersion = obj.value(QStringLiteral("blocklist_version")).toInt();
        const bool serverFocusActive = obj.value(QStringLiteral("focus_mode_active")).toBool();

        if( serverVersion != m_localBlocklistVersion )
        {
            qInfo() << "[CentralPolicy] Blocklist version changed:"
                    << m_localBlocklistVersion << "->" << serverVersion;
            // Pull the new list, then emit the rich signal once we have it.
            fetchBlocklist( serverVersion );
        }

        if( serverFocusActive != m_localFocusModeActive )
        {
            qInfo() << "[CentralPolicy] Focus Mode state changed:"
                    << m_localFocusModeActive << "->" << serverFocusActive;
            m_localFocusModeActive = serverFocusActive;
            CentralPolicyHooks::notifyFocusStateChanged( serverFocusActive );
        }
    });
}



void SyncAgent::fetchBlocklist( int newVersion )
{
    m_http->get( QStringLiteral("/api/v1/blocklist"),
                 [this, newVersion]( const HttpClient::Response& r ) {
        if( !r.ok )
        {
            qWarning() << "[CentralPolicy] Failed to fetch blocklist (HTTP"
                       << r.statusCode << "):" << r.errorString;
            return;
        }

        QStringList domains;
        if( r.json.isArray() )
        {
            const auto arr = r.json.toArray();
            for( const QJsonValue& v : arr )
            {
                const auto obj = v.toObject();
                const QString d = obj.value(QStringLiteral("domain")).toString();
                if( !d.isEmpty() )
                {
                    domains.append( d );
                }
            }
        }

        qInfo() << "[CentralPolicy] Pulled" << domains.size()
                << "domains for version" << newVersion;

        m_localBlocklistVersion = newVersion;
        CentralPolicyHooks::notifyBlocklistChanged( domains, newVersion );
    });
}
