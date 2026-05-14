/*
 * SessionMonitor.h - Tracks the active time-limited session for this PC.
 *
 * On each tick (every session_check_interval_seconds):
 *   1. GET /api/v1/sessions/agent-active with hostname header
 *   2. If null: no active session, drop any state and exit
 *   3. If a session is returned:
 *      - Compute time remaining (ends_at - now)
 *      - If a warning_minute threshold has just been crossed: notify the
 *        user (notify-send via the logged-in user's session) and POST a
 *        WARNED event
 *      - If time has expired: execute the configured timeout action
 *        (lock_screen / logout / shutdown), POST an ACTION_FIRED event,
 *        and remember session_id so we don't fire twice
 *
 * The agent runs as root, so it can:
 *   - call loginctl/systemctl for actions
 *   - shell out to notify-send via sudo -u <user> for warnings
 */

#pragma once

#include <QObject>
#include <QSet>
#include <QString>


class QNetworkAccessManager;
class QNetworkReply;
class QDateTime;


class SessionMonitor : public QObject
{
    Q_OBJECT

public:
    SessionMonitor(
        QNetworkAccessManager* nam,
        const QString& serverUrl,
        const QString& token,
        const QString& hostname,
        QObject* parent = nullptr );

    /// Called by PolicyAgent on every tick.
    void runOneTick();

private Q_SLOTS:
    void onActiveSessionResponse();

private:
    void handleSession(
        int sessionId,
        const QString& sessionName,
        const QDateTime& endsAt,
        const QString& timeoutAction,
        const QList<int>& warningMinutes );

    void clearSessionState();

    /// Fire a desktop notification to the active user (if any).
    /// Best-effort: returns true if we believe it got delivered.
    bool sendNotification( const QString& summary, const QString& body );

    /// Run the timeout action. Returns a short string describing result.
    QString executeTimeoutAction( const QString& action );

    /// Report an event back to the server. Fire-and-forget.
    void reportEvent( int sessionId, const QString& eventType, const QString& details );

    /// Determine the currently active graphical user (first logged-in one).
    /// Returns empty string if none found.
    QString activeUser() const;

    QNetworkAccessManager* m_nam;
    QString m_serverUrl;
    QString m_token;
    QString m_hostname;

    bool m_tickInFlight = false;

    // Persistent state across ticks
    int m_activeSessionId = -1;        // session we're currently tracking, -1 = none
    QSet<int> m_firedWarnings;         // warning-minute thresholds we've already warned for
    bool m_actionFired = false;        // did we already run the timeout action?
};
