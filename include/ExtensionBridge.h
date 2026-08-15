/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef EXTENSIONBRIDGE_H
#define EXTENSIONBRIDGE_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QVariantMap>
#include <functional>

class QWebChannel;

// Backs the browser (chromium M3) polyfills injected into content scripts
// and the background page. A single instance is shared across all tabs and
// the background page, allowing messages and responses to cross contexts.
class ExtensionBridge : public QObject {
    Q_OBJECT
public:
    static ExtensionBridge *instance();

    // Lazily creates the shared QWebChannel and registers this instance as
    // "extensionBridge". Each QWebEnginePage should use this channel.
    static QWebChannel *channel();

    // chrome.storage.local backing, scoped per extension.
    // Values are stored as JSON strings and persisted to disk with QSettings.
    Q_INVOKABLE QString storageGetAllJson(const QString &extId) const;
    Q_INVOKABLE void storageSet(const QString &extId, const QString &key, const QString &jsonValue);
    Q_INVOKABLE void storageRemove(const QString &extId, const QString &key);

    // chrome.storage.session backing, scoped per extension.
    // Stored in memory only and cleared when the application exits.
    Q_INVOKABLE QString sessionStorageGetAllJson(const QString &extId) const;
    Q_INVOKABLE void sessionStorageSet(const QString &extId, const QString &key, const QString &jsonValue);
    Q_INVOKABLE void sessionStorageRemove(const QString &extId, const QString &key);

    // Sends messages to all contexts of the extension.
    // sendResponse() routes the reply back to the sender using requestId.
    Q_INVOKABLE void sendMessage(const QString &extId, const QString &requestId, const QString &messageJson);
    Q_INVOKABLE void sendResponse(const QString &extId, const QString &requestId, const QString &responseJson);

    // Supports chrome.tabs.query and chrome.scripting.executeScript for the
    // background context. Browser provides the actual tab operations.
    Q_INVOKABLE QString queryTabs(const QString &urlPattern) const;
    Q_INVOKABLE bool executeScriptInTab(int tabId, const QString &extId, const QString &fileName);

    using TabQueryHandler = std::function<QString(const QString &urlPattern)>;
    using ScriptExecHandler = std::function<bool(int tabId, const QString &extId, const QString &fileName)>;

    static void setTabQueryHandler(TabQueryHandler handler);
    static void setScriptExecHandler(ScriptExecHandler handler);

signals:
    void messageReceived(const QString &extId, const QString &requestId, const QString &messageJson);
    void responseReceived(const QString &extId, const QString &requestId, const QString &responseJson);

private:
    explicit ExtensionBridge(QObject *parent = nullptr);

    static ExtensionBridge *m_instance;
    static QWebChannel *m_channel;
    static TabQueryHandler m_tabQueryHandler;
    static ScriptExecHandler m_scriptExecHandler;

    // extId -> (key -> JSON-encoded value). In-memory only.
    QHash<QString, QVariantMap> m_sessionStorage;
};

#endif // EXTENSIONBRIDGE_H
