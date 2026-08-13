/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/Browser.h"
#include "../include/Interceptor.h"
#include "../include/Localization.h"
#include "../include/DownloadManager.h"
#include "../include/Render.h"
#include "../include/ExtensionStore.h"
#include "../include/UpdateChecker.h"
#include <QNetworkAccessManager>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineSettings>
#include <QWebEnginePermission>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineProfile>
#include <QWebEnginePage>
#include <QCheckBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QVBoxLayout>
#include <QAction>
#include <QDebug>
#include <QApplication>
#include <QStyleFactory>
#include <QToolBar>
#include <QStandardPaths>
#include <QThreadPool>
#include <QToolButton>
#include <QFontDatabase>
#include <QShortcut>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QWebEngineExtensionManager>
#include <QWebEngineExtensionInfo>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

Browser::Browser(const QString &initialUrl) {

    // Main window configuration
    setWindowTitle("NullA Browser");
    setWindowIcon(QIcon(":/nulla_icon.png"));
    resize(854, 480);

    settings = new QSettings("NullA", "Browser", this);

    // Profile and storage isolation
    profile = new QWebEngineProfile("NullA", this);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Data";
    QDir().mkpath(dataPath);
    profile->setPersistentStoragePath(dataPath);
    profile->setCachePath(dataPath + "/Cache");
    profile->setHttpCacheMaximumSize(100 * 1024 * 1024); // Cap cache at 100MB

    connect(profile->extensionManager(), &QWebEngineExtensionManager::installFinished,
            this, [](const QWebEngineExtensionInfo &info) {
                qDebug() << "Extension install finished:" << info.name() << info.error();
    });
    connect(profile->extensionManager(), &QWebEngineExtensionManager::uninstallFinished,
            this, [](const QWebEngineExtensionInfo &info) {
                qDebug() << "Extension uninstall finished:" << info.name() << info.error();
    });

    QString savedLang = settings->value("language", "en").toString();
    Localization::loadLanguage(savedLang.toStdString());

    auto *store = profile->cookieStore();

    // In-memory cookie management and manual persistence
    connect(store, &QWebEngineCookieStore::cookieAdded, this, [this](const QNetworkCookie &cookie) {

        cookieCache.removeAll(cookie);
        cookieCache.append(cookie);

        qDebug() << "Saved cookie:" << cookie.name();

        saveCookiesToJson();
    });

    connect(store, &QWebEngineCookieStore::cookieRemoved, this, [this](const QNetworkCookie &cookie) {

        cookieCache.removeAll(cookie);

        qDebug() << "Removed cookie:" << cookie.name();

        saveCookiesToJson();
    });

    // Dynamic User-Agent fetching to match the latest stable Chrome version
    auto *mgr = new QNetworkAccessManager(this);
    QUrl url("https://googlechromelabs.github.io/chrome-for-testing/last-known-good-versions.json");

    connect(mgr, &QNetworkAccessManager::finished, this, [this](QNetworkReply* reply) {
        if (reply->error() == QNetworkReply::NoError && profile) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);

            QJsonObject root = doc.object();
            QJsonObject channels = root["channels"].toObject();
            QJsonObject stable = channels["Stable"].toObject();
            QString version = stable["version"].toString();

            if (!version.isEmpty()) {
                QString newUA = QString("Chrome/%1").arg(version);

                profile->setHttpUserAgent(newUA);
                qDebug() << "NullA UA:" << newUA;
            } else {
                qWarning() << "Version information could not be retrieved.";
            }
        } else {
            qWarning() << "Network ERROR:" << reply->errorString();
        }

        reply->deleteLater();
    });

    mgr->get(QNetworkRequest(url));

    // The aim here is to try and reduce font fingerprinting; I don't think it's working, but let it stay here anyway
    QFontDatabase::removeAllApplicationFonts();

    // Strict WebEngine security hardening
    profile->settings()->setAttribute(QWebEngineSettings::WebRTCPublicInterfacesOnly, true); // Prevent local IP leaks
    profile->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);
    profile->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false); // Block pop-ups
    profile->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, false);
    profile->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, false);
    profile->settings()->setAttribute(QWebEngineSettings::ErrorPageEnabled, true);
    profile->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, false); // Disable PDF/Flash plugins
    profile->settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    profile->settings()->setAttribute(QWebEngineSettings::AutoLoadImages, true);
    profile->settings()->setAttribute(QWebEngineSettings::HyperlinkAuditingEnabled, false); // Disable <a ping> tracking
    profile->settings()->setAttribute(QWebEngineSettings::ScrollAnimatorEnabled, false);
    profile->settings()->setAttribute(QWebEngineSettings::XSSAuditingEnabled, true);
    profile->settings()->setAttribute(QWebEngineSettings::WebAttribute::LocalContentCanAccessRemoteUrls, false);
    profile->settings()->setAttribute(QWebEngineSettings::FocusOnNavigationEnabled, false);
    profile->settings()->setAttribute(QWebEngineSettings::AllowRunningInsecureContent, false);

    // Injecting anti-fingerprinting script at document creation
    QFile antiFingerprint(":/scripts/antiFingerprint.js");
    if(antiFingerprint.open(QIODevice::ReadOnly)) {
        QByteArray scriptCode = antiFingerprint.readAll();

        QWebEngineScript antiFP;
        antiFP.setName("antiFingerprint");
        antiFP.setInjectionPoint(QWebEngineScript::DocumentCreation);
        antiFP.setRunsOnSubFrames(true);
        antiFP.setWorldId(QWebEngineScript::MainWorld);
        antiFP.setSourceCode(QString::fromUtf8(scriptCode));

        profile->scripts()->insert(antiFP);
    }

    adBlocker = new Interceptor(profile);
    profile->setUrlRequestInterceptor(adBlocker);
    setAdBlockEnabled(settings->value("adBlockEnabled", true).toBool());

    m_downloadManager = DownloadManager::instance();

    connect(profile, &QWebEngineProfile::downloadRequested, this, &Browser::handleDownload);

    // Ad-Blocker initialization and filter list fetching
    adBlocker = new Interceptor(profile);
    adBlocker = new Interceptor(profile);
    profile->setUrlRequestInterceptor(adBlocker);
    adBlocker->setEnabled(settings->value("adBlockEnabled", true).toBool());
    profile->setUrlRequestInterceptor(adBlocker);

    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    QNetworkRequest request(QUrl("https://filters.adtidy.org/extension/ublock/filters/2.txt"));

    QNetworkReply* reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            // Process filter rules in a background thread to keep UI responsive
            QThreadPool::globalInstance()->start([this, data]() {
                QTextStream in(data);
                int count = 0;
                QStringList domainsToAdd;

                while (!in.atEnd()) {
                    QString line = in.readLine().trimmed();
                    if (line.isEmpty() || line.startsWith("!") || line.startsWith("[") ||
                        line.contains("##") || line.contains("#@#") || line.startsWith("@@")) {
                        continue;
                        }

                        QString domain;
                    if (line.startsWith("||")) {
                        domain = line.mid(2);
                        int end = domain.indexOf(QRegularExpression("[\\^/\\$:]"));
                        if (end != -1) domain = domain.left(end);
                    } else if (line.contains("/") || line.contains("*")) {
                        continue;
                    } else {
                        domain = line;
                    }

                    domain = domain.trimmed().toLower();
                    if (domain.isEmpty() || !domain.contains(".")) continue;

                    // Essential domains whitelist to prevent breaking core web services
                    static const QStringList whiteList = {
                        "google", "gstatic", "googleapis", "youtube", "ytimg", "ggpht",
                        "cloudflare", "cdn", "unpkg", "jsdelivr", "fontawesome", "accounts.google",
                        "discord", "discordapp", "discordmedia", "discordusercontent"
                    };

                    bool isWhiteListed = false;
                    for (const QString &allowed : whiteList) {
                        if (domain.contains(allowed)) {
                            isWhiteListed = true;
                            break;
                        }
                    }

                    if (!isWhiteListed) {
                        domainsToAdd << domain;
                        count++;
                    }
                }

                for(const QString& d : domainsToAdd) {
                    adBlocker->addBlockedDomain(d);
                }

                qDebug() << "AdRules:" << count;
            });
        }
        reply->deleteLater();
    });

    // Layout and UI construction
    QWidget* central = new QWidget();
    central->setContentsMargins(0, 0, 0, 0); // Remove margins from window
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0); // Same here
    mainLayout->setSpacing(0);
    setCentralWidget(central);

    tabWidget = new TabWidget();
    TabBar* bar = qobject_cast<TabBar*>(tabWidget->tabBar());

    createToolbar();
    loadCookiesFromJson();
    loadBookmarks();
    loadExtensions();

    m_updateChecker = new UpdateChecker(this);
    connect(m_updateChecker, &UpdateChecker::updateAvailable, this, &Browser::onUpdateAvailable);
    connect(m_updateChecker, &UpdateChecker::checkFailed, this, &Browser::onUpdateCheckFailed);

    m_updateChecker->checkForUpdates();

    updateCheckTimer = new QTimer(this);
    connect(updateCheckTimer, &QTimer::timeout, m_updateChecker, &UpdateChecker::checkForUpdates);
    updateCheckTimer->start(6 * 60 * 60 * 1000); // 6H

    bookmarkContextMenu = new QMenu(this);
    QAction* deleteAction = bookmarkContextMenu->addAction(Localization::qget("bookmark_del"));
    connect(deleteAction, &QAction::triggered, this, [this]() {
        QListWidgetItem* currentItem = suggestionList->currentItem();
        if (currentItem) {
            QString url = currentItem->data(Qt::UserRole).toString();
            removeBookmark(url);
        }
    });

    mainLayout->addWidget(toolbar);
    mainLayout->addWidget(tabWidget);

    renderController = new RenderController(this);

    connect(renderController, &RenderController::frameReady, this, [this]() {
        if (auto* page = currentTabPage()) {
            page->webView()->update();
        }
    });

    connect(tabWidget, &QTabWidget::currentChanged, this, &Browser::handleTabChange);

    connect(tabWidget, &TabWidget::tabCloseRequested, this, &Browser::closeTab);

    if (bar) {
        if (tabWidget) {
            TabBar* bar = tabWidget->getTabBar();
            if (bar && bar->plusButton) {
                connect(bar->plusButton, &QPushButton::clicked, this, [this](){
                    addNewTab();
                });
            }
        }
    }

    // Search engine and initial state setup
    searchEngines["DuckDuckGo"] = "https://duckduckgo.com/?q=%1";
    QStringList engines = settings->value("customSearchEngines").toStringList();

    for(const QString &engine : engines) {
        QStringList parts = engine.split("|");
        if(parts.size() == 2) {
            QString name = parts[0];
            QString url = parts[1];
            url.replace("%s", "%1");
            searchEngines[name] = url;
        }
    }

    currentSearchEngine = settings->value("searchEngine", "DuckDuckGo").toString();

    if (initialUrl.isEmpty()) {
        addNewTab();
    } else {
        addNewTab();
        if (auto* p = currentTabPage()) {
            p->webView()->load(QUrl::fromUserInput(initialUrl));
            p->setCurrentIndex(1); // Switch to web content from StartPage
            urlBar->setText(initialUrl);
        }
    }

    suggestionList = new QListWidget(this);
    suggestionList->setWindowFlags(Qt::FramelessWindowHint | Qt::Tool);
    suggestionList->setContextMenuPolicy(Qt::CustomContextMenu);
    suggestionList->setFocusPolicy(Qt::NoFocus);
    suggestionList->hide();

    connect(suggestionList, &QListWidget::customContextMenuRequested, this, &Browser::showBookmarkContextMenu);;

    connect(urlBar, &QLineEdit::textChanged, this, &Browser::updateSuggestions);

    connect(suggestionList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item){
        QString url = item->data(Qt::UserRole).toString();
        urlBar->setText(url);
        handleUrlBarSubmit();
        suggestionList->hide();
    });

    bool systemIsDark = isSystemDarkTheme();
    int initialTheme = systemIsDark ? 1 : 0;

    if (!settings->contains("theme")) {
        settings->setValue("theme", initialTheme);
    }

    int savedTheme = settings->value("theme", initialTheme).toInt();
    applyTheme(savedTheme);

    // Keyboard shortcuts
    QShortcut* fullscreenKey = new QShortcut(QKeySequence("F11"), this);
    connect(fullscreenKey, &QShortcut::activated, this, [this]() {
        if (this->isFullScreen()) {
            isFullscreen = false;

            toolbar->show();
            tabWidget->tabBar()->show();
            if (TabBar* bar = qobject_cast<TabBar*>(tabWidget->tabBar())) bar->plusButton->show();

            this->showNormal();
        } else {
            isFullscreen = true;

            toolbar->hide();
            tabWidget->tabBar()->hide();
            if (TabBar* bar = qobject_cast<TabBar*>(tabWidget->tabBar())) bar->plusButton->hide();

            this->showFullScreen();
        }
    });

    // Download cancellation logic
    QShortcut* cancelKey = new QShortcut(QKeySequence("Ctrl+C"), this);
    connect(cancelKey, &QShortcut::activated, this, [this]() {
        if (m_downloadManager->activeCount() <= 0) return;

        if (m_downloadManager->activeCount() == 1) {
            m_downloadManager->cancelDownload(0);
        } else {
            isWaitingForCancelInput = true;
            statusBar()->showMessage(QString(Localization::qget("stadus_cncl")).arg(m_downloadManager->activeCount()));
        }
    });

    // Number keys for multi-download selection
    for (int i = 1; i <= 9; ++i) {
        QShortcut* numKey = new QShortcut(QKeySequence(QString::number(i)), this);
        connect(numKey, &QShortcut::activated, this, [this, i]() {
            if (isWaitingForCancelInput) {
                int index = i - 1;
                if (index < m_downloadManager->activeCount()) {
                    m_downloadManager->cancelDownload(index);
                }
                isWaitingForCancelInput = false;
                QTimer::singleShot(100, this, [this]() { updateStatusBarContent(); });
            }
        });
    }

    // May Day event
    maydayPlayer = new QMediaPlayer(this);
    maydayAudio = new QAudioOutput(this);

    maydayPlayer->setAudioOutput(maydayAudio);
    maydayPlayer->setSource(QUrl("qrc:/mayday/a_las_barricadas.ogg"));
    maydayAudio->setVolume(0.50);

    QTimer::singleShot(0, this, [this]() {

        QDate now = QDate::currentDate();

        int lastPlayedYear = settings->value("mayday/lastPlayedYear", 0).toInt();

        if (now.month() == 5 && now.day() == 1 && lastPlayedYear != now.year()) {

            settings->setValue("mayday/lastPlayedYear", now.year());
            maydayActive = true;
            maydayPlayer->play();
            QMessageBox::information(this, Localization::qget("mayday_title"), Localization::qget("mayday_desc"));
        }
    });

    connect(maydayPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {

        if (status == QMediaPlayer::EndOfMedia) {
            maydayActive = false;
        }
    });

    qApp->installEventFilter(this);

}

