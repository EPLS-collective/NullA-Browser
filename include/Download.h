/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QObject>
#include <QWebEngineDownloadRequest>
#include <QPointer>

class Download : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileNameChanged)
    Q_PROPERTY(qint64 receivedBytes READ receivedBytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 totalBytes READ totalBytes NOTIFY progressChanged)
    Q_PROPERTY(int progressPercent READ progressPercent NOTIFY progressChanged)
    Q_PROPERTY(QString progressBar READ progressBar NOTIFY progressChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)

public:
    enum State {
        DownloadInProgress,
        DownloadCompleted,
        DownloadCancelled,
        DownloadInterrupted
    };
    Q_ENUM(State)

    explicit Download(QWebEngineDownloadRequest* download, QObject *parent = nullptr);
    ~Download();

    QString fileName() const { return m_fileName; }
    qint64 receivedBytes() const { return m_receivedBytes; }
    qint64 totalBytes() const { return m_totalBytes; }
    int progressPercent() const;
    QString progressBar() const;
    State state() const { return m_state; }

    void cancel();
    bool isFinished() const;

signals:
    void fileNameChanged();
    void progressChanged();
    void stateChanged();
    void finished();

private slots:
    void onReceivedBytesChanged();
    void onStateChanged();

private:
    void updateProgress();
    QString getTerminalProgress(qint64 received, qint64 total) const;

    QPointer<QWebEngineDownloadRequest> m_download;
    QString m_fileName;
    qint64 m_receivedBytes = 0;
    qint64 m_totalBytes = 0;
    State m_state = DownloadInProgress;
};

#endif // DOWNLOAD_H
