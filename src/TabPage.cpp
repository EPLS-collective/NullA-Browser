/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/TabPage.h"
#include "../include/Browser.h"
#include "../include/Localization.h"
#include "../include/ExtensionBridge.h"
#include <QApplication>
#include <QWebChannel>
#include <QDebug>

class WebPage : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;

protected:
    QWebEnginePage* createWindow(WebWindowType type) override {
        Q_UNUSED(type);

        // Explicitly override createWindow to handle requests for new windows (e.g., target="_blank" or middle-click).
        // It interacts with the main Browser instance to create a new tab and returns the pointer to its underlying web page.

        Browser* browser = qobject_cast<Browser*>(QApplication::activeWindow());

        if (browser) {
            browser->addNewTab();
            TabPage* newTab = browser->currentTabPage();
            if (newTab) {
                newTab->setCurrentIndex(1);
                return newTab->webView()->page();
            }
        }
        return nullptr;
    }

    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                    const QString &message,
                                    int lineNumber,
                                    const QString &sourceID) override {
    #ifdef DEBUG_MODE
        const char *levelStr = "LOG";
        switch (level) {
            case QWebEnginePage::InfoMessageLevel:    levelStr = "INFO"; break;
            case QWebEnginePage::WarningMessageLevel: levelStr = "WARN"; break;
            case QWebEnginePage::ErrorMessageLevel:    levelStr = "ERROR"; break;
        }
        qDebug().noquote() << QString("[JS %1] %2:%3 - %4")
            .arg(levelStr).arg(sourceID).arg(lineNumber).arg(message);
    #else
        Q_UNUSED(level); Q_UNUSED(message); Q_UNUSED(lineNumber); Q_UNUSED(sourceID);
    #endif
    }
};

TabPage::TabPage(QWebEngineProfile* profile, QWidget* parent) : QStackedWidget(parent) {
    view = new QWebEngineView(this);
    view->setPage(new WebPage(profile, view));
    view->page()->setWebChannel(ExtensionBridge::channel());

    startPage = new StartPage();

    addWidget(startPage);
    addWidget(view);
    setCurrentIndex(0);

    connect(startPage, &StartPage::navigateTo, this, [this](const QUrl& url) {
        view->load(url);
        setCurrentIndex(1);
    });

    connect(view, &QWebEngineView::titleChanged, this, &TabPage::titleChanged);
    connect(view, &QWebEngineView::urlChanged, this, &TabPage::urlChanged);

    connect(view, &QWebEngineView::loadStarted, this, [this]() {
        if (currentIndex() == 1) {
            emit iconChanged(QIcon(":/nulla_load.png"));
        }
    });

    connect(view, &QWebEngineView::iconChanged, this, [this](const QIcon &icon) {
        if (currentIndex() == 1 && !icon.isNull()) {
            emit iconChanged(icon);
        }
    });

    connect(view, &QWebEngineView::loadFinished, this, [this](bool) {
        if (currentIndex() == 1) {
            if (view->icon().isNull()) {
                emit iconChanged(QIcon(":/nulla_icon.png"));
            } else {
                emit iconChanged(view->icon());
            }
        }
    });
}

void TabPage::goBack() {

    if (currentIndex() == 1) {
        if (view->history()->canGoBack()) {
            view->back();
        } else {
            setCurrentIndex(0);
            emit urlChanged(QUrl(""));
            emit titleChanged(Localization::qget("new_tab"));
            emit iconChanged(QIcon(":/nulla_icon.png"));
        }
    }
}

void TabPage::goForward() {
    if (currentIndex() == 0 && view->history()->count() > 0) {
        setCurrentIndex(1);
        emit titleChanged(view->title());
        emit urlChanged(view->url());
        emit iconChanged(view->icon().isNull() ? QIcon(":/nulla_icon.png") : view->icon());
    } else if (currentIndex() == 1) {
        view->forward();
    }
}

QUrl TabPage::currentUrl() const {
    return (currentIndex() == 1) ? view->url() : QUrl("");
}

QWebEngineView* TabPage::webView() {
    return view;
}

StartPage* TabPage::getStartPage() {
    return startPage;
}

void TabPage::applyTheme(bool isDark) {
    startPage->applyTheme(isDark);
}
