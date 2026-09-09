/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/AdBlock.h"
#include "../include/Interceptor.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QStandardPaths>
#include <QThreadPool>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <QVector>
#include <QPair>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>
#include <optional>
#include <chrono>

AdBlock::AdBlock(QWebEngineProfile *profile, QObject *parent)
    : QObject(parent), m_profile(profile), m_enabled(true) {
    m_interceptor = new Interceptor(m_profile);
    m_profile->setUrlRequestInterceptor(m_interceptor);
}

Interceptor *AdBlock::interceptor() const {
    return m_interceptor;
}

bool AdBlock::isEnabled() const {
    return m_enabled;
}

void AdBlock::fetchFilterLists(QNetworkAccessManager *nam) {
    const QStringList filterLists = {
        "https://ublockorigin.github.io/uAssets/filters/filters.txt", // uBlock Origin – Base/Ads
        "https://ublockorigin.github.io/uAssets/filters/privacy.txt", // uBlock Origin – Privacy
        "https://ublockorigin.github.io/uAssets/filters/quick-fixes.txt", // uBlock Origin – Quick fixes
        "https://ublockorigin.github.io/uAssets/filters/unbreak.txt", // uBlock Origin – Unbreak
        "https://ublockorigin.github.io/uAssets/thirdparties/easylist.txt", // 3rdParty - EasyList
        "https://ublockorigin.github.io/uAssets/thirdparties/easyprivacy.txt" // 3rdParty - EasyPrivacy
    };

    const QString filterCacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/FilterCache";
    QDir().mkpath(filterCacheDir);

    for (const QString &url : filterLists) {
        const QString cachePath = filterCacheDir + "/" + QString::number(qHash(url), 16) + ".txt";

        QFile cacheFile(cachePath);
        bool hadCache = false;
        if (cacheFile.open(QIODevice::ReadOnly)) {
            hadCache = true;
            applyFilterData(cacheFile.readAll());
            cacheFile.close();
        }

        QNetworkRequest request{QUrl(url)};
        QNetworkReply* reply = nam->get(request);

        connect(reply, &QNetworkReply::finished, this, [this, reply, cachePath, hadCache]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();

                QFile cacheOut(cachePath);
                if (cacheOut.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    cacheOut.write(data);
                    cacheOut.close();
                }

                // Cache already applied above if it existed, only apply this
                // fresh copy live when this was the very first run for this URL.
                if (!hadCache) {
                    applyFilterData(data);
                }
            } else {
                qWarning() << "Filter list fetch failed:" << reply->url() << reply->errorString();
            }
            reply->deleteLater();
        });
    }
}

void AdBlock::fetchPublicSuffixData(QNetworkAccessManager *nam) {
    QUrl pslUrl("https://publicsuffix.org/list/public_suffix_list.dat");
    QNetworkReply* reply = nam->get(QNetworkRequest(pslUrl));

    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QThreadPool::globalInstance()->start([data]() {
                Interceptor::loadPublicSuffixData(data);
            });
        }
        reply->deleteLater();
    });
}

