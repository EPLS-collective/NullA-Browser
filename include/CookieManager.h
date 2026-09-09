/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef COOKIEMANAGER_H
#define COOKIEMANAGER_H

#include <QObject>
#include <QList>
#include <QNetworkCookie>

class QWebEngineProfile;
class QTimer;

class CookieManager : public QObject {
    Q_OBJECT
public:
    explicit CookieManager(QWebEngineProfile *profile, QObject *parent = nullptr);

    // Restores stored cookies into the profile's cookie store at startup
    void loadCookies();

    // Removes one cookie from the cache and asks Qt to drop it site side
    void deleteCookie(const QString &domain, const QString &name);

    // Writes any pending (debounced) cache to disk; used before shutdown
    void flush();

private:
    void saveCookiesToJson();

    QWebEngineProfile *m_profile = nullptr;
    QList<QNetworkCookie> cookieCache;
    QTimer *m_cookieSaveTimer = nullptr;
};

#endif // COOKIEMANAGER_H
