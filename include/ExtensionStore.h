/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef EXTENSIONSTORE_H
#define EXTENSIONSTORE_H

#include <QDialog>
#include <QList>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QListWidget;
class QLabel;
class QLineEdit;
class QPushButton;

class ExtensionStore : public QDialog {
    Q_OBJECT
public:
    struct ExtensionInfo {
        QString id;
        QString name;
        QString description;
        QString version;
        QString author;
        QString downloadUrl;
    };

    explicit ExtensionStore(QWidget *parent = nullptr);
    void refreshInstalledState();

signals:

    void extensionInstallRequested(const QString &zipPath, const QString &extId,
                                    const QString &name, const QString &description,
                                    const QString &version, const QString &author);
    void extensionUninstallRequested(const QString &extId);
    void extensionToggleRequested(const QString &extId, bool enabled);

private slots:
    void fetchIndex();
    void onIndexReplyFinished(QNetworkReply *reply);
    void filterList(const QString &text);

private:
    void loadInstalledLocally();
    void populateList();
    void startInstall(const ExtensionInfo &info, QPushButton *button);
    bool isInstalled(const QString &id) const;
    bool isExtensionEnabled(const QString &id) const;
    QString extensionsRoot() const;
    int indexOfExtension(const QString &id) const;

    void applyTheme(bool isDark);
    bool isSystemDarkTheme() const;

    QNetworkAccessManager *m_nam;
    QListWidget *m_list;
    QLineEdit *m_searchBox;
    QLabel *m_statusLabel;
    QString m_rowBgColor;
    QList<ExtensionInfo> m_extensions;

    static const QString kIndexUrl;
    static const QString kMetaFileName;
};

#endif // EXTENSIONSTORE_H
