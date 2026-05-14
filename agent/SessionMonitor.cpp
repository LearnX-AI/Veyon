/*
 * SessionMonitor.cpp
 */

#include "SessionMonitor.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QtDebug>
#include <QUrl>


SessionMonitor::SessionMonitor(
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


void SessionMonitor::runOneTick()
{
    if( m_tickInFlight )
    {
        return;
    }
    m_tickInFlight = true;

    const QUrl url( m_serverUrl
                  + QStringLiteral("/api/v1/sessions/agent-active") );
    QNetworkRequest req{ url };
    req.setRawHeader( "Authorization",
                      QByteArray("Bearer ") + m_token.toUtf8() );
    req.setRawHeader( "X-Veyon-Hostname", m_hostname.toUtf8() );

    QNetworkReply* reply = m_nam->get( req );
    connect( reply, &QNetworkReply::finished,
             this, &SessionMonitor::onActiveSessionResponse );
}


void SessionMonitor::onActiveSessionResponse()
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
        qWarning().noquote() << "[session] /agent-active HTTP" << httpCode;
        m_tickInFlight = false;
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson( body );

    // Response is either null (no active session) or a JSON object.
    if( doc.isNull() || (doc.isObject() && doc.object().isEmpty()) )
    {
        clearSessionState();
        m_tickInFlight = false;
        return;
    }

    if( !doc.isObject() )
    {
        m_tickInFlight = false;
        return;
    }

    const QJsonObject obj = doc.object();
    const int sessionId = obj.value( QStringLiteral("session_id") ).toInt();
    const QString name  = obj.value( QStringLiteral("name") ).toString();
    const QString endsAtStr = obj.value( QStringLiteral("ends_at") ).toString();
    const QString action    = obj.value( QStringLiteral("timeout_action") ).toString();

    // ISO-8601 timestamp from the server
    const QDateTime endsAt = QDateTime::fromString( endsAtStr, Qt::ISODateWithMs );
    QDateTime endsAtUtc = endsAt;
    if( !endsAtUtc.isValid() )
    {
        // Some servers send without ms - try plain ISO
        endsAtUtc = QDateTime::fromString( endsAtStr, Qt::ISODate );
    }
    if( !endsAtUtc.isValid() )
    {
        qWarning().noquote() << "[session] invalid ends_at:" << endsAtStr;
        m_tickInFlight = false;
        return;
    }
    endsAtUtc.setTimeSpec( Qt::UTC );

    QList<int> warningMinutes;
    for( const QJsonValue& v : obj.value( QStringLiteral("warning_minutes") ).toArray() )
    {
        warningMinutes.append( v.toInt() );
    }

    handleSession( sessionId, name, endsAtUtc, action, warningMinutes );
    m_tickInFlight = false;
}


