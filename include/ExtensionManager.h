/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef EXTENSIONMANAGER_H
#define EXTENSIONMANAGER_H

#include <QObject>
#include <QHash>

class QWebEngineProfile;
class QWebEnginePage;
class QSettings;
class QJsonObject;

class ExtensionManager : public QObject {
    Q_OBJECT
public:
    explicit ExtensionManager(QWebEngineProfile *profile, QSettings *settings, QObject *parent = nullptr);

    void loadExtensions();
    void setExtensionEnabled(const QString &extId, bool enabled);
    bool isExtensionEnabled(const QString &extId) const;

    // Store install flow: extract the zip, write the store metadata, then
    // inject the scripts. Mirrors the manual unzip-into-folder path
    void installExtension(const QString &zipPath, const QString &extId,
                          const QString &name, const QString &description,
                          const QString &version, const QString &author);

    // Un-injects scripts and removes the extension directory.
    void uninstallExtension(const QString &extId);

    // Injected before every extension script (content + background). Pure
    // string builder, used by the tab-facing scripting API too
    static QString chromePolyfillFor(const QString &extId, bool isBackground = false);

private:
    void loadExtensionScripts(const QString &extId);
    void unloadExtensionScripts(const QString &extId);
    void loadExtensionBackground(const QString &extId, const QString &extPath, const QJsonObject &manifestJson);
    void unloadExtensionBackground(const QString &extId);
    void extractZip(const QString &zipPath, const QString &destDir);
    static QString extensionsRoot();

    QWebEngineProfile *m_profile = nullptr;
    QSettings *m_settings = nullptr;
    QHash<QString, QStringList> m_extensionScriptNames;
    QHash<QString, QWebEnginePage*> m_backgroundPages;
};

#endif // EXTENSIONMANAGER_H