void AdBlock::applyFilterData(const QByteArray &data) {
    // Process filter rules in a background thread to keep UI responsive
    QThreadPool::globalInstance()->start([this, data]() {
            // modifiers we can't safely enforce (need body/header rewrite,
            // or reference other rules), drop the rule instead of guessing
            static const QSet<QString> kUnsupportedModifiers = {
                "badfilter", "csp", "removeparam", "redirect", "redirect-rule",
                "replace", "cookie", "empty", "mp4", "cname", "elemhide",
                "generichide", "genericblock", "content", "jsonprune", "hls",
                "referrerpolicy", "urltransform", "uritransform", "header",
                "document", "doc", "all"
            };
            static const QHash<QString, uint32_t> kResourceKeywords = {
                {"script", ResCatScript}, {"image", ResCatImage},
                {"stylesheet", ResCatStylesheet}, {"css", ResCatStylesheet},
                {"object", ResCatObject},
                {"xmlhttprequest", ResCatXHR}, {"xhr", ResCatXHR},
                {"subdocument", ResCatSubdocument}, {"frame", ResCatSubdocument},
                {"font", ResCatFont}, {"media", ResCatMedia},
                {"websocket", ResCatWebSocket}, {"ws", ResCatWebSocket},
                {"ping", ResCatPing}, {"popup", ResCatPopup}, {"other", ResCatOther},
            };

            // parses "a,b,c" in "||domain^$a,b,c". false = drop the rule
            auto parseFilterOptions = [&](const QString &optionsStr, FilterRule &rule) -> bool {
                if (optionsStr.isEmpty()) return true;
                const QStringList tokens = optionsStr.split(',', Qt::SkipEmptyParts);
                for (QString token : tokens) {
                    token = token.trimmed();
                    if (token.isEmpty()) continue;

                    if (token.startsWith("domain=", Qt::CaseInsensitive)) {
                        const QStringList domains = token.mid(7).split('|', Qt::SkipEmptyParts);
                        for (const QString &d : domains) {
                            if (d.startsWith('~')) rule.domainExcludes.push_back(d.mid(1).toLower().toStdU16String());
                            else rule.domainIncludes.push_back(d.toLower().toStdU16String());
                        }
                        continue;
                    }

                    if (token.startsWith("method=", Qt::CaseInsensitive)) {
                        const QStringList methods = token.mid(7).split('|', Qt::SkipEmptyParts);
                        for (const QString &m : methods) {
                            bool neg = m.startsWith('~');
                            const QByteArray name = (neg ? m.mid(1) : m).toUtf8();
                            const uint16_t bit = Interceptor::methodForString(name);
                            if (bit == 0) return false; // unknown method, drop rule instead of guessing
                            if (neg) rule.methodExcludeMask |= bit;
                            else rule.methodIncludeMask |= bit;
                        }
                        continue;
                    }

                    bool negated = token.startsWith('~');
                    QString key = (negated ? token.mid(1) : token).toLower();

                    if (kUnsupportedModifiers.contains(key)) return false;

                    if (key == "third-party" || key == "3p") { rule.thirdParty = negated ? -1 : 1; continue; }
                    if (key == "first-party" || key == "1p") { rule.thirdParty = negated ? 1 : -1; continue; }
                    if (key == "important") { rule.important = true; continue; }

                    auto it = kResourceKeywords.find(key);
                    if (it != kResourceKeywords.end()) {
                        if (negated) rule.excludeMask |= it.value();
                        else rule.includeMask |= it.value();
                        continue;
                    }

                    // unknown modifier, drop instead of guessing
                    return false;
                }
                return true;
            };

            auto extractBase = [](QString l) -> QString {
                l = l.trimmed();
                int d = l.indexOf('$');
                if (d != -1) l = l.left(d);
                return l.trimmed().toLower();
            };

            QSet<QString> badfilteredBases; {
                QTextStream scan(data);
                while (!scan.atEnd()) {
                    QString line = scan.readLine().trimmed();
                    if (line.isEmpty() || line.startsWith('!') || line.startsWith('@')) continue;
                    int dollarPos = line.indexOf('$');
                    if (dollarPos == -1) continue;
                    const QStringList opts = line.mid(dollarPos + 1).split(',', Qt::SkipEmptyParts);
                    bool hasBadfilter = false;
                    for (const QString &o : opts) {
                        if (o.trimmed().compare("badfilter", Qt::CaseInsensitive) == 0) { hasBadfilter = true; break; }
                    }
                    if (hasBadfilter) badfilteredBases.insert(extractBase(line));
                }
            }

            QTextStream in(data);
            int count = 0;
            QStringList domainsToAdd;
            QVector<QPair<QString, FilterRule>> restrictedDomainsToAdd;
            QStringList patternsToAdd;
            QVector<QPair<QString, FilterRule>> restrictedPatternsToAdd;
            QStringList allowsToAdd;
            QVector<QPair<QString, FilterRule>> restrictedAllowsToAdd;

            struct CosmeticRuleData { QStringList domains; QString selector; bool isException; };
            QVector<CosmeticRuleData> cosmeticRulesToAdd;

            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith("!") || line.startsWith("[")) {
                    continue;
                    }

                    // Cosmetic filters: "domain1,domain2##selector" or generic "##selector",
                    // with "#@#" exceptions. We don't support "#$#"/"#%#" injection or
                    // scriptlets to prevent untrusted CSS/JavaScript execution.
                    if (!line.contains("#$#") && !line.contains("#%#") &&
                        (line.contains("#@#") || line.contains("##"))) {
                        const bool isException = line.contains("#@#");
                    const QString marker = isException ? "#@#" : "##";
                    const int markerPos = line.indexOf(marker);
                    if (markerPos == -1) continue;

                    const QString domainsPart = line.left(markerPos).trimmed();
                        const QString selector = line.mid(markerPos + marker.length()).trimmed();

                        QStringList domains;
                        if (!domainsPart.isEmpty()) {
                            domains = domainsPart.split(',', Qt::SkipEmptyParts);
                            for (QString &d : domains) d = d.trimmed().toLower();
                        }
                        if (!selector.isEmpty())
                            cosmeticRulesToAdd << CosmeticRuleData{domains, selector, isException};
                        continue;
                    }

                    if (!badfilteredBases.isEmpty() && badfilteredBases.contains(extractBase(line))) {
                        continue; // cancelled by a $badfilter rule elsewhere in this list
                    }

                    if (line.startsWith("@@")) {
                        QString exception = line.mid(2);
                        if (exception.startsWith("||")) exception = exception.mid(2);

                        QString optionsStr;
                        int dollarPos = exception.indexOf('$');
                        if (dollarPos != -1) {
                            optionsStr = exception.mid(dollarPos + 1);
                            exception = exception.left(dollarPos);
                        }

                        if (exception.contains('/')) {
                            QString pattern = exception.trimmed().toLower();
                            if (!pattern.isEmpty()) {
                                FilterRule rule;
                                if (parseFilterOptions(optionsStr, rule)) {
                                    m_interceptor->addAllowedDomain(pattern.section('/', 0, 0), rule.domainIncludes.empty() && rule.domainExcludes.empty() ? std::optional<FilterRule>(rule) : std::optional<FilterRule>(rule));
                                }
                            }
                            continue;
                        }

                        exception = exception.section('^', 0, 0).trimmed().toLower();

                        if (!exception.isEmpty() && exception.contains(".")) {
                            FilterRule rule;
                            if (parseFilterOptions(optionsStr, rule)) {
                                if (rule.isTrivial()) allowsToAdd << exception;
                                else restrictedAllowsToAdd << qMakePair(exception, rule);
                            }
                        }
                        continue;
                    }

                    QString domain;
                    QString optionsStr;
                    if (line.startsWith("||")) {
                        domain = line.mid(2);
                        int dollarPos = domain.indexOf('$');
                        if (dollarPos != -1) {
                            optionsStr = domain.mid(dollarPos + 1);
                            domain = domain.left(dollarPos);
                        }
                        // '/' means this targets a path, not the whole domain route it through the pattern matcher instead of blockedDomains.
                        int slashPos = domain.indexOf('/');
                        if (slashPos != -1) {
                            QString pattern = domain.trimmed().toLower();
                            if (!pattern.isEmpty()) {
                                FilterRule rule;
                                if (parseFilterOptions(optionsStr, rule)) {
                                    if (rule.isTrivial()) patternsToAdd << pattern;
                                    else restrictedPatternsToAdd << qMakePair(pattern, rule);
                                }
                            }
                            continue;
                        }
                        int end = domain.indexOf(QRegularExpression("[\\^/:]"));
                        if (end != -1) domain = domain.left(end);
                    } else if (line.contains("/") && !line.contains("*")) {
                        QString pattern = line.startsWith("||") ? line.mid(2) : line;
                        int dollarPos = pattern.indexOf('$');
                        QString patOptions;
                        if (dollarPos != -1) {
                            patOptions = pattern.mid(dollarPos + 1);
                            pattern = pattern.left(dollarPos);
                        }
                        // '|' anchors the URL start/end, it never appears literally
                        // in a real URL, so leaving it in the pattern text makes the
                        // rule permanently unmatchable.
                        if (pattern.startsWith("|")) pattern = pattern.mid(1);
                        if (pattern.endsWith("|")) pattern.chop(1);
                        pattern = pattern.trimmed().toLower();
                        if (!pattern.isEmpty()) {
                            FilterRule rule;
                            if (parseFilterOptions(patOptions, rule)) {
                                if (rule.isTrivial()) patternsToAdd << pattern;
                                else restrictedPatternsToAdd << qMakePair(pattern, rule);
                            }
                        }
                        continue;
                    } else if (line.contains("*")) {
                        QString pattern = line;
                        int dollarPos = pattern.indexOf('$');
                        QString patOptions;
                        if (dollarPos != -1) {
                            patOptions = pattern.mid(dollarPos + 1);
                            pattern = pattern.left(dollarPos);
                        }
                        if (pattern.startsWith("||")) pattern = pattern.mid(2);
                        if (pattern.startsWith("|")) pattern = pattern.mid(1);
                        if (pattern.endsWith("|")) pattern.chop(1);
                        pattern = pattern.trimmed().toLower();
                        if (!pattern.isEmpty()) {
                            FilterRule rule;
                            if (parseFilterOptions(patOptions, rule)) {
                                if (rule.isTrivial()) patternsToAdd << pattern;
                                else restrictedPatternsToAdd << qMakePair(pattern, rule);
                            }
                        }
                        continue;
                    } else if (line.startsWith(".")) {
                        QString d = line.mid(1);
                        int dollarPos = d.indexOf('$');
                        if (dollarPos != -1) {
                            optionsStr = d.mid(dollarPos + 1);
                            d = d.left(dollarPos);
                        }
                        int end = d.indexOf(QRegularExpression("[\\^/:]"));
                        if (end != -1) d = d.left(end);
                        domain = d;
                    } else {
                        domain = line;
                    }

                    domain = domain.trimmed().toLower();
                    if (domain.isEmpty() || !domain.contains(".") || domain.contains("*")) continue;

                    FilterRule rule;
                if (!parseFilterOptions(optionsStr, rule)) continue;

                if (rule.isTrivial()) {
                    domainsToAdd << domain;
                } else {
                    restrictedDomainsToAdd << qMakePair(domain, rule);
                }
                count++;
            }

            for (const auto &c : cosmeticRulesToAdd) {
                m_interceptor->addCosmeticRule(c.domains, c.selector, c.isException);
            }
            for(const QString& d : domainsToAdd) {
                m_interceptor->addBlockedDomain(d);
            }
            for (const auto &pair : restrictedDomainsToAdd) {
                m_interceptor->addBlockedDomain(pair.first, pair.second);
            }
            for(const QString& p : patternsToAdd) {
                m_interceptor->addBlockedPattern(p);
            }
            for (const auto &pair : restrictedPatternsToAdd) {
                m_interceptor->addBlockedPattern(pair.first, pair.second);
            }
            for(const QString& a : allowsToAdd) {
                m_interceptor->addAllowedDomain(a);
            }
            for (const auto &pair : restrictedAllowsToAdd) {
                m_interceptor->addAllowedDomain(pair.first, pair.second);
            }

            // profile->scripts() / tabWidget must be touched on the GUI thread.
            QMetaObject::invokeMethod(this, [this]() { refreshGenericCosmeticScript(); }, Qt::QueuedConnection);

            #ifdef DEBUG_MODE
            qDebug() << "AdRules:" << count;
            #endif
    });
}

