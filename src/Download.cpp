/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/Download.h"
#include <QFileInfo>

Download::Download(QWebEngineDownloadRequest* download, QObject *parent)
: QObject(parent)
, m_download(download)
{
    if (!download) return;

    m_fileName = download->downloadFileName();
    m_receivedBytes = download->receivedBytes();
    m_totalBytes = download->totalBytes();

    switch (download->state()) {
        case QWebEngineDownloadRequest::DownloadInProgress:
            m_state = DownloadInProgress;
            break;
        case QWebEngineDownloadRequest::DownloadCompleted:
            m_state = DownloadCompleted;
            break;
        case QWebEngineDownloadRequest::DownloadCancelled:
            m_state = DownloadCancelled;
            break;
        case QWebEngineDownloadRequest::DownloadInterrupted:
            m_state = DownloadInterrupted;
            break;
    }

    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged,
            this, &Download::onReceivedBytesChanged);
    connect(download, &QWebEngineDownloadRequest::stateChanged,
            this, &Download::onStateChanged);
}

Download::~Download()
{
    if (m_download && m_state == DownloadInProgress) {
        m_download->cancel();
    }
}

int Download::progressPercent() const
{
    if (m_totalBytes <= 0) return 0;
    return static_cast<int>((static_cast<double>(m_receivedBytes) / m_totalBytes) * 100);
}

QString Download::progressBar() const
{
    return getTerminalProgress(m_receivedBytes, m_totalBytes);
}

void Download::cancel()
{
    if (m_download && m_state == DownloadInProgress) {
        m_download->cancel();
    }
}

bool Download::isFinished() const
{
    return m_state != DownloadInProgress;
}

void Download::onReceivedBytesChanged()
{
    if (!m_download) return;

    m_receivedBytes = m_download->receivedBytes();
    m_totalBytes = m_download->totalBytes();
    updateProgress();
}

void Download::onStateChanged()
{
    if (!m_download) return;

    switch (m_download->state()) {
        case QWebEngineDownloadRequest::DownloadInProgress:
            m_state = DownloadInProgress;
            break;
        case QWebEngineDownloadRequest::DownloadCompleted:
            m_state = DownloadCompleted;
            break;
        case QWebEngineDownloadRequest::DownloadCancelled:
            m_state = DownloadCancelled;
            break;
        case QWebEngineDownloadRequest::DownloadInterrupted:
            m_state = DownloadInterrupted;
            break;
    }

    emit stateChanged();

    if (isFinished()) {
        emit finished();
    }
}

void Download::updateProgress()
{
    emit progressChanged();
}

QString Download::getTerminalProgress(qint64 received, qint64 total) const
{
    if (total <= 0) return "[Uncountable...]";

    int width = 10;
    double progress = static_cast<double>(received) / total;
    int filled = static_cast<int>(progress * width);

    QString bar = "[";
    for (int i = 0; i < width; ++i) {
        if (i < filled) bar += "#";
        else bar += "-";
    }

    return bar + QString("] %1%").arg(static_cast<int>(progress * 100), 3);
}
