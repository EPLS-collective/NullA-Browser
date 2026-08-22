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

void Interceptor::addBlockedPattern(const QString &pattern) {
    QMutexLocker locker(&mutex);
    blockedPatterns.push_back(pattern.toLower().trimmed().toStdU16String());
}

void Interceptor::addAllowedDomain(const QString &domain) {
    QMutexLocker locker(&mutex);
    allowedDomains.insert(domain.toLower().trimmed().toStdU16String());
}

bool Interceptor::isBlocked(const QString &host) const {
    // Performs a recursive domain check. It doesn't just check the full host,
    // but also iterates through subdomains to ensure nested domains (e.g., sub.example.com)
    // are correctly matched against the blocked list.

    if (host.isEmpty()) return false;
    QString lowerHost = host.toLower();

    std::u16string_view hostView(reinterpret_cast<const char16_t*>(lowerHost.utf16()), lowerHost.size());

    QMutexLocker locker(&mutex);

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

bool Interceptor::isAllowed(const QString &host) const {
    QString lowerHost = host.toLower();
    std::u16string_view hostView(reinterpret_cast<const char16_t*>(lowerHost.utf16()), lowerHost.size());

    QMutexLocker locker(&mutex);
    if (allowedDomains.find(std::u16string(hostView)) != allowedDomains.end()) return true;

    size_t index = 0;
    while ((index = hostView.find(u'.', index)) != std::u16string_view::npos) {
        index++;
        std::u16string_view subView = hostView.substr(index);
        if (allowedDomains.find(std::u16string(subView)) != allowedDomains.end()) return true;
    }
    return false;
}

bool Interceptor::isBlockedPath(const QString &host, const QString &path) const {
    QString combined = (host + path).toLower();
    std::u16string_view view(reinterpret_cast<const char16_t*>(combined.utf16()), combined.size());

    QMutexLocker locker(&mutex);
    for (const auto &pattern : blockedPatterns) {
        bool hit = pattern.find(u'*') != std::u16string::npos
        ? wildcardMatch(view, std::u16string_view(pattern))
        : view.find(std::u16string_view(pattern)) != std::u16string_view::npos;
        if (hit) return true;
    }
    return false;
}

bool Interceptor::wildcardMatch(std::u16string_view text, std::u16string_view pattern) {
    size_t t = 0, p = 0, starIdx = std::u16string_view::npos, match = 0;
    while (t < text.size()) {
        if (p < pattern.size() && pattern[p] == text[t]) { t++; p++; }
        else if (p < pattern.size() && pattern[p] == u'*') { starIdx = p++; match = t; }
        else if (starIdx != std::u16string_view::npos) { p = starIdx + 1; t = ++match; }
        else return false;
    }
    while (p < pattern.size() && pattern[p] == u'*') p++;
    return p == pattern.size();
}

void Interceptor::interceptRequest(QWebEngineUrlRequestInfo &info) {

    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame)
        return;

    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeServiceWorker ||
        info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeWorker ||
        info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeSharedWorker) {
        info.block(true);
        return;
    }

    if (!m_enabled) return;

    QUrl requestUrl = info.requestUrl();
    QString host = requestUrl.host();
    if (host.isEmpty()) return;

    QString firstPartyHost = info.firstPartyUrl().host();
    if (host == firstPartyHost) {
        return;
    }

    if (isAllowed(host)) {
        return;
    }

    if (isBlocked(host) || isBlockedPath(host, requestUrl.path())) {
        info.block(true);
        #ifdef DEBUG_MODE
        qDebug() << "Blocked:" << host;
        #endif
    }
}
