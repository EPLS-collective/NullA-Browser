/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/ExtensionManager.h"
#include "../include/ExtensionBridge.h"

#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QProcess>
#include <QHash>
#include <QStringList>

ExtensionManager::ExtensionManager(QWebEngineProfile *profile, QSettings *settings, QObject *parent)
    : QObject(parent), m_profile(profile), m_settings(settings) {
}

void ExtensionManager::loadExtensions() {
    QDir rootDir(extensionsRoot());

    if (!rootDir.exists()) return;

    QStringList subDirs = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &dirName : subDirs) {
        if (m_extensionScriptNames.contains(dirName)) continue;
        if (!isExtensionEnabled(dirName)) continue;

        loadExtensionScripts(dirName);
    }
}

void ExtensionManager::setExtensionEnabled(const QString &extId, bool enabled) {
    m_settings->setValue("extensions/disabled/" + extId, !enabled);

    if (enabled) {
        if (!m_extensionScriptNames.contains(extId)) {
            loadExtensionScripts(extId);
        }
    } else {
        unloadExtensionScripts(extId);
    }
}

bool ExtensionManager::isExtensionEnabled(const QString &extId) const {
    return !m_settings->value("extensions/disabled/" + extId, false).toBool();
}

void ExtensionManager::installExtension(const QString &zipPath, const QString &extId,
                                        const QString &name, const QString &description,
                                        const QString &version, const QString &author) {
    QString destDir = extensionsRoot() + "/" + extId;
    QDir().mkpath(destDir);

    extractZip(zipPath, destDir);

    QJsonObject meta;
    meta["id"] = extId;
    meta["name"] = name;
    meta["description"] = description;
    meta["version"] = version;
    meta["author"] = author;

    QFile metaFile(destDir + "/.nulla_store_meta.json");
    if (metaFile.open(QIODevice::WriteOnly)) {
        metaFile.write(QJsonDocument(meta).toJson());
        metaFile.close();
    }

    loadExtensionScripts(extId);
    QFile::remove(zipPath);
}

void ExtensionManager::uninstallExtension(const QString &extId) {
    unloadExtensionScripts(extId);

    QDir(extensionsRoot() + "/" + extId).removeRecursively();
}

