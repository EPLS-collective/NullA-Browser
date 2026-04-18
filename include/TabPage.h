/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef TABPAGE_H
#define TABPAGE_H

#include <QStackedWidget>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QWebEngineHistory>
#include <QUrl>
#include "StartPage.h"

class TabPage : public QStackedWidget {
    Q_OBJECT
public:
    explicit TabPage(QWebEngineProfile* profile, QWidget* parent = nullptr);

    void goBack();
    void goForward();
    QUrl currentUrl() const;
    QWebEngineView* webView();
    StartPage* getStartPage();
    void applyTheme(bool isDark);

signals:
    void titleChanged(const QString& title);
    void urlChanged(const QUrl& url);
    void iconChanged(const QIcon &icon);

private:
    QWebEngineView* view;
    StartPage* startPage;
};

#endif // TABPAGE_H
