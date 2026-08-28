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

// a rule's $options. default = matches everything (plain "||domain^")
struct FilterRule {
    uint32_t includeMask = 0; // resource types this applies to (0 = all)
    uint32_t excludeMask = 0; // resource types this does NOT apply to
    int8_t thirdParty = 0;    // 0 = no restriction, 1 = 3p only, -1 = 1p only
    bool important = false;
    std::vector<std::u16string> domainIncludes; // $domain=a.com|b.com
    std::vector<std::u16string> domainExcludes; // $domain=~a.com

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
    bool isTrivial() const {
        return includeMask == 0 && excludeMask == 0 && thirdParty == 0
            && domainIncludes.empty() && domainExcludes.empty();
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

    // plain lookups, no options - old API
    bool isBlocked(const QString &host) const;
    bool isAllowed(const QString &host) const;
    bool isBlockedPath(const QString &host, const QString &path) const;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

    static uint32_t categoryForResourceType(int resourceType);

private:
    struct PatternRule {
        std::u16string pattern;
        FilterRule rule;
    };

    bool restrictedDomainMatch(const std::unordered_map<std::u16string, std::vector<FilterRule>> &map,
                                const QString &host, uint32_t category, bool thirdParty,
                                const QString &firstPartyHost) const;
    bool restrictedPatternMatch(const std::vector<PatternRule> &patterns,
                                 std::u16string_view combinedView, uint32_t category, bool thirdParty,
                                 const QString &firstPartyHost) const;
    static bool domainMatchesAny(const QString &host, const std::vector<std::u16string> &list);

    std::unordered_set<std::u16string> blockedDomains;
    std::vector<std::u16string> blockedPatterns;
    std::unordered_set<std::u16string> allowedDomains;

    // $-option rules, need request context to evaluate
    std::unordered_map<std::u16string, std::vector<FilterRule>> restrictedBlockedDomains;
    std::vector<PatternRule> restrictedBlockedPatterns;
    std::unordered_map<std::u16string, std::vector<FilterRule>> restrictedAllowedDomains;

    static bool wildcardMatch(std::u16string_view text, std::u16string_view pattern);
    mutable QMutex mutex;
    bool m_enabled = true;
};

#endif // INTERCEPTOR_H
