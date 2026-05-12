/*
 * FileDownloader.h - Downloads files from the central server to disk.
 *
 * Workflow per file:
 *   1. GET /api/v1/files/{distribution_id}/download
 *      - Streams bytes directly to a temp file (no full-file load in memory)
 *      - Computes SHA-256 during the write
 *   2. Verifies SHA against the value provided by /pending
 *   3. Atomically renames temp file to final destination
 *   4. POSTs /ack with success or detailed failure
 *
 * Each FileDownloader instance handles exactly one file. Caller creates
 * a fresh one per download and deletes it via QObject parenting or
 * deleteLater() when done() is emitted.
 */

#pragma once

#include <QObject>
#include <QString>

#include <functional>


class HttpClient;
class QFile;
class QNetworkAccessManager;
class QNetworkReply;
class QCryptographicHash;


class FileDownloader : public QObject
{
    Q_OBJECT

public:
    struct PendingFile
    {
        int     distributionId = 0;
        int     fileId         = 0;
        QString storageId;
        QString filename;
        QString sha256;
        qint64  sizeBytes      = 0;
    };

    /**
     * @param http   already-configured HttpClient (re-used for the ACK call)
     * @param nam    QNetworkAccessManager owned by the agent - used directly
     *               for the download because we need streaming reads, not
     *               the full-body callback pattern HttpClient uses.
     * @param serverUrl   base URL e.g. http://server:8000 (no trailing slash)
     * @param token       Bearer token
     * @param hostname    this machine's hostname (for X-Veyon-Hostname header)
     * @param destDir     directory where downloaded files land
     */
    FileDownloader(
        HttpClient* http,
        QNetworkAccessManager* nam,
        const QString& serverUrl,
        const QString& token,
        const QString& hostname,
        const QString& destDir,
        QObject* parent = nullptr );

    ~FileDownloader() override;

    /// Begin downloading the given file. done() is emitted on success or failure.
    void start( const PendingFile& info );

Q_SIGNALS:
    /// Fired exactly once when the whole pipeline (download + verify + ack) finishes.
    /// @param success  true if the file was delivered + ACK'd as success
    /// @param info     the PendingFile we were asked to fetch
    /// @param message  human-readable detail (empty on success, error on failure)
    void done( bool success, FileDownloader::PendingFile info, QString message );

private Q_SLOTS:
    void onReadyRead();
    void onDownloadFinished();

private:
    void fail( const QString& message );
    void succeed();
    void postAck( bool success, qint64 bytes, const QString& message );

    HttpClient*            m_http;
    QNetworkAccessManager* m_nam;
    QString                m_serverUrl;
    QString                m_token;
    QString                m_hostname;
    QString                m_destDir;

    PendingFile            m_info;
    QNetworkReply*         m_reply = nullptr;
    QFile*                 m_tempFile = nullptr;
    QString                m_tempPath;
    QString                m_finalPath;
    QCryptographicHash*    m_hash = nullptr;
    qint64                 m_bytesWritten = 0;
    bool                   m_finished = false;
};