void AdBlock::setEnabled(bool enabled) {
    m_enabled = enabled;
    m_interceptor->setEnabled(enabled);

    QList<QWebEngineScript> existing = m_profile->scripts()->find("ytAdBlock");

    if (enabled) {
        if (existing.isEmpty()) {
            QFile ytAdBlock(":/scripts/ytAdBlock.js");
            if (ytAdBlock.open(QIODevice::ReadOnly)) {
                QByteArray scriptCode = ytAdBlock.readAll();

                QWebEngineScript ytAB;
                ytAB.setName("ytAdBlock");
                ytAB.setInjectionPoint(QWebEngineScript::DocumentCreation);
                ytAB.setRunsOnSubFrames(false);
                ytAB.setWorldId(QWebEngineScript::MainWorld);
                ytAB.setSourceCode(QString::fromUtf8(scriptCode));

                m_profile->scripts()->insert(ytAB);
            }
        }
    } else {
        for (const QWebEngineScript &s : existing) {
            m_profile->scripts()->remove(s);
        }
    }
    if (enabled) {
        doRefreshGenericCosmeticScript();
    } else {
        const QList<QWebEngineScript> genericCosmetic = m_profile->scripts()->find("cosmeticGeneric");
        for (const QWebEngineScript &s : genericCosmetic) m_profile->scripts()->remove(s);
    }
}

