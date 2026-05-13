/*
 * FolderSyncer.h - Periodically syncs shared folder state with the server
 * and uploads any new submission files.
 *
 * On each tick (every folder_sync_interval_seconds):
 *   1. GET /api/v1/folders/agent-sync (with X-Veyon-Hostname)
 *   2. For each folder returned:
 *      - Ensure <submissions_root>/<safe-name>/ exists
 *      - Skip if folder is CLOSED
 *      - Scan the directory for files NOT ending in .submitted, NOT hidden,
 *        NOT directories
 *   3. Process candidates one at a time via SubmissionUploader. After each
 *      finishes, kick off the next.
 *
 * State is kept on disk via the .submitted rename: nothing in this class
 * survives a restart.
 */

#pragma once

#include "SubmissionUploader.h"

#include <QObject>
#include <QQueue>
#include <QString>


class QNetworkAccessManager;
class QNetworkReply;


class FolderSyncer : public QObject
{
    Q_OBJECT

public:
    FolderSyncer(
        QNetworkAccessManager* nam,
        const QString& serverUrl,
        const QString& token,
        const QString& hostname,
        const QString& submissionsRootDir,
        QObject* parent = nullptr );

    /// Called by PolicyAgent on each timer tick. Safe to call concurrently
    /// with an in-flight upload (will no-op until the previous tick finishes).
    void runOneTick();

Q_SIGNALS:
    /// Emitted after a tick finishes (with no pending work). For tests / logs.
    void tickDone();

private Q_SLOTS:
    void onSyncResponseFinished();
    void onUploadDone( bool success, SubmissionUploader::PendingSubmission info, QString message );

private:
    /// Turn a user-facing folder name into a safe filesystem directory name.
    /// e.g. "Math 101 - Assignment 1" -> "Math_101_-_Assignment_1"
    static QString safeFolderName( const QString& raw );

    /// Process the next pending submission, if any.
    void processNextPending();

    QNetworkAccessManager* m_nam;
    QString m_serverUrl;
    QString m_token;
    QString m_hostname;
    QString m_submissionsRootDir;

    bool m_tickInFlight = false;
    bool m_uploadInFlight = false;
    QQueue<SubmissionUploader::PendingSubmission> m_queue;
};