void SessionMonitor::handleSession(
    int sessionId,
    const QString& sessionName,
    const QDateTime& endsAt,
    const QString& timeoutAction,
    const QList<int>& warningMinutes )
{
    // If this is a different session than we were tracking, reset state.
    if( m_activeSessionId != sessionId )
    {
        m_activeSessionId = sessionId;
        m_firedWarnings.clear();
        m_actionFired = false;
        qInfo().noquote() << "[session] tracking session" << sessionId
                          << ":" << sessionName
                          << "ends at" << endsAt.toString( Qt::ISODate );
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qint64 secsRemaining = now.secsTo( endsAt );

    // Has time expired? Fire the action exactly once.
    if( secsRemaining <= 0 )
    {
        if( !m_actionFired )
        {
            m_actionFired = true;
            const QString result = executeTimeoutAction( timeoutAction );
            qInfo().noquote() << "[session] timeout fired:"
                              << timeoutAction << "->" << result;
            reportEvent( sessionId, QStringLiteral("action_fired"),
                         timeoutAction + QStringLiteral(": ") + result );
        }
        return;
    }

    // Check warning thresholds. Each value is "minutes-before-end".
    // We fire when the time crosses INTO that window for the first time.
    const qint64 minutesRemaining = (secsRemaining + 59) / 60;     // ceiling
    for( int threshold : warningMinutes )
    {
        if( minutesRemaining <= threshold && !m_firedWarnings.contains( threshold ) )
        {
            m_firedWarnings.insert( threshold );

            const QString summary = QStringLiteral("Veyon: %1 min remaining").arg( threshold );
            const QString body = QStringLiteral("Session \"%1\" ends in %2 minute%3.")
                                 .arg( sessionName )
                                 .arg( threshold )
                                 .arg( threshold == 1 ? QString() : QStringLiteral("s") );

            const bool ok = sendNotification( summary, body );
            qInfo().noquote() << "[session] warned at" << threshold
                              << "min (delivered=" << ok << ")";
            reportEvent( sessionId, QStringLiteral("warned"),
                         QStringLiteral("at -%1 min").arg( threshold ) );
        }
    }
}


void SessionMonitor::clearSessionState()
{
    if( m_activeSessionId != -1 )
    {
        qInfo().noquote() << "[session] no active session; resetting state";
    }
    m_activeSessionId = -1;
    m_firedWarnings.clear();
    m_actionFired = false;
}


QString SessionMonitor::activeUser() const
{
    // Use 'loginctl list-users' to find the first non-system user with a session
    QProcess proc;
    proc.start( QStringLiteral("loginctl"),
                { QStringLiteral("list-users"), QStringLiteral("--no-legend") } );
    if( !proc.waitForFinished( 3000 ) )
    {
        return QString();
    }
    const QStringList lines = QString::fromUtf8( proc.readAllStandardOutput() )
                                .split( QLatin1Char('\n'), Qt::SkipEmptyParts );
    for( const QString& line : lines )
    {
        const QStringList cols = line.simplified().split( QLatin1Char(' '), Qt::SkipEmptyParts );
        if( cols.size() >= 2 )
        {
            bool ok = false;
            const int uid = cols.at( 0 ).toInt( &ok );
            if( ok && uid >= 1000 )       // skip system users
            {
                return cols.at( 1 );
            }
        }
    }
    return QString();
}


bool SessionMonitor::sendNotification( const QString& summary, const QString& body )
{
    const QString user = activeUser();
    if( user.isEmpty() )
    {
        qWarning().noquote() << "[session] no active user, skipping notification";
        return false;
    }

    // sudo -u <user> DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/<uid>/bus notify-send ...
    // Need to determine the UID for the bus path.
    QProcess uidProc;
    uidProc.start( QStringLiteral("id"), { QStringLiteral("-u"), user } );
    uidProc.waitForFinished( 2000 );
    const QString uid = QString::fromUtf8( uidProc.readAllStandardOutput() ).trimmed();
    if( uid.isEmpty() )
    {
        return false;
    }

    const QString busPath = QStringLiteral("unix:path=/run/user/%1/bus").arg( uid );

    QStringList args;
    args << QStringLiteral("-u") << user
         << QStringLiteral("DBUS_SESSION_BUS_ADDRESS=") + busPath
         << QStringLiteral("notify-send")
         << QStringLiteral("-u") << QStringLiteral("critical")
         << QStringLiteral("-i") << QStringLiteral("dialog-warning")
         << summary << body;

    QProcess notifyProc;
    notifyProc.start( QStringLiteral("sudo"), args );
    const bool finished = notifyProc.waitForFinished( 3000 );
    return finished && notifyProc.exitCode() == 0;
}


QString SessionMonitor::executeTimeoutAction( const QString& action )
{
    QProcess proc;
    QString cmd;
    QStringList args;

    if( action == QStringLiteral("lock_screen") )
    {
        // Lock all active GUI sessions
        cmd  = QStringLiteral("loginctl");
        args = { QStringLiteral("lock-sessions") };
    }
    else if( action == QStringLiteral("logout") )
    {
        const QString user = activeUser();
        if( user.isEmpty() )
        {
            return QStringLiteral("no active user");
        }
        cmd  = QStringLiteral("loginctl");
        args = { QStringLiteral("terminate-user"), user };
    }
    else if( action == QStringLiteral("shutdown") )
    {
        cmd  = QStringLiteral("systemctl");
        args = { QStringLiteral("poweroff") };
    }
    else
    {
        return QStringLiteral("unknown action: ") + action;
    }

    proc.start( cmd, args );
    if( !proc.waitForFinished( 5000 ) )
    {
        return QStringLiteral("timeout");
    }
    return proc.exitCode() == 0
        ? QStringLiteral("ok")
        : QStringLiteral("exit=") + QString::number( proc.exitCode() )
          + QStringLiteral(" stderr=")
          + QString::fromUtf8( proc.readAllStandardError() ).left( 120 );
}


void SessionMonitor::reportEvent(
    int sessionId,
    const QString& eventType,
    const QString& details )
{
    QJsonObject payload;
    payload[QStringLiteral("event_type")] = eventType;
    if( !details.isEmpty() )
    {
        payload[QStringLiteral("details")] = details;
    }

    const QUrl url( m_serverUrl
                  + QStringLiteral("/api/v1/sessions/")
                  + QString::number( sessionId )
                  + QStringLiteral("/agent-event") );

    QNetworkRequest req{ url };
    req.setHeader( QNetworkRequest::ContentTypeHeader,
                   QStringLiteral("application/json") );
    req.setRawHeader( "Authorization",
                      QByteArray("Bearer ") + m_token.toUtf8() );
    req.setRawHeader( "X-Veyon-Hostname", m_hostname.toUtf8() );

    const QByteArray body =
        QJsonDocument( payload ).toJson( QJsonDocument::Compact );

    QNetworkReply* reply = m_nam->post( req, body );
    connect( reply, &QNetworkReply::finished, this, [reply]() {
        const int hc = reply->attribute( QNetworkRequest::HttpStatusCodeAttribute ).toInt();
        if( reply->error() != QNetworkReply::NoError || hc < 200 || hc >= 300 )
        {
            qWarning().noquote() << "[session] event report HTTP" << hc
                                 << reply->errorString();
        }
        reply->deleteLater();
    });
}