void Browser::handleDownload(QWebEngineDownloadRequest* download) {
    QString path = QFileDialog::getSaveFileName(this, "Save File", download->downloadFileName());
    if (path.isEmpty()) {
        download->cancel();
        return;
    }

    download->setDownloadDirectory(QFileInfo(path).path());
    download->setDownloadFileName(QFileInfo(path).fileName());
    download->accept();

    Download* d = new Download(download, this);
    m_downloadManager->addDownload(d);

    statusBar()->setFont(QFont("Courier New", 9));
    statusBar()->show();

    connect(d, &Download::progressChanged, this, [this]() {
        if (!isWaitingForCancelInput) {
            updateStatusBarContent();
        }
    });

    connect(d, &Download::stateChanged, this, [this]() {
        updateStatusBarContent();
    });

    connect(d, &Download::finished, this, [this]() {
        QTimer::singleShot(3000, this, [this]() {
            if (m_downloadManager->activeCount() <= 0) {
                statusBar()->hide();
            }
        });
        updateStatusBarContent();
    });

    updateStatusBarContent();
}

void Browser::applyTheme(int themeIndex) {
    bool isDark = (themeIndex == 1);
    settings->setValue("theme", themeIndex);
    emit themeChanged(themeIndex);

    QPalette palette;
    if (isDark) {
        // Dark mode palette
        palette.setColor(QPalette::Window, QColor(28, 28, 28));
        palette.setColor(QPalette::WindowText, QColor(230, 230, 230));
        palette.setColor(QPalette::Base, QColor(38, 38, 38));
        palette.setColor(QPalette::AlternateBase, QColor(50, 50, 50));
        palette.setColor(QPalette::ToolTipBase, QColor(230, 230, 230));
        palette.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
        palette.setColor(QPalette::Text, QColor(230, 230, 230));
        palette.setColor(QPalette::Button, QColor(38, 38, 38));
        palette.setColor(QPalette::ButtonText, QColor(230, 230, 230));
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, QColor(65, 160, 240));
        palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        palette.setColor(QPalette::HighlightedText, QColor(230, 230, 230));
    } else {
        // Light mode palette
        palette.setColor(QPalette::Window, QColor(245, 245, 245));
        palette.setColor(QPalette::WindowText, QColor(50, 50, 50));
        palette.setColor(QPalette::Base, QColor(235, 235, 235));
        palette.setColor(QPalette::AlternateBase, QColor(225, 225, 225));
        palette.setColor(QPalette::ToolTipBase, QColor(50, 50, 50));
        palette.setColor(QPalette::ToolTipText, QColor(50, 50, 50));
        palette.setColor(QPalette::Text, QColor(50, 50, 50));
        palette.setColor(QPalette::Button, QColor(235, 235, 235));
        palette.setColor(QPalette::ButtonText, QColor(50, 50, 50));
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, QColor(65, 130, 240));
        palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
        palette.setColor(QPalette::HighlightedText, QColor(245, 245, 245));
    }

    qApp->setPalette(palette);

    // Toolbar
    toolbar->setStyleSheet(QString(R"(
        QToolBar {
            background-color: %1;
            border: none;
            padding: 0px;
        }
        QToolBar QToolButton {
            background-color: transparent;
            color: %2;
            border: none;
            font-size: 14px;
            min-width: 28px;
        }
        QToolBar QToolButton:hover {
            background-color: %3;
        }
        QToolBar QToolButton:pressed {
            background-color: %4;
        }
    )")
    .arg(isDark ? "#2c2c2c" : "#ececec")
    .arg(isDark ? "#e6e6e6" : "#333333")
    .arg(isDark ? "#3a3a3a" : "#dcdcdc")
    .arg(isDark ? "#4a4a4a" : "#c8c8c8")
    );

    // URL Bar
    urlBar->setStyleSheet(QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 5px;
            padding: 4px 8px;
            font-size: 12px;
            min-height: 20px;
            selection-background-color: #0078d4;
        }
        QLineEdit:focus {
            border: 2px solid #0078d4;
        }
    )")
    .arg(isDark ? "#3a3a3a" : "#ffffff")
    .arg(isDark ? "#e6e6e6" : "#333333")
    .arg(isDark ? "#555555" : "#cccccc")
    );

    // TabWidget
    tabWidget->setStyleSheet(QString(R"(
        QTabWidget::pane {
            border: none;
            background-color: %1;
        }
        QTabBar {
            border: none;
            qproperty-drawBase: 0;
            padding-right: 30px;
        }
        QTabBar::tab {
            background-color: %2;
            color: %3;
            border: none;
            border-right: 1px solid %4;
            padding: 4px 24px 4px 8px;
            height: 24px;
            font-size: 11px;
        }
        QTabBar::scroller {
            width: 0px;
        }
        QTabBar QToolButton {
            width: 0px;
            height: 0px;
            padding: 0px;
            margin: 0px;
        }
        QTabBar::tab:selected {
            background-color: %5;
            color: %6;
        }
        QTabBar::tab:hover:!selected {
            background-color: %7;
        }
    )")
    .arg(isDark ? "#1c1c1c" : "#eaeaea")
    .arg(isDark ? "#222222" : "#dcdcdc")
    .arg(isDark ? "#cccccc" : "#555555")
    .arg(isDark ? "#3a3a3a" : "#d0d0d0")
    .arg(isDark ? "#2c2c2c" : "#ececec")
    .arg(isDark ? "#e6e6e6" : "#333333")
    .arg(isDark ? "#262626" : "#e6e6e6")
    );

    if (suggestionList) {
        suggestionList->setStyleSheet(QString(R"(
            QListWidget {
                background-color: %1;
                border: 1px solid %2;
                border-radius: 5px;
                outline: none;
                font-size: 12px;
                padding: 2px;
            }
            QListWidget::item {
                padding: 6px 8px;
                color: %3;
                border: none;
            }
            QListWidget::item:hover {
                background-color: %4;
            }
            QListWidget::item:selected {
                background-color: %5;
                color: %6;
            }
        )")
        .arg(isDark ? "#3a3a3a" : "#ffffff")
        .arg(isDark ? "#555555" : "#cccccc")
        .arg(isDark ? "#e6e6e6" : "#333333")
        .arg(isDark ? "#4a4a4a" : "#e8e8e8")
        .arg(isDark ? "#0078d4" : "#0078d4")
        .arg(isDark ? "#ffffff" : "#ffffff"));
    }

    // Plus (New Tab) Button
    if (TabBar* bar = qobject_cast<TabBar*>(tabWidget->tabBar())) {
        if (bar->plusButton) {
            bar->plusButton->setStyleSheet(QString(R"(
                QPushButton {
                    background-color: %1;
                    color: %2;
                    border: none;
                    font-size: 14px;
                    font-weight: bold;
                }
                QPushButton:hover {
                    background-color: %3;
                }
                QPushButton:pressed {
                    background-color: %4;
                }
            )")
            .arg(isDark ? "#222222" : "#dcdcdc")
            .arg(isDark ? "#e6e6e6" : "#333333")
            .arg(isDark ? "#3a3a3a" : "#e0e0e0")
            .arg(isDark ? "#4a4a4a" : "#c8c8c8")
            );
        }
    }

    auto getScrollbarCSS = [](bool isDark) -> QString {
        if (isDark) {
            return R"(
            ::-webkit-scrollbar { width:8px; height:8px; }
            ::-webkit-scrollbar-track { background: #1c1c1c; }
            ::-webkit-scrollbar-thumb { background: #555; border-radius: 0; }
            ::-webkit-scrollbar-thumb:hover { background: #777; }
        )";
        } else {
            return R"(
            ::-webkit-scrollbar { width:8px; height:8px; }
            ::-webkit-scrollbar-track { background: #f0f0f0; }
            ::-webkit-scrollbar-thumb { background: #bbb; border-radius: 0; }
            ::-webkit-scrollbar-thumb:hover { background: #999; }
        )";
        }
    };

    // scrollbar theme script injection
    for (int i = 0; i < tabWidget->count(); ++i) {
        TabPage* page = qobject_cast<TabPage*>(tabWidget->widget(i));
        if (page) {
            page->applyTheme(isDark);

            // Remove any scrollbar scripts if they exist
            auto scripts = profile->scripts()->toList();
            for (const QWebEngineScript &s : scripts) {
                if (s.name() == "scrollbarTheme") {
                    profile->scripts()->remove(s);
                    break;
                }
            }

            QWebEngineScript script;
            script.setName("scrollbarTheme");
            script.setInjectionPoint(QWebEngineScript::DocumentReady);
            script.setRunsOnSubFrames(true);
            script.setSourceCode(QString(R"(
            var oldStyle = document.getElementById("scrollbarTheme");
            if(oldStyle) oldStyle.remove();

            var style = document.createElement('style');
            style.id = "scrollbarTheme";
            style.textContent = `%1`;
            document.documentElement.appendChild(style);
            )").arg(getScrollbarCSS(isDark)));

            profile->scripts()->insert(script);

            // Apply this within the page as well for open tabs.
            page->webView()->page()->runJavaScript(QString(R"(
            var oldStyle = document.getElementById("scrollbarTheme");
            if(oldStyle) oldStyle.remove();

            var style = document.createElement('style');
            style.id = "scrollbarTheme";
            style.textContent = `%1`;
            document.documentElement.appendChild(style);
            )").arg(getScrollbarCSS(isDark)));
        }
    }

    if (favoriteButton) {
        updateFavoriteButtonStyle();
    }
}

void Browser::createToolbar() {
    toolbar = new QToolBar();
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setContextMenuPolicy(Qt::PreventContextMenu);

    if (toolbar->layout()) {
        toolbar->layout()->setSpacing(0);
        toolbar->layout()->setContentsMargins(0, 0, 0, 0);
    }

    QAction* backAction = toolbar->addAction("←");
    QAction* forwardAction = toolbar->addAction("→");
    QAction* reloadAction = toolbar->addAction("⟳");

    urlBar = new QLineEdit(this);
    urlBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    QHBoxLayout* urlLayout = new QHBoxLayout(urlBar);
    urlLayout->setContentsMargins(5, 0, 5, 0);
    urlLayout->setSpacing(5);

    urlLayout->addStretch();

    favoriteButton = new QPushButton("★");
    favoriteButton->setFixedSize(24, 24);
    favoriteButton->setCursor(Qt::PointingHandCursor);
    favoriteButton->setStyleSheet(R"(
    QPushButton {
        background-color: transparent;
        border: none;
        font-size: 16px;
        color: #888;
    }
    QPushButton:hover {
        background-color: transparent;
        color: #888;
    }
    QPushButton:pressed {
        background-color: transparent;
        color: #888;
    }
    )");

    urlLayout->addWidget(favoriteButton);
    urlBar->setLayout(urlLayout);

    toolbar->addWidget(urlBar);

    QAction* extensionsAction = toolbar->addAction("</>");
    extensionsButton = qobject_cast<QToolButton*>(toolbar->widgetForAction(extensionsAction));

    if (extensionsButton) {
        extensionsButton->setCursor(Qt::PointingHandCursor);
        connect(extensionsButton, &QToolButton::clicked, this, &Browser::setupExtensionsButton);
    }

    QAction* settingsAction = toolbar->addAction("☰");

    int buttonHeight = 44;
    toolbar->setIconSize(QSize(buttonHeight, buttonHeight));

    for (QAction* action : toolbar->actions()) {
        if (QToolButton* btn = qobject_cast<QToolButton*>(toolbar->widgetForAction(action))) {
            btn->setFixedHeight(buttonHeight);
        }
    }

    urlBar->setFixedHeight(buttonHeight - 12);

    for (QAction* action : toolbar->actions()) {
        if (QToolButton* btn = qobject_cast<QToolButton*>(toolbar->widgetForAction(action))) {
            btn->installEventFilter(this);
        }
    }

    settingsButton = qobject_cast<QToolButton*>(toolbar->widgetForAction(settingsAction));
    if (settingsButton) {
        updateBadge = new QLabel(settingsButton);
        updateBadge->setFixedSize(10, 10);
        updateBadge->setStyleSheet(R"(
            background-color: #2ecc71;
            border-radius: 5px;
            border: 2px solid #1e2327;
        )");
        updateBadge->setAttribute(Qt::WA_TransparentForMouseEvents);
        updateBadge->setVisible(false);
        positionUpdateBadge();
    }

    connect(favoriteButton, &QPushButton::clicked, this, [this]() {
        if (TabPage* page = currentTabPage()) {
            QString url = page->webView()->url().toString();
            QString title = page->webView()->title();

            // Do not take action if the URL is empty or the start page
            if (!url.isEmpty() && !url.contains("about:blank") && page->currentIndex() == 1) {
                // Check if the same URL is already in your bookmarks
                bool exists = false;
                int bookmarkIndex = -1;
                for (int i = 0; i < bookmarks.size(); ++i) {
                    if (bookmarks[i].second == url) {
                        exists = true;
                        bookmarkIndex = i;
                        break;
                    }
                }

                if (exists) {
                    bookmarks.removeAt(bookmarkIndex);
                    saveBookmarks();
                    updateFavoriteButtonStyle();

                    favoriteButton->setStyleSheet(R"(
                    QPushButton {
                        background-color: transparent;
                        border: none;
                        font-size: 16px;
                        color: #888;
                    }
                    QPushButton:hover {
                        background-color: transparent;
                        color: #888;
                    }
                    QPushButton:pressed {
                        background-color: transparent;
                        color: #888;
                    }
                    )");
                } else {
                    bookmarks.append({title, url});
                    saveBookmarks();
                    updateFavoriteButtonStyle();

                    favoriteButton->setStyleSheet(R"(
                    QPushButton {
                        background-color: transparent;
                        border: none;
                        font-size: 16px;
                        color: #ffaa00;
                    }
                    QPushButton:hover {
                        background-color: transparent;
                        color: #ffaa00;
                    }
                    QPushButton:pressed {
                        background-color: transparent;
                        color: #ffaa00;
                    }
                    )");
                }

                QTimer::singleShot(2000, this, [this]() {
                    if (favoriteButton) {
                        updateFavoriteButtonStyle();
                    }
                });
            }
        }
    });

    connect(backAction, &QAction::triggered, this, [this]() {
        if (auto* p = currentTabPage()) p->goBack();
    });

        connect(forwardAction, &QAction::triggered, this, [this]() {
            if (auto* p = currentTabPage()) p->goForward();
        });

            connect(reloadAction, &QAction::triggered, this, [this]() {
                if (auto* p = currentTabPage()) p->webView()->reload();
            });

                connect(urlBar, &QLineEdit::returnPressed, this, &Browser::handleUrlBarSubmit);

                connect(settingsAction, &QAction::triggered, this, [this]() {
                    if (updateBadge) updateBadge->setVisible(false);

                    SettingsDialog* dlg = new SettingsDialog(profile, this);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->setUpdateDownloadUrl(m_pendingUpdateDownloadUrl, m_pendingUpdateVersion);

                    connect(dlg, &SettingsDialog::cookieDeleted, this, &Browser::deleteCookie);
                    connect(dlg, &SettingsDialog::themeChanged, this, &Browser::applyTheme);
                    connect(this, &Browser::themeChanged, dlg, [dlg](int index) {
                        dlg->updateTheme(index);
                    });

                    connect(dlg, &SettingsDialog::searchEngineChanged, this, [this](const QString &engine){
                        currentSearchEngine = engine;
                        settings->setValue("searchEngine", engine);
                        searchEngines.clear();
                        searchEngines["DuckDuckGo"] = "https://duckduckgo.com/?q=%1";

                        QStringList engines = settings->value("customSearchEngines").toStringList();
                        for(const QString &engineEntry : engines) {
                            QStringList parts = engineEntry.split("|");
                            if(parts.size() == 2) {
                                QString name = parts[0];
                                QString url = parts[1];
                                url.replace("%s", "%1");
                                searchEngines[name] = url;
                            }
                        }

                        QString templateUrl = searchEngines.value(engine, "https://duckduckgo.com/?q=%1");
                        for(int i = 0; i < tabWidget->count(); i++) {
                            TabPage* page = qobject_cast<TabPage*>(tabWidget->widget(i));
                            if(page) {
                                page->getStartPage()->setSearchTemplate(templateUrl);
                            }
                        }
                    });

                    connect(dlg, &SettingsDialog::adBlockToggled, this, [this](bool enabled){
                        setAdBlockEnabled(enabled);
                    });

                    dlg->open();
                });
}

void Browser::positionUpdateBadge() {
    if (!settingsButton || !updateBadge) return;

    int x = settingsButton->width() - updateBadge->width() - 4;
    int y = settingsButton->height() - updateBadge->height() - 8;
    updateBadge->move(x, y);
    updateBadge->raise();
}

void Browser::handleTabChange(int index) {
    if (!renderController) return;
    if (index == -1) return;
    TabPage* page = qobject_cast<TabPage*>(tabWidget->widget(index));
    if (page) {
        urlBar->setText(page->currentUrl().toString());
        urlBar->setCursorPosition(0);

        checkAndUpdateFavoriteButton();

        if (renderController) {
            renderController->enable(true);
            renderController->request();
        }
    }
}

void Browser::updateFavoriteButtonVisibility() {
    if (!favoriteButton) return;

    TabPage* page = currentTabPage();
    if (page) {
        if (page->currentIndex() == 0) {
            favoriteButton->hide();
        } else {
            favoriteButton->show();
            updateFavoriteButtonStyle();
        }
    }
}

void Browser::checkAndUpdateFavoriteButton() {
    if (!favoriteButton) return;

    TabPage* page = currentTabPage();
    if (page) {
        if (page->currentIndex() == 0) {
            favoriteButton->hide();
        } else {
            favoriteButton->show();
            updateFavoriteButtonStyle();
        }
    }
}

void Browser::updateFavoriteButtonStyle() {
    if (!favoriteButton) return;

    TabPage* page = currentTabPage();
    if (page && page->currentIndex() == 1) {
        QString currentUrl = page->webView()->url().toString();

        bool isBookmarked = false;
        for (const auto& bookmark : bookmarks) {
            if (bookmark.second == currentUrl) {
                isBookmarked = true;
                break;
            }
        }

        if (isBookmarked) {
            favoriteButton->setStyleSheet(R"(
                QPushButton {
                    background-color: transparent;
                    border: none;
                    font-size: 16px;
                    color: #ffaa00;
                }
                QPushButton:hover {
                    background-color: transparent;
                    color: #ffaa00;
                }
                QPushButton:pressed {
                    background-color: transparent;
                    color: #ffaa00;
                }
            )");
        } else {
            favoriteButton->setStyleSheet(R"(
                QPushButton {
                    background-color: transparent;
                    border: none;
                    font-size: 16px;
                    color: #888;
                }
                QPushButton:hover {
                    background-color: transparent;
                    color: #888;
                }
                QPushButton:pressed {
                    background-color: transparent;
                    color: #888;
                }
            )");
        }
    }
}

void Browser::addNewTab() {
    TabPage* page = new TabPage(profile);
    page->webView()->page()->setBackgroundColor(Qt::transparent);
    page->webView()->setAttribute(Qt::WA_TranslucentBackground);

    page->webView()->installEventFilter(this);
    if (page->webView()->focusProxy()) {
        page->webView()->focusProxy()->installEventFilter(this);
    }

    page->getStartPage()->setSearchTemplate(
        searchEngines.value(currentSearchEngine, "https://duckduckgo.com/?q=%1")
    );

    int currentTheme = settings->value("theme", 0).toInt();
    bool isDark = (currentTheme == 1);
    page->applyTheme(isDark);

    int index = tabWidget->addTab(page, QIcon(":/nulla_icon.png"), Localization::qget("new_tab"));

    TabBar* bar = qobject_cast<TabBar*>(tabWidget->tabBar());
    if (bar) {
        connect(bar, &TabBar::reloadTabRequested, this, [this](int index) {
            if (TabPage* page = qobject_cast<TabPage*>(tabWidget->widget(index))) {
                page->webView()->reload();
            }
        });

        connect(bar, &TabBar::muteTabRequested, this, [this](int index, bool shouldMute) {
            if (TabPage* page = qobject_cast<TabPage*>(tabWidget->widget(index))) {
                page->webView()->page()->setAudioMuted(shouldMute);
            }
        });

        bar->setElideMode(Qt::ElideRight);
    }

    connect(page->webView()->page(), &QWebEnginePage::permissionRequested, this, [this](QWebEnginePermission permission) {

        QString origin = permission.origin().host();
        auto type = permission.permissionType();

        QString key = QString("permissions/%1/%2")
        .arg(origin)
        .arg((int)type);

        if (settings->contains(key)) {
            bool allowed = settings->value(key).toBool();
            if (allowed) permission.grant();
            else permission.deny();
            return;
        }

        QString featureName;

        switch (type) {
            case QWebEnginePermission::PermissionType::MediaAudioCapture:
                featureName = Localization::qget("mic");
                break;
            case QWebEnginePermission::PermissionType::MediaVideoCapture:
                featureName = Localization::qget("cam");
                break;
            case QWebEnginePermission::PermissionType::MediaAudioVideoCapture:
                featureName = Localization::qget("cam_mic");
                break;
            case QWebEnginePermission::PermissionType::DesktopVideoCapture:
                featureName = Localization::qget("screen");
                break;
            case QWebEnginePermission::PermissionType::DesktopAudioVideoCapture:
                featureName = Localization::qget("screen_audio");
                break;
            default:
                featureName = Localization::qget("unknown_feat");
                break;
        }

        QMessageBox msgBox(this);
        msgBox.setWindowTitle(Localization::qget("perm_title"));
        msgBox.setText(QString(Localization::qget("perm_desc")).arg(origin).arg(featureName));

        QCheckBox* rememberBox = new QCheckBox(Localization::qget("remember_perm"));
        rememberBox->setChecked(false);
        msgBox.setCheckBox(rememberBox);

        QPushButton* allowBtn = msgBox.addButton(Localization::qget("allow"), QMessageBox::AcceptRole);
        msgBox.addButton(Localization::qget("deny"), QMessageBox::RejectRole);

        msgBox.exec();

        bool allowed = (msgBox.clickedButton() == allowBtn);

        if (allowed) permission.grant();
        else permission.deny();

        if (rememberBox->isChecked()) {
            settings->setValue(key, allowed);
        }
    });

    connect(page->webView()->page(), &QWebEnginePage::fullScreenRequested, this, [this](QWebEngineFullScreenRequest request) {
        request.accept();

        if (request.toggleOn()) {
            toolbar->hide();
            tabWidget->tabBar()->hide();
            if (TabBar* bar = qobject_cast<TabBar*>(tabWidget->tabBar())) bar->plusButton->hide();
            showFullScreen();
        } else {
            toolbar->show();
            tabWidget->tabBar()->show();
            if (TabBar* bar = qobject_cast<TabBar*>(tabWidget->tabBar())) bar->plusButton->show();
            showNormal();
        }
    });

    connect(page->webView()->page(), &QWebEnginePage::linkHovered, this, [this](const QString &url) {
        if (url.isEmpty()) {
            statusBar()->clearMessage();

            if (m_downloadManager->activeCount() > 0) {
                updateStatusBarContent();
            } else {
                statusBar()->hide();
            }
        } else {
            if (!isWaitingForCancelInput) {
                statusBar()->setFont(QFont("Courier New", 9));
                statusBar()->show();
                statusBar()->showMessage(url);
            }
        }
    });

    connect(page, &TabPage::titleChanged, this, [this, page](const QString& t) {
        int i = tabWidget->indexOf(page);
        if (i != -1) {
            QString title = t.isEmpty() ? Localization::qget("new_tab") : t;
            tabWidget->setTabText(i, title);
        }
    });

    connect(page, &TabPage::urlChanged, this, [this, page](const QUrl& u) {
        if (tabWidget->currentWidget() == page) {
            urlBar->setText(u.toString());
            urlBar->setCursorPosition(0);
            checkAndUpdateFavoriteButton();
        }
        if (renderController) {
            renderController->enable(true);
            renderController->request();
        }
    });

    connect(page->getStartPage(), &StartPage::focusUrlBarAndType, this, [this](const QString& text) {
        if (isFullscreen) {
            toolbar->show();
        }
        urlBar->setFocus();
        urlBar->insert(text);
        urlBar->end(false);
    });

    connect(page->getStartPage(), &StartPage::forwardKeyToUrlBar, this, [this](int key) {
        if (urlBar->hasFocus()) {
            return;
        }
        urlBar->setFocus();

        QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier);
        QCoreApplication::postEvent(urlBar, event);
    });

    connect(page, &TabPage::iconChanged, this, [this, page](const QIcon &icon) {
        int i = tabWidget->indexOf(page);
        if (i != -1) {
            if (!icon.isNull()) {
                tabWidget->setTabIcon(i, icon);
            } else {
                tabWidget->setTabIcon(i, QIcon(":/nulla_icon.png"));
            }
        }
    });

    connect(page, &TabPage::currentChanged, this, [this](int index) {
        Q_UNUSED(index);
        checkAndUpdateFavoriteButton();
    });

    QTimer::singleShot(0, this, [this]() {
        checkAndUpdateFavoriteButton();
    });

    tabWidget->setCurrentIndex(index);
    urlBar->clear();
}

void Browser::handleUrlBarSubmit() {
    QString text = urlBar->text().trimmed();
    if (text.isEmpty()) return;

    QUrl url;

    if (text.startsWith("http://") || text.startsWith("https://") ||
        QHostAddress(text).protocol() != QAbstractSocket::UnknownNetworkLayerProtocol ||
        text == "localhost") {
        url = QUrl::fromUserInput(text);
        } else {
            if (text.contains(".") && !text.contains(" ")) {
                url = QUrl::fromUserInput(text);
            } else {
                QString templateUrl = searchEngines.value(
                    currentSearchEngine,
                    "https://duckduckgo.com/?q=%1"
                );
                url = QUrl(templateUrl.arg(QString(QUrl::toPercentEncoding(text))));
            }
        }

        if (url.isValid() && url.scheme().startsWith("http") && url.path().isEmpty()) {
            url.setPath("/");
        }

        if (auto* p = currentTabPage()) {
            p->webView()->load(url);
            p->setCurrentIndex(1);

            urlBar->setText(url.toDisplayString(QUrl::PreferLocalFile | QUrl::RemoveUserInfo));
            urlBar->setCursorPosition(0);
        }
}

void Browser::closeTab(int index) {

    if (index == 0 && maydayActive) {
        maydayPlayer->stop();
        maydayActive = false;
    }

    if (tabWidget->count() > 1) {
        QWidget* w = tabWidget->widget(index);
        tabWidget->removeTab(index);
        w->deleteLater();
    } else {
        close();
    }
}

TabPage* Browser::currentTabPage() {
    return qobject_cast<TabPage*>(tabWidget->currentWidget());
}


bool Browser::isSystemDarkTheme() {
    // Cross-platform check for system-wide dark mode.
    // On Windows, it queries the registry; on other platforms, it analyzes the application palette's lightness.

    #ifdef Q_OS_WIN
    QSettings registry("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                       QSettings::NativeFormat);
    int value = registry.value("AppsUseLightTheme", 1).toInt();
    return (value == 0);
    #else
    QPalette palette = qApp->palette();
    return palette.window().color().lightness() < 128;
    #endif
}

void Browser::updateStatusBarContent() {
    if (isWaitingForCancelInput) return;

    QString currentMsg = statusBar()->currentMessage();
    bool isShowingUrl = !currentMsg.isEmpty() &&
    (currentMsg.contains("://") ||
    currentMsg.startsWith("javascript:") ||
    currentMsg.contains("www.") ||
    currentMsg.startsWith("mailto:"));

    if (isShowingUrl) return;

    QString statusText = m_downloadManager->statusText();

    if (statusText.isEmpty()) {
        statusBar()->clearMessage();
        statusBar()->hide();
    } else {
        statusBar()->show();
        statusBar()->showMessage(statusText);
    }
}

void Browser::closeEvent(QCloseEvent* event) {
    if(profile) {
        profile->clearAllVisitedLinks();
    }
    QMainWindow::closeEvent(event);
}

void Browser::mousePressEvent(QMouseEvent* event) {
    if (isFullscreen) {
        QWidget* clickedWidget = QApplication::widgetAt(event->globalPosition().toPoint());
        if (clickedWidget != urlBar && !urlBar->isAncestorOf(clickedWidget)) {
            toolbar->hide();
        }
    }

    QWidget* focusedWidget = QApplication::focusWidget();
    if (focusedWidget && qobject_cast<QLineEdit*>(focusedWidget)) {
        focusedWidget->clearFocus();
    }
    QMainWindow::mousePressEvent(event);
}

void Browser::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);

    positionUpdateBadge();

    if (suggestionList && suggestionList->isVisible() && urlBar) {
        suggestionList->setFixedWidth(urlBar->width());
        suggestionList->move(urlBar->mapToGlobal(QPoint(0, urlBar->height())));
    }
}

void Browser::moveEvent(QMoveEvent *event) {
    QMainWindow::moveEvent(event);

    if (suggestionList && suggestionList->isVisible() && urlBar) {
        suggestionList->move(urlBar->mapToGlobal(QPoint(0, urlBar->height())));
    }
}

void Browser::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);

}

bool Browser::eventFilter(QObject* obj, QEvent* ev) {
    if (ev->type() == QEvent::ToolTip)
        return true;

    if (ev->type() == QEvent::MouseButtonPress) {
        QMouseEvent* me = static_cast<QMouseEvent*>(ev);
        QPoint globalPos = me->globalPosition().toPoint();

        if (suggestionList && suggestionList->isVisible()) {
            QWidget* clicked = QApplication::widgetAt(globalPos);

            bool clickedInsideSuggestion =
            clicked == suggestionList ||
            suggestionList->isAncestorOf(clicked);

            bool clickedUrlBar =
            clicked == urlBar || urlBar->isAncestorOf(clicked);

            if (!clickedInsideSuggestion && !clickedUrlBar) {
                suggestionList->hide();
            }
        }
    }

    if (obj == urlBar) {
        if (ev->type() == QEvent::FocusIn) {
            if (!urlBar->text().isEmpty()) {
                updateSuggestions(urlBar->text());
            }
        }
    }

    return QMainWindow::eventFilter(obj, ev);
}

void Browser::openUrlInNewTab(const QString &urlStr) {
    if (urlStr.isEmpty()) {
        addNewTab();
    } else {
        addNewTab();

        if (auto* p = currentTabPage()) {
            p->webView()->load(QUrl::fromUserInput(urlStr));
            p->setCurrentIndex(1);
            urlBar->setText(urlStr);
        }
    }
}

void Browser::saveCookiesToJson() {
    QJsonArray array;

    for (const QNetworkCookie &cookie : cookieCache) {
        QJsonObject obj;

        obj["name"] = QString(cookie.name());
        obj["value"] = QString(cookie.value());
        obj["domain"] = cookie.domain();
        obj["path"] = cookie.path();
        obj["secure"] = cookie.isSecure();
        obj["httpOnly"] = cookie.isHttpOnly();
        obj["expiration"] = cookie.expirationDate().toSecsSinceEpoch();

        array.append(obj);
    }

    QJsonDocument doc(array);

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    + "/cookies.json";

        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
        }
}

