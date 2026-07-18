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
#include <string>
#include <QMutex>

class Interceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    explicit Interceptor(QObject* parent = nullptr);

    void interceptRequest(QWebEngineUrlRequestInfo &info) override;
    void addBlockedDomain(const QString &domain);
    bool isBlocked(const QString &host) const;
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }

private:
    std::unordered_set<std::u16string> blockedDomains;
    mutable QMutex mutex;
    bool m_enabled = true;
};

#endif // INTERCEPTOR_H
