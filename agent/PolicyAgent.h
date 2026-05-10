/*
 * PolicyAgent.h - The main agent logic.
 *
 * Lifecycle:
 *   start()  - reads config, registers with server, starts heartbeat timer
 *   stop()   - stops the timer
 *
 * The heartbeat tick:
 *   1. POST /api/v1/machines/heartbeat with current_blocklist_version
 *   2. If server's blocklist_version != ours -> GET /api/v1/blocklist
 *      and apply via HostsWriter
 *   3. Update last_focus_active for completeness
 */

#pragma once

#include <QObject>

#include "ConfigLoader.h"
#include "HostsWriter.h"


class HttpClient;
class QTimer;


class PolicyAgent : public QObject
{
    Q_OBJECT

public:
    explicit PolicyAgent( QObject* parent = nullptr );
    ~PolicyAgent() override;

    /// Returns true if config loaded successfully and registration started.
    bool start();
    void stop();

private Q_SLOTS:
    void onHeartbeatTick();

private:
    void registerWithServer();
    void sendHeartbeat();
    void fetchBlocklist( int newVersion );

    ConfigLoader m_config;
    HttpClient* m_http;
    HostsWriter m_hosts;
    QTimer* m_heartbeatTimer;

    int m_localVersion;
    bool m_localFocusActive;
    bool m_registered;
};
