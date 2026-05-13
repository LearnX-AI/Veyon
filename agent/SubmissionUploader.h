/*
 * SubmissionUploader.h - Uploads one submission file to the central server.
 *
 * Workflow per file:
 *   1. Build a multipart POST body containing the file's bytes
 *   2. POST /api/v1/folders/{folder_id}/submit with Bearer + X-Veyon-Hostname
 *   3. On success: rename file to "<original>.submitted"
 *   4. On failure: leave the file untouched (next sync tick will retry)
 *
 * Each instance handles ONE file. Caller creates a fresh instance per
 * upload and deletes it via deleteLater() when done() fires.
 */

#pragma once

#include <QObject>
#include <QString>


class QFile;
class QHttpMultiPart;
class QNetworkAccessManager;
class QNetworkReply;


class SubmissionUploader : public QObject
{
    Q_OBJECT

public:
    struct PendingSubmission
    {
        int     folderId = 0;
        QString folderName;     // for logs only
        QString filePath;       // absolute path on disk
    };

    /**
     * @param nam        QNetworkAccessManager owned by the agent
     * @param serverUrl  base URL e.g. http://server:8000 (no trailing slash)
     * @param token      Bearer token
     * @param hostname   this machine's hostname (X-Veyon-Hostname header)
     */
    SubmissionUploader(
        QNetworkAccessManager* nam,
        const QString& serverUrl,
        const QString& token,
        const QString& hostname,
        QObject* parent = nullptr );

    ~SubmissionUploader() override;

    /// Begin the upload. done() is emitted exactly once when finished.
    void start( const PendingSubmission& info );

Q_SIGNALS:
    /// @param success  true if the file was uploaded AND renamed successfully
    /// @param info     the PendingSubmission we tried to send
    /// @param message  empty on success, error detail on failure
    void done( bool success, SubmissionUploader::PendingSubmission info, QString message );

private Q_SLOTS:
    void onFinished();

private:
    void fail( const QString& message );
    void succeed();

    QNetworkAccessManager* m_nam;
    QString m_serverUrl;
    QString m_token;
    QString m_hostname;

    PendingSubmission m_info;
    QNetworkReply*    m_reply     = nullptr;
    QFile*            m_file      = nullptr;
    QHttpMultiPart*   m_multipart = nullptr;
    bool              m_finished  = false;
};
