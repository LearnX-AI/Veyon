/*
 * PolicyAgent.cpp
 */

#include "PolicyAgent.h"

#include "../plugins/centralpolicy/HttpClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QTimer>
#include <QtDebug>


PolicyAgent::PolicyAgent( QObject* parent ) :
    QObject( parent ),
    m_http( new HttpClient( this ) ),
    m_hosts( QStringLiteral("/etc/hosts") ),
    m_heartbeatTimer( new QTimer( this ) ),
    m_localVersion( 0 ),
    m_localFocusActive( false ),
    m_registered( false )
{
    connect( m_heartbeatTimer, &QTimer::timeout,
             this, &PolicyAgent::onHeartbeatTick );
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
    qInfo().noquote() << "Heartbeat timer started.";
    return true;
}


void PolicyAgent::stop()
{
    m_heartbeatTimer->stop();
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
