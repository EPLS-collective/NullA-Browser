/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject* parent = nullptr);

    void checkForUpdates();
    void downloadUpdate(const QString &downloadUrl);
    QString currentVersion() const;
    void installUpdate(const QString &archivePath);

signals:
    void updateAvailable(const QString &latestVersion, const QString &releaseUrl,
                         const QString &releaseNotes, const QString &downloadUrl);
    void upToDate();
    void checkFailed(const QString &errorString);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString &filePath);
    void downloadFailed(const QString &errorString);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_manager;
    QNetworkReply* m_downloadReply = nullptr;

    static QString normalizeVersion(QString v);
    static int compareVersions(const QString &a, const QString &b);
};

#endif // UPDATECHECKER_H
