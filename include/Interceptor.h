/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef INTERCEPTOR_H
#define INTERCEPTOR_H

#include <QWebEngineUrlRequestInterceptor>
#include <QString>
#include <QUrl>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>
#include <QMutex>
#include <QSet>
#include <QHash>

// bitmask for $script/$subdocument/etc, mapped from ResourceType
enum ResourceCategory : uint32_t {
    ResCatOther       = 1u << 0,
    ResCatScript      = 1u << 1,
    ResCatImage       = 1u << 2,
    ResCatStylesheet  = 1u << 3,
    ResCatObject      = 1u << 4,
    ResCatXHR         = 1u << 5,
    ResCatSubdocument = 1u << 6, // iframes
    ResCatFont        = 1u << 7,
    ResCatMedia       = 1u << 8,
    ResCatWebSocket   = 1u << 9,
    ResCatPing        = 1u << 10,
    ResCatPopup       = 1u << 11,
};

enum HttpMethod : uint16_t {
    MethodGet     = 1u << 0,
    MethodPost    = 1u << 1,
    MethodPut     = 1u << 2,
    MethodDelete  = 1u << 3,
    MethodHead    = 1u << 4,
    MethodOptions = 1u << 5,
    MethodPatch   = 1u << 6,
    MethodConnect = 1u << 7,
    MethodTrace   = 1u << 8,
};

// a rule's $options. default = matches everything (plain "||domain^")
struct FilterRule {
    uint32_t includeMask = 0; // resource types this applies to (0 = all)
    uint32_t excludeMask = 0; // resource types this does NOT apply to
    int8_t thirdParty = 0;    // 0 = no restriction, 1 = 3p only, -1 = 1p only
    bool important = false;
    std::vector<std::u16string> domainIncludes; // $domain=a.com|b.com
    std::vector<std::u16string> domainExcludes; // $domain=~a.com
    uint16_t methodIncludeMask = 0; // $method=get|post
    uint16_t methodExcludeMask = 0; // $method=~post

    bool matchesResource(uint32_t category) const {
        if (includeMask != 0) return (includeMask & category) != 0;
        if (excludeMask != 0) return (excludeMask & category) == 0;
        return true;
    }
    bool matchesThirdParty(bool isThirdParty) const {
        if (thirdParty == 1) return isThirdParty;
        if (thirdParty == -1) return !isThirdParty;
        return true;
    }
    bool matchesMethod(uint16_t method) const {
        if (methodIncludeMask != 0) return (methodIncludeMask & method) != 0;
        if (methodExcludeMask != 0) return (methodExcludeMask & method) == 0;
        return true;
    }
    bool isTrivial() const {
        return includeMask == 0 && excludeMask == 0 && thirdParty == 0
        && domainIncludes.empty() && domainExcludes.empty()
        && methodIncludeMask == 0 && methodExcludeMask == 0
        && !important;
    }
};

class Interceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit Interceptor(QObject* parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;

    // no rule = unconditional match (old behavior). with rule = only
    // matches when resource type / 3p / domain= conditions hold.
    void addBlockedDomain(const QString &domain, const std::optional<FilterRule> &rule = std::nullopt);
    void addBlockedPattern(const QString &pattern, const std::optional<FilterRule> &rule = std::nullopt);
    void addAllowedDomain(const QString &domain, const std::optional<FilterRule> &rule = std::nullopt);
    void addAllowedDomain(const QString &domain, const QString &path);

    // plain lookups, no options - old API
    bool isBlocked(const QString &host) const;
    bool isAllowed(const QString &host, const QString &path) const;
    bool isBlockedPath(const QString &host, const QString &path) const;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    static uint32_t categoryForResourceType(int resourceType);
    static uint16_t methodForString(const QByteArray &method);
    static void loadPublicSuffixData(const QByteArray &data);

    // Cosmetic (CSS element-hiding) filters: "domain1,domain2##selector"
    // (hide) or "domain1,domain2#@#selector" (unhide exception).
    // domains.isEmpty() == generic rule, applies on every site.
    void addCosmeticRule(const QStringList &domains, const QString &selector, bool isException);

    // Domain-specific + generic selectors combined for this host, minus
    // exceptions, ready to drop straight into a <style> tag. Empty if none.
    QString cosmeticCssFor(const QString &host) const;

    // Only the generic (non domain-scoped) selectors - safe to inject once
    // for the whole profile since it doesn't depend on which page loads.
    QString genericCosmeticCss() const;

private:
    struct PatternRule {
        std::u16string pattern;
        FilterRule rule;
    };

    bool restrictedDomainMatch(const std::unordered_map<std::u16string, std::vector<FilterRule>> &map,
                                const QString &host, uint32_t category, bool thirdParty, uint16_t method,
                                const QString &firstPartyHost) const;
    bool hasImportantMatch(std::u16string_view hostView, std::u16string_view combinedView,
                           uint32_t category, bool thirdParty, uint16_t method,
                           const QString &firstPartyHost) const;
    bool matchBlockingPatterns(std::u16string_view combinedView, uint32_t category, bool thirdParty,
                               uint16_t method, const QString &firstPartyHost, bool onlyImportant) const;
    bool isAllowedInternal(std::u16string_view hostView, const QString &lowerPath, const QString &lowerHost,
                           uint32_t category, bool thirdParty, uint16_t method,
                           const QString &firstPartyHost) const;

    static bool domainMatchesAny(const QString &host, const std::vector<std::u16string> &list);
    static QString registrableDomain(const QString &host);
    static bool checkDomainSet(const std::unordered_set<std::u16string> &set, std::u16string_view hostView);
    static std::u16string firstToken(std::u16string_view pattern);
    static void collectUrlTokens(std::u16string_view combined, std::vector<std::u16string> &out);

    std::unordered_set<std::u16string> blockedDomains;
    std::vector<std::u16string> blockedPatterns;
    std::unordered_set<std::u16string> allowedDomains;
    std::unordered_map<std::u16string, std::vector<std::u16string>> allowedDomainPaths;

    // $-option rules, need request context to evaluate
    std::unordered_map<std::u16string, std::vector<FilterRule>> restrictedBlockedDomains;
    std::vector<PatternRule> restrictedBlockedPatterns;
    std::unordered_map<std::u16string, std::vector<FilterRule>> restrictedAllowedDomains;

    // token-bucket index over blockedPatterns / restrictedBlockedPatterns so a
    // request only probes patterns sharing one of its own host/path tokens
    // instead of scanning the whole rule list.
    std::unordered_map<std::u16string, std::vector<uint32_t>> m_patternIndex;
    std::unordered_map<std::u16string, std::vector<uint32_t>> m_restrictedPatternIndex;

    static bool wildcardMatch(std::u16string_view text, std::u16string_view pattern);
    mutable QMutex mutex;
    bool m_enabled = true;

    static inline QSet<QString> s_pslRules;
    static inline QSet<QString> s_pslExceptions;
    static inline QMutex s_pslMutex;
    static inline bool s_pslLoaded = false;
    static inline QHash<QString, QString> s_regCache;
    static inline QMutex s_regCacheMutex;

    static bool isSafeCosmeticSelector(const QString &selector);

    std::unordered_map<std::u16string, std::vector<std::u16string>> cosmeticSelectorsByDomain;
    std::unordered_map<std::u16string, std::unordered_set<std::u16string>> cosmeticExceptionsByDomain;
    std::vector<std::u16string> cosmeticGenericSelectors;
    std::unordered_set<std::u16string> cosmeticGenericExceptions;
    mutable QMutex cosmeticMutex;
};

#endif // INTERCEPTOR_H