void AdBlock::refreshGenericCosmeticScript() {
    // gets called once per list (cache + network + local-filters.txt), so on
    // startup this fires like 10 times back to back, debounce it.
    if (m_cosmeticRefreshPending) return;
    m_cosmeticRefreshPending = true;
    QTimer::singleShot(300, this, [this]() {
        m_cosmeticRefreshPending = false;
        doRefreshGenericCosmeticScript();
    });
}

// Takes the raw selector list and produces the page-side cosmetic engine.
// Class/id/tag-only selectors go straight into an unconditional stylesheet
// (Blink hashes those, cheap to match). Selectors with combinators/
// attributes/pseudo-classes are probed against the live DOM once and only
// the actually-matching ones get injected, so heavy sites don't carry a few
// hundred unused complex selectors on every style recalc / resize. A slow
// watchdog re-scans late DOM additions (SPA ads) for a while after load.
QString AdBlock::buildCosmeticEngine(const QStringList &selectors, const QString &styleId) {
    return QString::fromLatin1(R"((function(){
var all=%1;
var STYLE="%2";
if(!all||!all.length)return;
var cheap=[],probe=[];
for(var i=0;i<all.length;i++){
    var s=all[i],cpx=false;
    for(var j=0;j<s.length;j++){
        var c=s.charCodeAt(j);
        if(c===32||c===62||c===126||c===43||c===91||c===58){cpx=true;break;}
    }
    (cpx?probe:cheap).push(s);
}
function putStyle(idTag,text){
    if(!document.getElementById)return;
    var el=document.getElementById(idTag);
    if(el)el.remove();
    if(!text)return;
    el=document.createElement("style");
    el.id=idTag;
    el.textContent=text;
    var root=document.head||document.documentElement;
    if(root)root.appendChild(el);
}
function listBlock(list){return ":where("+list.join(",\n")+"){display:none!important}";}
if(cheap.length)putStyle(STYLE,listBlock(cheap));
var matched=[],cursor=0;
function putMatched(){
    if(matched.length)putStyle(STYLE+"M",listBlock(matched));
    else putStyle(STYLE+"M","");
}
function idleRun(fn){
    if(window.requestIdleCallback)window.requestIdleCallback(fn,{timeout:300});
    else setTimeout(fn,60);
}
function probeSome(){
    var t0=Date.now();
    while(cursor<probe.length&&Date.now()-t0<5){
        var s=probe[cursor++];
        try{if(document.querySelector(s)&&matched.indexOf(s)<0)matched.push(s);}catch(e){}
    }
    putMatched();
    if(cursor<probe.length)idleRun(probeSome);
}
idleRun(probeSome);
var quiet=0;
function watchdog(){
    var t0=Date.now(),changed=false;
    for(var i=0;i<probe.length&&Date.now()-t0<5;i++){
        var s=probe[i];
        if(matched.indexOf(s)>=0)continue;
        try{if(document.querySelector(s)){matched.push(s);changed=true;}}catch(e){}
    }
    if(changed){quiet=0;putMatched();}else quiet++;
    if(quiet<40)setTimeout(watchdog,2000);
}
setTimeout(watchdog,1500);
})();)")
    .arg(QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(selectors)).toJson(QJsonDocument::Compact)), styleId);
}

