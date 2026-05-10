/*
 * SyncAgent.h - background heartbeat + policy sync.
 *
 * Lifecycle:
 *   start()  - registers with server (one-time), then starts heartbeat timer
 *   stop()   - stops the timer, no further requests sent
 *
 * State changes detected during a heartbeat (new blocklist version,
 * focus-mode toggle) are published via CentralPolicyHooks. Plugins
 * (FocusMode etc.) subscribe through that registry, not through this
 * class directly.
 */

#pragma once

#include <QObject>
#include <QStringList>


class HttpClient;
class QTimer;


class SyncAgent : public QObject
{
    Q_OBJECT

public:
    explicit SyncAgent( QObject* parent = nullptr );
    ~SyncAgent() override;

    void start();
    void stop();

    bool isRunning() const            { return m_running; }
    int localBlocklistVersion() const { return m_localBlocklistVersion; }

private Q_SLOTS:
    void onHeartbeatTick();

private:
    void registerSelf();
    void sendHeartbeat();
    void fetchBlocklist( int newVersion );
    void loadConfiguration();

    HttpClient* m_http;
    QTimer* m_heartbeatTimer;

    QString m_hostname;
    int m_localBlocklistVersion;
    bool m_localFocusModeActive;
    bool m_running;
    bool m_registered;
};
