/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/Interceptor.h"
#include <QDebug>
#include <QHostAddress>
#include <QByteArray>
#include <QSet>

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

void Interceptor::addAllowedDomain(const QString &domain, const QString &path) {
    QMutexLocker locker(&mutex);

    std::u16string domainKey = domain.toLower().trimmed().toStdU16String();
    std::u16string pathKey = path.toLower().trimmed().toStdU16String();

    if (pathKey.empty()) {
        allowedDomains.insert(domainKey);
    } else {
        allowedDomainPaths[domainKey].push_back(pathKey);
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

bool Interceptor::isAllowed(const QString &host, const QString &path) const {
    if (host.isEmpty())
        return false;

    const QString lowerHost = host.toLower();
    const QString lowerPath = path.toLower();

    std::u16string_view hostView(
        reinterpret_cast<const char16_t*>(lowerHost.utf16()),
                                 lowerHost.size()
    );

    QMutexLocker locker(&mutex);

    auto checkDomain = [&](std::u16string_view domain) -> bool {
        const std::u16string domainKey(domain);

        // Normal domain allow
        if (allowedDomains.find(domainKey) != allowedDomains.end())
            return true;

        // Domain + path allow
        auto it = allowedDomainPaths.find(domainKey);
        if (it == allowedDomainPaths.end())
            return false;

        for (const auto &allowedPath : it->second) {
            if (lowerPath == QString::fromStdU16String(allowedPath))
                return true;
        }

        return false;
    };

    if (checkDomain(hostView))
        return true;

    size_t index = 0;

    while ((index = hostView.find(u'.', index)) != std::u16string_view::npos) {
        index++;

        if (checkDomain(hostView.substr(index)))
            return true;
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

void Interceptor::loadPublicSuffixData(const QByteArray &data) {
    QMutexLocker locker(&s_pslMutex);
    s_pslRules.clear();
    s_pslExceptions.clear();

    const QList<QByteArray> lines = data.split('\n');
    for (QByteArray line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith("//")) continue;

            if (line.startsWith('!')) {
                s_pslExceptions.insert(QString::fromUtf8(line.mid(1)).toLower());
            } else {
                s_pslRules.insert(QString::fromUtf8(line).toLower());
            }
    }
    s_pslLoaded = true;
}

QString Interceptor::registrableDomain(const QString &host) {
    if (host.isEmpty()) return host;
    const QString lower = host.toLower();
    {
        QHostAddress addr;
        if (addr.setAddress(lower)) return lower;
    }

    QMutexLocker locker(&s_pslMutex);

    // If PSL hasn't downloaded/filled yet, see the old 2-tagged fallback[cite: 7]
    if (!s_pslLoaded) {
        const QStringList labels = lower.split(QLatin1Char('.'), Qt::SkipEmptyParts);
        if (labels.size() <= 2) return lower;
        return labels.mid(labels.size() - 2).join(QLatin1Char('.'));
    }

    QStringList parts = lower.split('.', Qt::SkipEmptyParts);
    QString suffix;

    for (int i = 0; i < parts.size(); ++i) {
        QString sub = parts.mid(i).join('.');
        if (s_pslExceptions.contains(sub)) {
            suffix = parts.mid(i + 1).join('.');
            break;
        }
    }

    if (suffix.isEmpty()) {
        for (int i = 0; i < parts.size(); ++i) {
            QString sub = parts.mid(i).join('.');
            if (s_pslRules.contains(sub)) {
                suffix = sub;
                break;
            }
            if (i > 0) {
                QString wildcardSub = "*." + parts.mid(i).join('.');
                if (s_pslRules.contains(wildcardSub)) {
                    suffix = parts.mid(i - 1).join('.');
                    break;
                }
            }
        }
    }

    if (suffix.isEmpty()) suffix = parts.last();
    if (suffix == lower) return lower;

    // Finding eTLD+1 (Include the tag preceding the suffix)
    QString prefix = lower.left(lower.length() - suffix.length() - 1);
    QStringList prefixLabels = prefix.split('.', Qt::SkipEmptyParts);
    if (prefixLabels.isEmpty()) return lower;

    return prefixLabels.last() + QLatin1Char('.') + suffix;
}

bool Interceptor::restrictedDomainMatch(const std::unordered_map<std::u16string, std::vector<FilterRule>> &map,
                                         const QString &host, uint32_t category, bool thirdParty, uint16_t method,
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
            if (!rule.matchesMethod(method)) continue;
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
                                          std::u16string_view combinedView, uint32_t category, bool thirdParty, uint16_t method,
                                          const QString &firstPartyHost) const {
    // caller must hold "mutex"
    for (const auto &pr : patterns) {
        if (!pr.rule.matchesResource(category)) continue;
        if (!pr.rule.matchesThirdParty(thirdParty)) continue;
        if (!pr.rule.matchesMethod(method)) continue;
        if (!pr.rule.domainExcludes.empty() && domainMatchesAny(firstPartyHost, pr.rule.domainExcludes)) continue;
        if (!pr.rule.domainIncludes.empty() && !domainMatchesAny(firstPartyHost, pr.rule.domainIncludes)) continue;

        bool hit = pr.pattern.find(u'*') != std::u16string::npos
            ? wildcardMatch(combinedView, std::u16string_view(pr.pattern))
            : combinedView.find(std::u16string_view(pr.pattern)) != std::u16string_view::npos;
        if (hit) return true;
    }
    return false;
}

bool Interceptor::hasImportantBlockMatch(const QString &host, uint32_t category, bool thirdParty, uint16_t method,
                                          const QString &firstPartyHost, const QString &path) const {
    // caller must hold "mutex"
    QString lowerHost = host.toLower();
    std::u16string_view hostView(reinterpret_cast<const char16_t*>(lowerHost.utf16()), lowerHost.size());

    auto matches = [&](const FilterRule &rule) -> bool {
        if (!rule.important) return false;
        if (!rule.matchesResource(category)) return false;
        if (!rule.matchesThirdParty(thirdParty)) return false;
        if (!rule.matchesMethod(method)) return false;
        if (!rule.domainExcludes.empty() && domainMatchesAny(firstPartyHost, rule.domainExcludes)) return false;
        if (!rule.domainIncludes.empty() && !domainMatchesAny(firstPartyHost, rule.domainIncludes)) return false;
        return true;
    };

    auto checkAt = [&](std::u16string_view suffix) -> bool {
        auto it = restrictedBlockedDomains.find(std::u16string(suffix));
        if (it == restrictedBlockedDomains.end()) return false;
        for (const auto &rule : it->second) {
            if (matches(rule)) return true;
        }
        return false;
    };

    if (checkAt(hostView)) return true;
    size_t index = 0;
    while ((index = hostView.find(u'.', index)) != std::u16string_view::npos) {
        index++;
        if (checkAt(hostView.substr(index))) return true;
    }

    QString combined = (host + path).toLower();
    std::u16string_view view(reinterpret_cast<const char16_t*>(combined.utf16()), combined.size());
    for (const auto &pr : restrictedBlockedPatterns) {
        if (!matches(pr.rule)) continue;
        bool hit = pr.pattern.find(u'*') != std::u16string::npos
            ? wildcardMatch(view, std::u16string_view(pr.pattern))
            : view.find(std::u16string_view(pr.pattern)) != std::u16string_view::npos;
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

uint16_t Interceptor::methodForString(const QByteArray &method) {
    const QByteArray m = method.toUpper();
    if (m == "GET")     return MethodGet;
    if (m == "POST")    return MethodPost;
    if (m == "PUT")     return MethodPut;
    if (m == "DELETE")  return MethodDelete;
    if (m == "HEAD")    return MethodHead;
    if (m == "OPTIONS") return MethodOptions;
    if (m == "PATCH")   return MethodPatch;
    if (m == "CONNECT") return MethodConnect;
    if (m == "TRACE")   return MethodTrace;
    return 0;
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

bool Interceptor::isSafeCosmeticSelector(const QString &selector) {
    if (selector.isEmpty() || selector.size() > 512) return false;

    // uBlock/AdGuard "procedural" cosmetic operators aren't plain CSS and we
    // can't safely evaluate them via querySelectorAll/<style>. Drop instead
    // of misinterpreting them.
    static const QStringList kUnsafeMarkers = {
        ":has-text(", ":matches-css(", ":xpath(", ":upward(", ":remove(",
        ":style(", ":matches-attr(", ":matches-path(", "+js(", ":contains(",
        ":min-text-length(", ":others("
    };
    for (const QString &m : kUnsafeMarkers)
        if (selector.contains(m, Qt::CaseInsensitive)) return false;

        // Never allow characters that could break out of the JS template literal
        // this gets embedded into later, a compromised/malicious filter list is
        // otherwise a script injection vector via a backtick in a "selector".
        for (const QChar &c : selector)
            if (c == QChar('`') || c == QChar('<') || c == QChar('>')) return false;

            return true;
}

void Interceptor::addCosmeticRule(const QStringList &domains, const QString &selector, bool isException) {
    if (!isSafeCosmeticSelector(selector)) return;
    const std::u16string sel = selector.trimmed().toStdU16String();
    if (sel.empty()) return;

    QMutexLocker locker(&cosmeticMutex);

    if (domains.isEmpty()) {
        if (isException) cosmeticGenericExceptions.insert(sel);
        else cosmeticGenericSelectors.push_back(sel);
        return;
    }

    for (const QString &d : domains) {
        QString domain = d.trimmed().toLower();
        const bool negated = domain.startsWith('~');
        if (negated) domain = domain.mid(1);
        if (domain.isEmpty()) continue;

        const std::u16string key = domain.toStdU16String();
        // "~domain##sel" means "hide everywhere except this domain",
        // that's an exception scoped to this domain.
        if (isException || negated) cosmeticExceptionsByDomain[key].insert(sel);
        else cosmeticSelectorsByDomain[key].push_back(sel);
    }
}

QString Interceptor::genericCosmeticCss() const {
    QMutexLocker locker(&cosmeticMutex);
    if (cosmeticGenericSelectors.empty()) return QString();

    QStringList selectors;
    for (const auto &s : cosmeticGenericSelectors) {
        if (cosmeticGenericExceptions.count(s)) continue;
        selectors << QString::fromStdU16String(s);
    }
    if (selectors.isEmpty()) return QString();

    // :where(...) is a *forgiving* selector list, one bad/unsupported
    // selector in the batch is skipped instead of invalidating the whole
    // rule, and it carries zero specificity so it can't fight page CSS.
    return QStringLiteral(":where(\n%1\n) { display: none !important; }").arg(selectors.join(",\n"));
}

QString Interceptor::cosmeticCssFor(const QString &host) const {
    if (host.isEmpty()) return QString();
    const QString lowerHost = host.toLower();
    std::u16string_view hostView(reinterpret_cast<const char16_t*>(lowerHost.utf16()), lowerHost.size());

    QMutexLocker locker(&cosmeticMutex);

    QSet<QString> exceptions;
    auto collectExceptionsAt = [&](std::u16string_view suffix) {
        auto it = cosmeticExceptionsByDomain.find(std::u16string(suffix));
        if (it == cosmeticExceptionsByDomain.end()) return;
        for (const auto &s : it->second) exceptions.insert(QString::fromStdU16String(s));
    };
        collectExceptionsAt(hostView);
        for (size_t i = 0; (i = hostView.find(u'.', i)) != std::u16string_view::npos; )
            collectExceptionsAt(hostView.substr(++i));
    for (const auto &s : cosmeticGenericExceptions) exceptions.insert(QString::fromStdU16String(s));

    QSet<QString> selectors;
    auto collectAt = [&](std::u16string_view suffix) {
        auto it = cosmeticSelectorsByDomain.find(std::u16string(suffix));
        if (it == cosmeticSelectorsByDomain.end()) return;
        for (const auto &s : it->second) {
            QString sel = QString::fromStdU16String(s);
            if (!exceptions.contains(sel)) selectors.insert(sel);
        }
    };
    collectAt(hostView);
    for (size_t i = 0; (i = hostView.find(u'.', i)) != std::u16string_view::npos; )
        collectAt(hostView.substr(++i));
    for (const auto &s : cosmeticGenericSelectors) {
        QString sel = QString::fromStdU16String(s);
        if (!exceptions.contains(sel)) selectors.insert(sel);
    }

    if (selectors.isEmpty()) return QString();
    return QStringLiteral(":where(\n%1\n) { display: none !important; }")
    .arg(QStringList(selectors.begin(), selectors.end()).join(",\n"));
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

    const bool thirdParty = (registrableDomain(host) != registrableDomain(firstPartyHost));
    const uint32_t category = categoryForResourceType(static_cast<int>(info.resourceType()));
    const uint16_t method = methodForString(info.requestMethod());

    {
        QMutexLocker locker(&mutex);

        // $important block rules always win, even over an allow rule.
        if (hasImportantBlockMatch(host, category, thirdParty, method, firstPartyHost, requestUrl.path())) {
            info.block(true);
            #ifdef DEBUG_MODE
            qDebug() << "Blocked (important):" << host << requestUrl.path();
            #endif
            return;
        }

        // allow rules only win for the resource type they declare
        if (allowedDomains.find(host.toLower().toStdU16String()) != allowedDomains.end())
            return;
        if (restrictedDomainMatch(restrictedAllowedDomains, host, category, thirdParty, method, firstPartyHost))
            return;
    }
    if (isAllowed(host, requestUrl.path())) return;

    bool blocked = false;
    {
        QMutexLocker locker(&mutex);
        blocked = restrictedDomainMatch(restrictedBlockedDomains, host, category, thirdParty, method, firstPartyHost);
        if (!blocked) {
            QString combined = (host + requestUrl.path()).toLower();
            std::u16string_view view(reinterpret_cast<const char16_t*>(combined.utf16()), combined.size());
            blocked = restrictedPatternMatch(restrictedBlockedPatterns, view, category, thirdParty, method, firstPartyHost);
        }
    }
    blocked = blocked || isBlocked(host) || isBlockedPath(host, requestUrl.path());

    if (blocked) {
        info.block(true);

        #ifdef DEBUG_MODE
        qDebug()
        << "Blocked:" << host << requestUrl.path()
        << "Type:" << info.resourceType()
        << "Category:" << category
        << "3rdParty:" << thirdParty
        << "Method:" << info.requestMethod();
        #endif
    }
}
