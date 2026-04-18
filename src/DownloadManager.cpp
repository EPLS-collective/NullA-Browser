/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/DownloadManager.h"
#include <QDebug>

DownloadManager* DownloadManager::m_instance = nullptr;

DownloadManager* DownloadManager::instance()
{
    if (!m_instance) {
        m_instance = new DownloadManager();
    }
    return m_instance;
}

DownloadManager::DownloadManager(QObject *parent)
: QObject(parent)
{
}

DownloadManager::~DownloadManager()
{
    cancelAllDownloads();
    qDeleteAll(m_downloads);
    m_downloads.clear();
}

void DownloadManager::addDownload(Download* download)
{
    if (!download || m_downloads.contains(download)) return;

    m_downloads.append(download);
    emit downloadAdded(download);
    emit activeCountChanged();
    emit statusTextChanged();

    connect(download, &Download::finished, this, [this, download]() {
        removeDownload(download);
    });
}

void DownloadManager::removeDownload(Download* download)
{
    if (!download) return;

    m_downloads.removeAll(download);
    emit downloadRemoved(download);
    emit activeCountChanged();
    emit statusTextChanged();

    download->deleteLater();
}

void DownloadManager::cancelDownload(int index)
{
    if (index >= 0 && index < m_downloads.size()) {
        m_downloads[index]->cancel();
    }
}

void DownloadManager::cancelAllDownloads()
{
    for (Download* d : m_downloads) {
        d->cancel();
    }
}

QString DownloadManager::statusText() const
{
    if (m_downloads.isEmpty()) return QString();

    QStringList display;
    for (int i = 0; i < m_downloads.size(); ++i) {
        Download* d = m_downloads[i];
        if (!d) continue;

        display << QString("%1:%2 %3").arg(i + 1)
        .arg(d->fileName(), d->progressBar());
    }

    if (display.isEmpty()) return QString();

    return display.join(" | ") + " (Press Ctrl+C to cancel download.)";
}