void Browser::loadCookiesFromJson() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cookies.json";
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QJsonArray array = doc.array();
    auto *store = profile->cookieStore();

    for (const QJsonValue &val : array) {
        QJsonObject obj = val.toObject();

        QNetworkCookie cookie;
        cookie.setName(obj["name"].toString().toUtf8());
        cookie.setValue(obj["value"].toString().toUtf8());
        cookie.setDomain(obj["domain"].toString());
        cookie.setPath(obj["path"].toString());
        cookie.setSecure(obj["secure"].toBool());
        cookie.setHttpOnly(obj["httpOnly"].toBool());

        qint64 exp = obj["expiration"].toVariant().toLongLong();
        if (exp > 0)
            cookie.setExpirationDate(QDateTime::fromSecsSinceEpoch(exp));

        QString domain = obj["domain"].toString();
        QString host = domain.startsWith('.') ? domain.mid(1) : domain;
        QString scheme = obj["secure"].toBool() ? "https://" : "http://";

        QString urlString = scheme + host;
        QUrl url(urlString);
        if (url.isValid()) {
            store->setCookie(cookie, url);
        } else {
            qWarning() << "Invalid URL for cookie:" << urlString;
        }
    }
}

void Browser::setAdBlockEnabled(bool enabled) {
    adBlocker->setEnabled(enabled);

    QList<QWebEngineScript> existing = profile->scripts()->find("ytAdBlock");

    if (enabled) {
        if (existing.isEmpty()) {
            QFile ytAdBlock(":/scripts/ytAdBlock.js");
            if (ytAdBlock.open(QIODevice::ReadOnly)) {
                QByteArray scriptCode = ytAdBlock.readAll();

                QWebEngineScript ytAB;
                ytAB.setName("ytAdBlock");
                ytAB.setInjectionPoint(QWebEngineScript::DocumentCreation);
                ytAB.setRunsOnSubFrames(true);
                ytAB.setWorldId(QWebEngineScript::MainWorld);
                ytAB.setSourceCode(QString::fromUtf8(scriptCode));

                profile->scripts()->insert(ytAB);
            }
        }
    } else {
        for (const QWebEngineScript &s : existing) {
            profile->scripts()->remove(s);
        }
    }
}

