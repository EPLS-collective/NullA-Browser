/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef BROWSER_H
#define BROWSER_H

#include <QMainWindow>
#include <QWebEngineProfile>
#include <QWebEngineDownloadRequest>
#include <QSettings>
#include <QMap>
#include <QTimer>
#include <QMenu>
#include <QWebEngineCookieStore>
#include <QListWidget>
#include <QNetworkCookie>
#include <QStatusBar>
#include <QFileDialog>
#include <QRandomGenerator>
#include "TabWidget.h"
#include "TabPage.h"
#include "SettingsDialog.h"
#include "DownloadManager.h"
#include "UpdateChecker.h"
#include "ExtensionStore.h"
#include "Render.h"
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QToolBar>
#include <QHash>
#include <QWebEnginePage>
#include <QJsonObject>

class Interceptor;

class Browser : public QMainWindow {
    Q_OBJECT
public:
    explicit Browser(const QString &initialUrl = QString());
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void addNewTab();
    void openUrlInNewTab(const QString &urlStr);
    TabPage* currentTabPage();
public slots:
    void showSettings();
    void deleteCookie(const QString &domain, const QString &name);
    void updateSuggestions(const QString& text);

signals:
    void themeChanged(int index);

protected:
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* e) override;
    void showEvent(QShowEvent* e) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void handleDownload(QWebEngineDownloadRequest* download);
    void applyTheme(int themeIndex);
    void handleTabChange(int index);
    void handleUrlBarSubmit();
    void closeTab(int index);
    void updateFavoriteButtonVisibility();
    void checkAndUpdateFavoriteButton();
    void onUpdateAvailable(const QString &version, const QString &url, const QString &notes, const QString &downloadUrl);
    void onUpdateCheckFailed(const QString &error);

private:
    bool isFullscreen = false;
    void createToolbar();
    void updatePlusButtonPosition();
    bool isSystemDarkTheme();
    QString getTerminalProgress(qint64 received, qint64 total);

    QMap<QWebEngineDownloadRequest*, QString> activeDownloadMessages;
    bool isWaitingForCancelInput = false;
    void updateStatusBarContent();

    DownloadManager* m_downloadManager;

    TabWidget* tabWidget;
    QLineEdit* urlBar;
    QToolBar* toolbar;
    QWebEngineProfile* profile;
    Interceptor* adBlocker;
    QPushButton* plusButton;
    QSettings* settings;
    QListWidget* suggestionList;
    QList<QPair<QString, QString>> bookmarks;
    bool maydayActive = false;
    QMediaPlayer* maydayPlayer;
    QAudioOutput* maydayAudio;

    QList<QNetworkCookie> cookieCache;

    void saveCookiesToJson();
    void loadCookiesFromJson();

    void saveBookmarks();
    void loadBookmarks();

    QPushButton* favoriteButton = nullptr;
    void updateFavoriteButtonStyle();
    QMenu* bookmarkContextMenu;
    void showBookmarkContextMenu(const QPoint& pos);
    void removeBookmark(const QString& url);

    RenderController* renderController = nullptr;

    UpdateChecker* m_updateChecker = nullptr;
    QTimer* updateCheckTimer = nullptr;
    QToolButton* settingsButton = nullptr;
    QLabel* updateBadge = nullptr;
    void positionUpdateBadge();
    QString m_pendingUpdateDownloadUrl;
    QString m_pendingUpdateVersion;

    QMap<QString, QString> searchEngines;
    QString currentSearchEngine;

    void loadExtensions();
    void setExtensionEnabled(const QString &extId, bool enabled);
    bool isExtensionEnabled(const QString &extId) const;
    void setupExtensionsButton();
    QToolButton* extensionsButton = nullptr;

    void setAdBlockEnabled(bool enabled);

    void refreshCosmeticGenericScript();
    void applyCosmeticFiltersForPage(TabPage* page, const QString &host);

    void loadExtensionScripts(const QString &extId);
    QString chromePolyfillFor(const QString &extId, bool isBackground = false) const;
    void unloadExtensionScripts(const QString &extId);
    void extractZip(const QString &zipPath, const QString &destDir);

    void loadExtensionBackground(const QString &extId, const QString &extPath, const QJsonObject &manifestJson);
    void unloadExtensionBackground(const QString &extId);

    QString queryTabsMatching(const QString &urlPattern) const;
    bool executeExtensionScriptInTab(int tabId, const QString &extId, const QString &fileName);

    QHash<QString, QStringList> m_extensionScriptNames;
    QHash<QString, QWebEnginePage*> m_backgroundPages;
};

#endif // BROWSER_H
