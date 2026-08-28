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

void Interceptor::addBlockedDomain(const QString &domain, const std::optional<FilterRule> &rule) {
    QMutexLocker locker(&mutex);
    std::u16string key = domain.toLower().trimmed().toStdU16String();
    if (!rule.has_value() || rule->isTrivial()) {
        blockedDomains.insert(key);
    } else {
        restrictedBlockedDomains[key].push_back(*rule);
    }
}

void Interceptor::addBlockedPattern(const QString &pattern, const std::optional<FilterRule> &rule) {
    QMutexLocker locker(&mutex);
    std::u16string key = pattern.toLower().trimmed().toStdU16String();
    if (!rule.has_value() || rule->isTrivial()) {
        blockedPatterns.push_back(key);
    } else {
        restrictedBlockedPatterns.push_back({key, *rule});
    }
}

void Interceptor::addAllowedDomain(const QString &domain, const std::optional<FilterRule> &rule) {
    QMutexLocker locker(&mutex);
    std::u16string key = domain.toLower().trimmed().toStdU16String();
    if (!rule.has_value() || rule->isTrivial()) {
        allowedDomains.insert(key);
    } else {
        restrictedAllowedDomains[key].push_back(*rule);
    }
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

bool Interceptor::domainMatchesAny(const QString &host, const std::vector<std::u16string> &list) {
    if (host.isEmpty() || list.empty()) return false;
    QString lowerHost = host.toLower();
    std::u16string_view hostView(reinterpret_cast<const char16_t*>(lowerHost.utf16()), lowerHost.size());

    auto contains = [&](std::u16string_view v) {
        for (const auto &d : list) {
            if (v == std::u16string_view(d)) return true;
        }
        return false;
    };

    if (contains(hostView)) return true;
    size_t index = 0;
    while ((index = hostView.find(u'.', index)) != std::u16string_view::npos) {
        index++;
        if (contains(hostView.substr(index))) return true;
    }
    return false;
}

bool Interceptor::restrictedDomainMatch(const std::unordered_map<std::u16string, std::vector<FilterRule>> &map,
                                         const QString &host, uint32_t category, bool thirdParty,
                                         const QString &firstPartyHost) const {
    if (host.isEmpty()) return false;
    QString lowerHost = host.toLower();
    std::u16string_view hostView(reinterpret_cast<const char16_t*>(lowerHost.utf16()), lowerHost.size());

    auto checkAt = [&](std::u16string_view suffix) -> bool {
        auto it = map.find(std::u16string(suffix));
        if (it == map.end()) return false;
        for (const auto &rule : it->second) {
            if (!rule.matchesResource(category)) continue;
            if (!rule.matchesThirdParty(thirdParty)) continue;
            if (!rule.domainExcludes.empty() && domainMatchesAny(firstPartyHost, rule.domainExcludes)) continue;
            if (!rule.domainIncludes.empty() && !domainMatchesAny(firstPartyHost, rule.domainIncludes)) continue;
            return true;
        }
        return false;
    };

    // caller must hold "mutex"
    if (checkAt(hostView)) return true;
    size_t index = 0;
    while ((index = hostView.find(u'.', index)) != std::u16string_view::npos) {
        index++;
        if (checkAt(hostView.substr(index))) return true;
    }
    return false;
}

bool Interceptor::restrictedPatternMatch(const std::vector<PatternRule> &patterns,
                                          std::u16string_view combinedView, uint32_t category, bool thirdParty,
                                          const QString &firstPartyHost) const {
    // caller must hold "mutex"
    for (const auto &pr : patterns) {
        if (!pr.rule.matchesResource(category)) continue;
        if (!pr.rule.matchesThirdParty(thirdParty)) continue;
        if (!pr.rule.domainExcludes.empty() && domainMatchesAny(firstPartyHost, pr.rule.domainExcludes)) continue;
        if (!pr.rule.domainIncludes.empty() && !domainMatchesAny(firstPartyHost, pr.rule.domainIncludes)) continue;

        bool hit = pr.pattern.find(u'*') != std::u16string::npos
            ? wildcardMatch(combinedView, std::u16string_view(pr.pattern))
            : combinedView.find(std::u16string_view(pr.pattern)) != std::u16string_view::npos;
        if (hit) return true;
    }
    return false;
}

uint32_t Interceptor::categoryForResourceType(int resourceType) {
    using RT = QWebEngineUrlRequestInfo::ResourceType;
    switch (static_cast<RT>(resourceType)) {
        case RT::ResourceTypeStylesheet:     return ResCatStylesheet;
        case RT::ResourceTypeScript:         return ResCatScript;
        case RT::ResourceTypeImage:          return ResCatImage;
        case RT::ResourceTypeFontResource:   return ResCatFont;
        case RT::ResourceTypeSubFrame:       return ResCatSubdocument; // iframes
        case RT::ResourceTypeObject:         return ResCatObject;
        case RT::ResourceTypePluginResource: return ResCatObject;
        case RT::ResourceTypeMedia:          return ResCatMedia;
        case RT::ResourceTypeXhr:            return ResCatXHR;
        case RT::ResourceTypePing:           return ResCatPing;
        case RT::ResourceTypeFavicon:        return ResCatImage;
        default:                             return ResCatOther; // no WS type on all Qt versions, safer to not guess
    }
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

    const bool thirdParty = true;
    const uint32_t category = categoryForResourceType(static_cast<int>(info.resourceType()));

    {
        QMutexLocker locker(&mutex);

        // allow rules only win for the resource type they declare
        if (allowedDomains.find(host.toLower().toStdU16String()) != allowedDomains.end())
            return;
        if (restrictedDomainMatch(restrictedAllowedDomains, host, category, thirdParty, firstPartyHost))
            return;
    }
    if (isAllowed(host)) return;

    bool blocked = false;
    {
        QMutexLocker locker(&mutex);
        blocked = restrictedDomainMatch(restrictedBlockedDomains, host, category, thirdParty, firstPartyHost);
        if (!blocked) {
            QString combined = (host + requestUrl.path()).toLower();
            std::u16string_view view(reinterpret_cast<const char16_t*>(combined.utf16()), combined.size());
            blocked = restrictedPatternMatch(restrictedBlockedPatterns, view, category, thirdParty, firstPartyHost);
        }
    }
    blocked = blocked || isBlocked(host) || isBlockedPath(host, requestUrl.path());

    if (blocked) {
        info.block(true);

        #ifdef DEBUG_MODE
        qDebug() << "Blocked:"
        << host
        << "Type:" << info.resourceType()
        << "Category:" << category;
        #endif
    }
}