void Browser::showSettings() {
    SettingsDialog *dialog = new SettingsDialog(profile, this);

    connect(dialog, &SettingsDialog::cookieDeleted, this, &Browser::deleteCookie);
    connect(dialog, &SettingsDialog::adBlockToggled, this, [this](bool enabled){
        adBlocker->setEnabled(enabled);
    });

    dialog->setUpdateDownloadUrl(m_pendingUpdateDownloadUrl, m_pendingUpdateVersion);

    dialog->show();
}

void Browser::onUpdateAvailable(const QString &version, const QString &url, const QString &notes, const QString &downloadUrl) {
    Q_UNUSED(notes);
    Q_UNUSED(url);

    m_pendingUpdateDownloadUrl = downloadUrl;
    m_pendingUpdateVersion = version;

    QString lastNotified = settings->value("lastNotifiedUpdateVersion").toString();
    bool alreadySeen = (lastNotified == version);

    if (updateBadge && settingsButton) {
        updateBadge->setVisible(!alreadySeen);
        positionUpdateBadge();
        settingsButton->setToolTip(Localization::qget("update_available_tooltip").arg(version));
    }

    if (!alreadySeen) {
        settings->setValue("lastNotifiedUpdateVersion", version);

        if (QApplication::activeModalWidget() != nullptr) {
            return;
        }

        QMessageBox::information(this,
        Localization::qget("update_notify_title"),
        Localization::qget("update_notify_text").arg(version));
    }
}

