/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/ExtensionBridge.h"

#include <QWebChannel>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>

ExtensionBridge *ExtensionBridge::m_instance = nullptr;
QWebChannel *ExtensionBridge::m_channel = nullptr;
ExtensionBridge::TabQueryHandler ExtensionBridge::m_tabQueryHandler = nullptr;
ExtensionBridge::ScriptExecHandler ExtensionBridge::m_scriptExecHandler = nullptr;

ExtensionBridge::ExtensionBridge(QObject *parent) : QObject(parent) {}

ExtensionBridge *ExtensionBridge::instance() {
    if (!m_instance) {
        m_instance = new ExtensionBridge();
    }
    return m_instance;
}

QWebChannel *ExtensionBridge::channel() {
    if (!m_channel) {
        m_channel = new QWebChannel();
        m_channel->registerObject("extensionBridge", instance());
    }
    return m_channel;
}

static QString storageGroup(const QString &extId) {
    return "extensions/storage/" + extId;
}

QString ExtensionBridge::storageGetAllJson(const QString &extId) const {
    QSettings settings("NullA", "Browser");
    settings.beginGroup(storageGroup(extId));
    const QStringList keys = settings.childKeys();

    QJsonObject obj;
    for (const QString &key : keys) {
        // Stored value is already a JSON-encoded string (see storageSet);
        // pass it through as-is, the JS side does the final JSON.parse.
        obj[key] = settings.value(key).toString();
    }
    settings.endGroup();

    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void ExtensionBridge::storageSet(const QString &extId, const QString &key, const QString &jsonValue) {
    QSettings settings("NullA", "Browser");
    settings.setValue(storageGroup(extId) + "/" + key, jsonValue);
}

void ExtensionBridge::storageRemove(const QString &extId, const QString &key) {
    QSettings settings("NullA", "Browser");
    settings.remove(storageGroup(extId) + "/" + key);
}

QString ExtensionBridge::sessionStorageGetAllJson(const QString &extId) const {
    QJsonObject obj;
    const QVariantMap &map = m_sessionStorage.value(extId);
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        obj[it.key()] = it.value().toString();
    }
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void ExtensionBridge::sessionStorageSet(const QString &extId, const QString &key, const QString &jsonValue) {
    m_sessionStorage[extId][key] = jsonValue;
}

void ExtensionBridge::sessionStorageRemove(const QString &extId, const QString &key) {
    if (!m_sessionStorage.contains(extId)) return;
    m_sessionStorage[extId].remove(key);
}

void ExtensionBridge::sendMessage(const QString &extId, const QString &requestId, const QString &messageJson) {
    emit messageReceived(extId, requestId, messageJson);
}

void ExtensionBridge::sendResponse(const QString &extId, const QString &requestId, const QString &responseJson) {
    emit responseReceived(extId, requestId, responseJson);
}

QString ExtensionBridge::queryTabs(const QString &urlPattern) const {
    if (m_tabQueryHandler) return m_tabQueryHandler(urlPattern);
    return QStringLiteral("[]");
}

bool ExtensionBridge::executeScriptInTab(int tabId, const QString &extId, const QString &fileName) {
    if (m_scriptExecHandler) return m_scriptExecHandler(tabId, extId, fileName);
    return false;
}

void ExtensionBridge::setTabQueryHandler(TabQueryHandler handler) {
    m_tabQueryHandler = std::move(handler);
}

void ExtensionBridge::setScriptExecHandler(ScriptExecHandler handler) {
    m_scriptExecHandler = std::move(handler);
}