void AdBlock::doRefreshGenericCosmeticScript() {
    const QStringList selectors = m_interceptor->genericCosmeticSelectors();
    const QString jsBody = buildCosmeticEngine(selectors, "cosmeticGeneric");

    QWebEngineScriptCollection* scripts = m_profile->scripts();
    const QList<QWebEngineScript> existing = scripts->find("cosmeticGeneric");
    for (const QWebEngineScript &s : existing) scripts->remove(s);
    if (selectors.isEmpty()) return;

    QWebEngineScript script;
    script.setName("cosmeticGeneric");
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    script.setSourceCode(jsBody);
    scripts->insert(script);

    emit genericCosmeticChanged(jsBody);
}

void AdBlock::applyDomainCosmeticScript(QWebEnginePage *page, const QString &host) {
    if (!page) return;

#ifdef DEBUG_MODE
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    static int s_cc = 0;
    static long long s_cus = 0;
    static long long s_cmax = 0;
    const auto cssDone = [&]() {
        long long us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
        s_cc++;
        s_cus += us;
        if (us > s_cmax) s_cmax = us;
        if (s_cc % 10 == 0) {
            qDebug() << "[cosmetic] calls:" << s_cc
                     << "avg_us:" << (int)(s_cus / qMax(1LL, s_cc))
                     << "max_us:" << s_cmax;
        }
    };
#endif

    const QStringList selectors = m_interceptor->isEnabled() ? m_interceptor->cosmeticSelectorsFor(host) : QStringList();
#ifdef DEBUG_MODE
    cssDone();
    qDebug() << "[cosmetic] host:" << host << "selectors:" << selectors.size();
#endif

    const QString jsBody = buildCosmeticEngine(selectors, "cosmeticDomain");

    QWebEngineScriptCollection &pageScripts = page->scripts();
    const QList<QWebEngineScript> existing = pageScripts.find("cosmeticDomain");
    for (const QWebEngineScript &s : existing) pageScripts.remove(s);

    if (!selectors.isEmpty()) {
        QWebEngineScript script;
        script.setName("cosmeticDomain");
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setRunsOnSubFrames(false);
        script.setSourceCode(jsBody);
        pageScripts.insert(script);
    }

    page->runJavaScript(jsBody);
}

void AdBlock::removeDomainCosmeticScript(QWebEnginePage *page) {
    if (!page) return;

    const QList<QWebEngineScript> domainCosmetic = page->scripts().find("cosmeticDomain");
    for (const QWebEngineScript &s : domainCosmetic) page->scripts().remove(s);

    page->runJavaScript(
        "document.getElementById('cosmeticGeneric')?.remove();"
        "document.getElementById('cosmeticDomain')?.remove();"
    );
}