void Browser::onUpdateCheckFailed(const QString &error) {
    qWarning() << "Update check failed:" << error;
}

void Browser::deleteCookie(const QString &domain, const QString &name) {
    for(int i = 0; i < cookieCache.size(); ++i) {
        if(cookieCache[i].domain() == domain && cookieCache[i].name() == name) {
            cookieCache.removeAt(i);
            break;
        }
    }

    QNetworkCookie dummy;
    dummy.setName(name.toUtf8());
    dummy.setDomain(domain);
    profile->cookieStore()->deleteCookie(dummy);

    saveCookiesToJson();
}
void Browser::updateSuggestions(const QString &text)
{
    if (!suggestionList) return;

    suggestionList->clear();

    if (!urlBar->hasFocus() || text.isEmpty()) {
        suggestionList->hide();
        return;
    }

    for (const auto &pair : bookmarks) {
        const QString &title = pair.first;
        const QString &url = pair.second;

        if (url == text || title == text) {
            continue;
        }

        if (title.contains(text, Qt::CaseInsensitive) ||
            url.contains(text, Qt::CaseInsensitive)) {

            QString displayText = "★ " + title + " - " + url;
        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, url);
        suggestionList->addItem(item);
            }
    }

    if (suggestionList->count() > 0) {
        suggestionList->setFixedWidth(urlBar->width());

        int listHeight = suggestionList->sizeHintForRow(0) * qMin(suggestionList->count(), 8) + 5;
        listHeight = qMin(listHeight, 300);
        suggestionList->setFixedHeight(listHeight);

        QPoint pos = urlBar->mapToGlobal(QPoint(0, urlBar->height()));
        suggestionList->move(pos);
        suggestionList->show();
        suggestionList->raise();
    } else {
        suggestionList->hide();
    }
}

