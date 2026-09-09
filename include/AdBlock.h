/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef ADBLOCK_H
#define ADBLOCK_H

#include <QObject>

class QWebEngineProfile;
class QWebEnginePage;
class QNetworkAccessManager;
class Interceptor;

// Owns the request interceptor and everything filter list/cosmetic related.
// Browser only keeps a thin pointer to it and asks for page-level cosmetics.
class AdBlock : public QObject {
    Q_OBJECT
public:
    explicit AdBlock(QWebEngineProfile *profile, QObject *parent = nullptr);

    Interceptor *interceptor() const;
    bool isEnabled() const;

    void fetchFilterLists(QNetworkAccessManager *nam);

    // Downloads the Public Suffix List and streams it to the interceptor for
    // registrable-domain (first-party) classification. Fetched alongside the
    // filter lists so all blocking data downloads live in one place.
    void fetchPublicSuffixData(QNetworkAccessManager *nam);

    // Toggles the whole stack: request interceptor, ytAdBlock script and the
    // profile-wide generic cosmetic script.
    void setEnabled(bool enabled);

    // Cosmetic scripts. Domain scripts attach per page, page->host() moves with
    // navigation. Generic script is profile-level; refresh is debounced against
    // the multiple per-list apply calls that fire at startup.
    void refreshGenericCosmeticScript();
    void applyDomainCosmeticScript(QWebEnginePage *page, const QString &host);
    void removeDomainCosmeticScript(QWebEnginePage *page);

signals:
    // Emitted after the generic list was re-applied; Browser re-runs the engine
    // on its already-open tabs.
    void genericCosmeticChanged(const QString &engineSource);

private:
    void doRefreshGenericCosmeticScript();
    void applyFilterData(const QByteArray &data);
    static QString buildCosmeticEngine(const QStringList &selectors, const QString &styleId);

    Interceptor *m_interceptor = nullptr;
    QWebEngineProfile *m_profile = nullptr;
    bool m_enabled = true;
    bool m_cosmeticRefreshPending = false;
};

#endif // ADBLOCK_H
