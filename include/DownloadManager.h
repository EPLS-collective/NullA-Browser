/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QList>
#include <QPointer>
#include "Download.h"

class DownloadManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    static DownloadManager* instance();

    void addDownload(Download* download);
    void removeDownload(Download* download);
    void cancelDownload(int index);
    void cancelAllDownloads();

    int activeCount() const { return m_downloads.size(); }
    QString statusText() const;
    QList<Download*> downloads() const { return m_downloads; }

signals:
    void activeCountChanged();
    void statusTextChanged();
    void downloadAdded(Download* download);
    void downloadRemoved(Download* download);

private:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager();

    static DownloadManager* m_instance;
    QList<Download*> m_downloads;
};

#endif // DOWNLOADMANAGER_H