void Browser::saveBookmarks() {
    QJsonArray arr;
    for(const auto& bm : bookmarks) {
        QJsonObject obj;
        obj["title"] = bm.first;
        obj["url"] = bm.second;
        arr.append(obj);
    }

    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);

    QString path = dataPath + "/bookmarks.json";
    QFile f(path);
    if(f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(arr).toJson());
        f.close();
    }
}

void Browser::loadBookmarks() {
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);

    QString path = dataPath + "/bookmarks.json";
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(f.readAll()).array();

    for(const auto& v : arr) {
        QJsonObject obj = v.toObject();
        bookmarks.append({obj["title"].toString(), obj["url"].toString()});
    }
}

void Browser::showBookmarkContextMenu(const QPoint& pos) {
    QListWidgetItem* item = suggestionList->itemAt(pos);
    if (item) {
        suggestionList->setCurrentItem(item);
        bookmarkContextMenu->exec(suggestionList->mapToGlobal(pos));
    }
}

void Browser::removeBookmark(const QString& url) {
    for (int i = 0; i < bookmarks.size(); ++i) {
        if (bookmarks[i].second == url) {
            bookmarks.removeAt(i);
            break;
        }
    }

    saveBookmarks();

    if (suggestionList->isVisible()) {
        updateSuggestions(urlBar->text());
    }

    updateFavoriteButtonStyle();
}