QString ExtensionManager::chromePolyfillFor(const QString &extId, bool isBackground) {
    QString extIdEscaped = extId;
    extIdEscaped.replace("\\", "\\\\").replace("'", "\\'");

    return QStringLiteral(R"JS(
(function() {
    var extId = '%1';
    var isBackground = %2;
    var contextId = 'ctx_' + Math.random().toString(36).slice(2) + '_' + Date.now();

    var listeners = [];
    var installListeners = [];
    var startupListeners = [];
    var bridgeReady = null;
    var pending = [];
    var pendingCallbacks = {};
    var reqCounter = 0;

    function withBridge(fn) {
        if (bridgeReady) { fn(bridgeReady); return; }
        pending.push(fn);
        if (window.__nullaChannelInitStarted) return;
        window.__nullaChannelInitStarted = true;
        function tryInit() {
            if (window.qt && window.qt.webChannelTransport && window.QWebChannel) {
                new QWebChannel(qt.webChannelTransport, function(channel) {
                    bridgeReady = channel.objects.extensionBridge;

                    bridgeReady.messageReceived.connect(function(msgExtId, requestId, messageJson) {
                        if (msgExtId !== extId) return;

                        var wrapper;
                        try { wrapper = JSON.parse(messageJson); } catch (e) { wrapper = null; }
                        if (!wrapper || wrapper.senderContextId === contextId) return;

                        var message = wrapper.payload;
                        var responded = false;
                        function sendResponse(response) {
                            if (responded) return;
                            responded = true;
                            try {
                                bridgeReady.sendResponse(extId, requestId, JSON.stringify(response === undefined ? null : response));
                            } catch (e) {}
                        }

                        listeners.forEach(function(fn) {
                            try { fn(message, { id: extId }, sendResponse); } catch (e) {}
                        });
                    });

                    bridgeReady.responseReceived.connect(function(msgExtId, requestId, responseJson) {
                        if (msgExtId !== extId) return;
                        var cb = pendingCallbacks[requestId];
                        if (!cb) return;
                        delete pendingCallbacks[requestId];
                        var response;
                        try { response = JSON.parse(responseJson); } catch (e) { response = responseJson; }
                        try { cb(response); } catch (e) {}
                    });

                    pending.forEach(function(fn) { fn(bridgeReady); });
                    pending = [];

                    if (isBackground) {
                        setTimeout(function() {
                            installListeners.forEach(function(fn) { try { fn({ reason: 'install' }); } catch (e) {} });
                            startupListeners.forEach(function(fn) { try { fn(); } catch (e) {} });
                        }, 0);
                    }
                });
            } else {
                setTimeout(tryInit, 20);
            }
        }
        tryInit();
    }

    var chromeObj = {
        runtime: {
            id: extId,
            getURL: function(path) {
                return 'nulla-extension://' + extId + '/' + String(path).replace(/^\//, '');
            },
            onMessage: {
                addListener: function(fn) { listeners.push(fn); }
            },
            onInstalled: {
                addListener: function(fn) { installListeners.push(fn); }
            },
            onStartup: {
                addListener: function(fn) { startupListeners.push(fn); }
            },
            sendMessage: function(message, callback) {
                withBridge(function(bridge) {
                    var requestId = 'r' + (++reqCounter) + '_' + Date.now();
                    if (typeof callback === 'function') {
                        pendingCallbacks[requestId] = callback;
                    }
                    bridge.sendMessage(extId, requestId, JSON.stringify({ senderContextId: contextId, payload: message }));
                });
            }
        },
        storage: {
            local: {
                get: function(keys, callback) {
                    withBridge(function(bridge) {
                        bridge.storageGetAllJson(extId, function(rawJson) {
                            var raw = {};
                            try { raw = JSON.parse(rawJson || '{}'); } catch (e) {}
                            var all = {};
                            Object.keys(raw).forEach(function(k) {
                                try { all[k] = JSON.parse(raw[k]); } catch (e) { all[k] = raw[k]; }
                            });
                            var result = {};
                            if (!keys) { result = all; }
                            else if (typeof keys === 'string') { result[keys] = all[keys]; }
                            else if (Array.isArray(keys)) { keys.forEach(function(k) { result[k] = all[k]; }); }
                            else if (typeof keys === 'object') {
                                Object.keys(keys).forEach(function(k) { result[k] = (k in all) ? all[k] : keys[k]; });
                            }
                            if (callback) callback(result);
                        });
                    });
                },
                set: function(items, callback) {
                    withBridge(function(bridge) {
                        Object.keys(items).forEach(function(k) {
                            bridge.storageSet(extId, k, JSON.stringify(items[k]));
                        });
                        if (callback) callback();
                    });
                },
                remove: function(keys, callback) {
                    withBridge(function(bridge) {
                        var list = Array.isArray(keys) ? keys : [keys];
                        list.forEach(function(k) { bridge.storageRemove(extId, k); });
                        if (callback) callback();
                    });
                }
            },
            session: {
                get: function(keys, callback) {
                    withBridge(function(bridge) {
                        bridge.sessionStorageGetAllJson(extId, function(rawJson) {
                            var raw = {};
                            try { raw = JSON.parse(rawJson || '{}'); } catch (e) {}
                            var all = {};
                            Object.keys(raw).forEach(function(k) {
                                try { all[k] = JSON.parse(raw[k]); } catch (e) { all[k] = raw[k]; }
                            });
                            var result = {};
                            if (!keys) { result = all; }
                            else if (typeof keys === 'string') { result[keys] = all[keys]; }
                            else if (Array.isArray(keys)) { keys.forEach(function(k) { result[k] = all[k]; }); }
                            else if (typeof keys === 'object') {
                                Object.keys(keys).forEach(function(k) { result[k] = (k in all) ? all[k] : keys[k]; });
                            }
                            if (callback) callback(result);
                        });
                    });
                },
                set: function(items, callback) {
                    withBridge(function(bridge) {
                        Object.keys(items).forEach(function(k) {
                            bridge.sessionStorageSet(extId, k, JSON.stringify(items[k]));
                        });
                        if (callback) callback();
                    });
                },
                remove: function(keys, callback) {
                    withBridge(function(bridge) {
                        var list = Array.isArray(keys) ? keys : [keys];
                        list.forEach(function(k) { bridge.sessionStorageRemove(extId, k); });
                        if (callback) callback();
                    });
                }
            }
        }
    };

    if (isBackground) {
        chromeObj.tabs = {
            query: function(queryInfo, callback) {
                withBridge(function(bridge) {
                    var pattern = (queryInfo && queryInfo.url) ? queryInfo.url : '<all_urls>';
                bridge.queryTabs(pattern, function(tabsJson) {
                    var tabs = [];
                    try { tabs = JSON.parse(tabsJson || '[]'); } catch (e) {}
                    if (callback) callback(tabs);
                });
                });
            }
        };

        chromeObj.scripting = {
            executeScript: function(details, callback) {
                withBridge(function(bridge) {
                    var tabId = (details && details.target) ? details.target.tabId : undefined;
                    var fileName = (details && details.files && details.files.length) ? details.files[0] : null;
                    if (tabId === undefined || !fileName) { if (callback) callback(); return; }
                    bridge.executeScriptInTab(tabId, extId, fileName, function() {
                        if (callback) callback();
                    });
                });
                return Promise.resolve();
            }
        };

        chromeObj.declarativeNetRequest = {
            updateDynamicRules: function() {
                console.warn('[NullA] chrome.declarativeNetRequest is not implemented natively yet; rule is ignored.');
                return Promise.resolve();
            }
        };
        chromeObj.webRequest = {
            onHeadersReceived: {
                addListener: function() {
                    console.warn('[NullA] chrome.webRequest.onHeadersReceived is not implemented natively yet; listener will never fire.');
                }
            }
        };
        chromeObj.cookies = {
            set: function() {
                return Promise.resolve();
            }
        };
    }

    window.chrome = chromeObj;
    window.browser = chromeObj;
})();
    )JS").arg(extIdEscaped, isBackground ? QStringLiteral("true") : QStringLiteral("false"));
}

void ExtensionManager::loadExtensionScripts(const QString &extId) {
    QString extPath = extensionsRoot() + "/" + extId;
    QFile manifestFile(extPath + "/manifest.json");

    if (!manifestFile.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
    QJsonObject json = doc.object();
    manifestFile.close();

    QStringList injectedNames;

    if (json.contains("content_scripts")) {
        QJsonArray scripts = json["content_scripts"].toArray();
        for (int i = 0; i < scripts.size(); ++i) {
            QJsonObject scriptObj = scripts[i].toObject();

            if (scriptObj.contains("js")) {
                QJsonArray jsFiles = scriptObj["js"].toArray();
                for (int j = 0; j < jsFiles.size(); ++j) {
                    QString jsFileName = jsFiles[j].toString();
                    QFile jsFile(extPath + "/" + jsFileName);

                    if (jsFile.open(QIODevice::ReadOnly)) {
                        QString jsCode = QString::fromUtf8(jsFile.readAll());
                        QString scriptName = extId + "_" + jsFileName;

                        QString combined = chromePolyfillFor(extId) + "\n;\n" + jsCode;

                        QWebEngineScript script;
                        script.setSourceCode(combined);
                        script.setName(scriptName);
                        script.setInjectionPoint(QWebEngineScript::DocumentReady);
                        script.setWorldId(QWebEngineScript::MainWorld);
                        script.setRunsOnSubFrames(true);

                        m_profile->scripts()->insert(script);
                        injectedNames << scriptName;
                        jsFile.close();
                    }
                }
            }
        }
    }

    m_extensionScriptNames[extId] = injectedNames;

    if (json.contains("background")) {
        loadExtensionBackground(extId, extPath, json);
    }
}

void ExtensionManager::loadExtensionBackground(const QString &extId, const QString &extPath, const QJsonObject &manifestJson) {
    QJsonObject bg = manifestJson.value("background").toObject();
    QString swFile = bg.value("service_worker").toString();
    if (swFile.isEmpty()) return;

    QFile jsFile(extPath + "/" + swFile);
    if (!jsFile.open(QIODevice::ReadOnly)) {
        #ifdef DEBUG_MODE
        qDebug() << "[Extensions] Background script not found for" << extId << ":" << swFile;
        #endif
        return;
    }
    QString jsCode = QString::fromUtf8(jsFile.readAll());
    jsFile.close();

    unloadExtensionBackground(extId);

    QWebEnginePage *bgPage = new QWebEnginePage(m_profile, this);
    bgPage->setWebChannel(ExtensionBridge::channel());

    QString combined = chromePolyfillFor(extId, /*isBackground=*/true) + "\n;\n" + jsCode;

    connect(bgPage, &QWebEnginePage::loadFinished, this, [bgPage, combined, extId](bool ok) {
        if (!ok) {
            #ifdef DEBUG_MODE
            qDebug() << "[Extensions] Background page failed to load for" << extId;
            #endif
            return;
        }
        bgPage->runJavaScript(combined);
    });

    bgPage->setHtml(QStringLiteral("<!DOCTYPE html><html><head><title>background:%1</title></head><body></body></html>").arg(extId));

    m_backgroundPages[extId] = bgPage;
    #ifdef DEBUG_MODE
    qDebug() << "[Extensions] Background page started for" << extId << "(" << swFile << ")";
    #endif
}

void ExtensionManager::unloadExtensionBackground(const QString &extId) {
    if (!m_backgroundPages.contains(extId)) return;
    QWebEnginePage *page = m_backgroundPages.take(extId);
    page->deleteLater();
}

void ExtensionManager::unloadExtensionScripts(const QString &extId) {
    if (m_extensionScriptNames.contains(extId)) {
        QWebEngineScriptCollection *collection = m_profile->scripts();
        const QStringList names = m_extensionScriptNames.value(extId);

        for (const QString &name : names) {
            const QList<QWebEngineScript> found = collection->find(name);
            for (const QWebEngineScript &s : found) {
                collection->remove(s);
            }
        }

        m_extensionScriptNames.remove(extId);
    }

    unloadExtensionBackground(extId);
}

void ExtensionManager::extractZip(const QString &zipPath, const QString &destDir) {
    QProcess process;
    #ifdef Q_OS_WIN
    QStringList arguments;
    arguments << "-Command" << QString("Expand-Archive -Path '%1' -DestinationPath '%2' -Force").arg(zipPath, destDir);
    process.start("powershell", arguments);
    #else
    QStringList arguments;
    arguments << "-o" << zipPath << "-d" << destDir;
    process.start("unzip", arguments);
    #endif
    process.waitForFinished(-1);
}

QString ExtensionManager::extensionsRoot() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/extensions";
}