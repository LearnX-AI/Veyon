/*
 * SyncAgent.h - background heartbeat + policy sync.
 *
 * Lifecycle:
 *   start()  - registers with server (one-time), then starts heartbeat timer
 *   stop()   - stops the timer, no further requests sent
 *
 * State:
 *   m_localBlocklistVersion - what we last pulled from the server.
 *                              Bumped to match server version after a
 *                              successful sync.
 */

#pragma once

#include <QObject>


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

    bool isRunning() const { return m_running; }
    int localBlocklistVersion() const { return m_localBlocklistVersion; }

Q_SIGNALS:
    /// Emitted when the server reports a different blocklist version.
    /// Subscribers (e.g. FocusMode) should re-pull the blocklist.
    void blocklistChanged( int newVersion );

    /// Emitted when the server reports a Focus Mode state change.
    void focusModeStateChanged( bool enabled );

private Q_SLOTS:
    void onHeartbeatTick();

private:
    void registerSelf();
    void sendHeartbeat();
    void loadConfiguration();

    HttpClient* m_http;
    QTimer* m_heartbeatTimer;

    QString m_hostname;
    int m_localBlocklistVersion;
    bool m_localFocusModeActive;
    bool m_running;
    bool m_registered;
};