bool Browser::isExtensionEnabled(const QString &extId) const {
    return !settings->value("extensions/disabled/" + extId, false).toBool();
}

void Browser::loadExtensions() {
    QWebEngineExtensionManager *extMgr = profile->extensionManager();
    const auto installed = extMgr->extensions();

    settings->beginGroup("extensions/extIdToNativeId");
    const QStringList knownExtIds = settings->childKeys();
    settings->endGroup();

    for (const QString &extId : knownExtIds) {
        const QString nativeId = settings->value("extensions/extIdToNativeId/" + extId).toString();
        bool shouldBeEnabled = isExtensionEnabled(extId);

        for (const QWebEngineExtensionInfo &info : installed) {
            if (info.id() == nativeId) {
                extMgr->setExtensionEnabled(info, shouldBeEnabled);
                break;
            }
        }
    }
}

void Browser::setExtensionEnabled(const QString &extId, bool enabled) {
    settings->setValue("extensions/disabled/" + extId, !enabled);

    const QString nativeId = settings->value("extensions/extIdToNativeId/" + extId).toString();
    if (nativeId.isEmpty()) return;

    QWebEngineExtensionManager *extMgr = profile->extensionManager();
    for (const QWebEngineExtensionInfo &info : extMgr->extensions()) {
        if (info.id() == nativeId) {
            extMgr->setExtensionEnabled(info, enabled);
            break;
        }
    }
}

