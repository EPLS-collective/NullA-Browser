/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/Interceptor.h"
#include <QDebug>

Interceptor::Interceptor(QObject* parent)
: QWebEngineUrlRequestInterceptor(parent)
{
}

void Interceptor::addBlockedDomain(const QString &domain) {
    QMutexLocker locker(&mutex);
    blockedDomains.insert(domain.toLower().trimmed().toStdU16String());
}

bool Interceptor::isBlocked(const QString &host) const {
    // Performs a recursive domain check. It doesn't just check the full host,
    // but also iterates through subdomains to ensure nested domains (e.g., sub.example.com)
    // are correctly matched against the blocked list.

    if (host.isEmpty()) return false;
    QString lowerHost = host.toLower();

    std::u16string_view hostView(reinterpret_cast<const char16_t*>(lowerHost.utf16()), lowerHost.size());

    if (blockedDomains.find(std::u16string(hostView)) != blockedDomains.end()) return true;

    size_t index = 0;
    while ((index = hostView.find(u'.', index)) != std::u16string_view::npos) {
        index++;
        std::u16string_view subView = hostView.substr(index);

        if (blockedDomains.find(std::u16string(subView)) != blockedDomains.end()) {
            return true;
        }
    }
    return false;
}

void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame)
        return;

    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeServiceWorker ||
        info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeWorker ||
        info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeSharedWorker) {
        info.block(true);
        return;
    }

    QUrl requestUrl = info.requestUrl();
    QString host = requestUrl.host();
    if (host.isEmpty()) return;

    if (host == info.firstPartyUrl().host()) {
        return;
    }

    if (isBlocked(host)) {
        info.block(true);
        // qDebug() << "Blocked:" << host; // For Debug
    }
}
