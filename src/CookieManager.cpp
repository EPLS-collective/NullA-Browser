/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/CookieManager.h"

#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QTimer>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QUrl>
#include <QDebug>

CookieManager::CookieManager(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent), m_profile(profile) {

    auto *store = m_profile->cookieStore();

    // Debounce cookie persistence to avoid heavy disk I/O on burst updates
    m_cookieSaveTimer = new QTimer(this);
    m_cookieSaveTimer->setSingleShot(true);
    m_cookieSaveTimer->setInterval(2000);
    connect(m_cookieSaveTimer, &QTimer::timeout, this, &CookieManager::saveCookiesToJson);

    // In-memory cookie management and manual persistence
    connect(store, &QWebEngineCookieStore::cookieAdded, this, [this](const QNetworkCookie &cookie) {

        cookieCache.removeAll(cookie);
        cookieCache.append(cookie);

        #ifdef DEBUG_MODE
        qDebug() << "Saved cookie:" << cookie.name();
        #endif

        m_cookieSaveTimer->start();
    });

    connect(store, &QWebEngineCookieStore::cookieRemoved, this, [this](const QNetworkCookie &cookie) {

        cookieCache.removeAll(cookie);

        #ifdef DEBUG_MODE
        qDebug() << "Removed cookie:" << cookie.name();
        #endif

        m_cookieSaveTimer->start();
    });
}

void CookieManager::saveCookiesToJson() {
    QJsonArray array;

    for (const QNetworkCookie &cookie : cookieCache) {
        QJsonObject obj;

        obj["name"] = QString(cookie.name());
        obj["value"] = QString(cookie.value());
        obj["domain"] = cookie.domain();
        obj["path"] = cookie.path();
        obj["secure"] = cookie.isSecure();
        obj["httpOnly"] = cookie.isHttpOnly();
        obj["expiration"] = cookie.expirationDate().toSecsSinceEpoch();

        array.append(obj);
    }

    QJsonDocument doc(array);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                   + "/cookies.json";

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void CookieManager::loadCookies() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cookies.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonArray array = doc.array();
    auto *store = m_profile->cookieStore();

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();

        QNetworkCookie cookie;
        cookie.setName(obj["name"].toString().toUtf8());
        cookie.setValue(obj["value"].toString().toUtf8());
        cookie.setDomain(obj["domain"].toString());
        cookie.setPath(obj["path"].toString());
        cookie.setSecure(obj["secure"].toBool());
        cookie.setHttpOnly(obj["httpOnly"].toBool());

        qint64 exp = obj["expiration"].toVariant().toLongLong();
        if (exp > 0)
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(exp));

        QString domain = obj["domain"].toString();
        QString host = domain.startsWith('.') ? domain.mid(1) : domain;
        QString scheme = obj["secure"].toBool() ? "https://" : "http://";

        QString urlString = scheme + host;
        QUrl url(urlString);
        if (url.isValid()) {
            store->setCookie(cookie, url);
        } else {
            qWarning() << "Invalid URL for cookie:" << urlString;
        }
    }
}

void CookieManager::deleteCookie(const QString &domain, const QString &name) {
    for(int i = 0; i < cookieCache.size(); ++i) {
        if(cookieCache[i].domain() == domain && cookieCache[i].name() == name) {
            cookieCache.removeAt(i);
            break;
        }
    }

    QNetworkCookie dummy;
    dummy.setName(name.toUtf8());
    dummy.setDomain(domain);
    m_profile->cookieStore()->deleteCookie(dummy);

    saveCookiesToJson();
}

void CookieManager::flush() {
    if (m_cookieSaveTimer && m_cookieSaveTimer->isActive()) {
        m_cookieSaveTimer->stop();
        saveCookiesToJson();
    }
}