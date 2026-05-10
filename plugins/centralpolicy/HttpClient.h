/*
 * HttpClient.h - clean async wrapper around QNetworkAccessManager.
 *
 * Handles:
 *   - Base URL prefixing (so callers use "/api/v1/blocklist" not full URL)
 *   - Automatic "Authorization: Bearer <token>" header injection
 *   - JSON serialization of request bodies
 *   - JSON parsing of responses
 *   - Timeout handling
 *
 * All methods are non-blocking; results arrive via the supplied callback.
 */

#pragma once

#include <QObject>
#include <QJsonObject>

#include <functional>


class QNetworkAccessManager;
class QNetworkReply;


class HttpClient : public QObject
{
    Q_OBJECT

public:
    /// Result passed to every callback.
    struct Response
    {
        bool ok;                ///< true if HTTP 2xx
        int statusCode;         ///< HTTP status (0 if network error)
        QString errorString;    ///< populated on failure
        QJsonValue json;        ///< parsed body (may be null)
    };

    using Callback = std::function<void(const Response&)>;

    explicit HttpClient( QObject* parent = nullptr );
    ~HttpClient() override;

    /// Configure the client. Must be called before any request.
    void configure( const QString& baseUrl, const QString& bearerToken );

    void get( const QString& path, Callback callback );
    void post( const QString& path, const QJsonObject& body, Callback callback );

private:
    void send( const QByteArray& method,
               const QString& path,
               const QJsonObject* body,
               Callback callback );

    QNetworkAccessManager* m_nam;
    QString m_baseUrl;
    QString m_bearerToken;
    int m_timeoutMs;
};