void Browser::setupExtensionsButton() {
    ExtensionStore *store = new ExtensionStore(this);
    store->setAttribute(Qt::WA_DeleteOnClose);

    connect(store, &ExtensionStore::extensionInstallRequested, this,
        [this](const QString &zipPath, const QString &extId,
                const QString &name, const QString &description,
                const QString &version, const QString &author) {
            Q_UNUSED(name); Q_UNUSED(description);
            Q_UNUSED(version); Q_UNUSED(author);

            QWebEngineExtensionManager *extMgr = profile->extensionManager();

            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = connect(extMgr, &QWebEngineExtensionManager::installFinished, this,
                [this, zipPath, extId, conn](const QWebEngineExtensionInfo &info) {
                    disconnect(*conn);
                    QFile::remove(zipPath);

                    if (info.isLoaded()) {
                        settings->setValue("extensions/extIdToNativeId/" + extId, info.id());
                        QMessageBox::information(this, "NullA Browser",
                            Localization::qget("extension_install_success"));
                    } else {
                        QMessageBox::warning(this, "NullA Browser", info.error());
                    }
            });

            extMgr->installExtension(zipPath);
    });

    connect(store, &ExtensionStore::extensionUninstallRequested, this,
        [this](const QString &extId) {
            const QString nativeId = settings->value("extensions/extIdToNativeId/" + extId).toString();
            QWebEngineExtensionManager *extMgr = profile->extensionManager();

            for (const QWebEngineExtensionInfo &info : extMgr->extensions()) {
                if (info.id() == nativeId) {
                    extMgr->uninstallExtension(info);
                    break;
                }
            }

            settings->remove("extensions/extIdToNativeId/" + extId);
            QMessageBox::information(this, "NullA Browser", Localization::qget("extension_removed"));
    });

    connect(store, &ExtensionStore::extensionToggleRequested, this,
            [this](const QString &extId, bool enabled) {
                setExtensionEnabled(extId, enabled);
    });

    store->exec();
}
