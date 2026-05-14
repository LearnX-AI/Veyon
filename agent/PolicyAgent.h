/*
 * PolicyAgent.h - The main agent logic.
 *
 * Lifecycle:
 *   start()  - reads config, registers with server, starts heartbeat timer
 *   stop()   - stops the timer
 *
 * Two independent timers run in parallel:
 *   - Heartbeat (default 30s): blocklist + focus mode sync
 *   - File check (default 10s): pulls and applies any pending file deliveries
 */

#pragma once

#include <QObject>

#include "ConfigLoader.h"
#include "FileDownloader.h"
#include "FolderSyncer.h"
#include "HostsWriter.h"
#include "SessionMonitor.h"


class HttpClient;
class QNetworkAccessManager;
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
    void onFileCheckTick();
    void onFileDownloadDone( bool success, FileDownloader::PendingFile info, QString message );
    void onFolderSyncTick();
    void onSessionCheckTick();

private:
    void registerWithServer();
    void sendHeartbeat();
    void fetchBlocklist( int newVersion );
    void checkPendingFiles();

    ConfigLoader m_config;
    HttpClient* m_http;
    QNetworkAccessManager* m_nam;
    HostsWriter m_hosts;
    QTimer* m_heartbeatTimer;
    QTimer* m_fileCheckTimer;
    QTimer* m_folderSyncTimer;
    QTimer* m_sessionCheckTimer;
    FolderSyncer* m_folderSyncer;
    SessionMonitor* m_sessionMonitor;
    bool m_fileCheckInFlight;

    int m_localVersion;
    bool m_localFocusActive;
    bool m_registered;
};
