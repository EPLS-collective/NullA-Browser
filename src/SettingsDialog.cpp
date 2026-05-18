/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include <QApplication>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QListWidget>
#include <QWebEngineCookieStore>
#include <QNetworkCookie>
#include <QAbstractItemView>
#include <QWebEngineSettings>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "../include/SettingsDialog.h"
#include "../include/Localization.h"

SettingsDialog::SettingsDialog(QWebEngineProfile* profile, QWidget* parent)
: QDialog(parent), m_profile(profile) {

    setWindowTitle(Localization::qget("nulla_setting"));
    setFixedWidth(380);
    setMinimumHeight(500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0); // Remove margins from window

    auto addDescription = [&](const QString &text, QVBoxLayout* parentLayout) {
        QLabel* descLabel = new QLabel(text);
        descLabel->setWordWrap(true);
        parentLayout->addWidget(descLabel);
    };

    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");

    QWidget* scrollContent = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(15);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    // Search Engine Section
    QLabel* searchTitle = new QLabel(Localization::qget("search_engine_title"));
    searchTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 5px; background: none;");

    QHBoxLayout* searchLayout = new QHBoxLayout();
    QLabel* searchLabel = new QLabel(Localization::qget("engine_label"));
    searchLabel->setFixedWidth(60);
    searchLabel->setStyleSheet("background: none;");

    settings = new QSettings("NullA", "Browser", this);

    searchCombo = new QComboBox();
    searchCombo->addItems({"DuckDuckGo"});

    QStringList engines = settings->value("customSearchEngines").toStringList();

    for(const QString &engine : engines)
    {
        QStringList parts = engine.split("|");
        if(parts.size() == 2)
            searchCombo->addItem(parts[0]);
    }

    QString savedEngine = settings->value("searchEngine", "DuckDuckGo").toString();
    int index = searchCombo->findText(savedEngine);
    if(index != -1) searchCombo->setCurrentIndex(index);

    connect(searchCombo, &QComboBox::currentTextChanged, this, &SettingsDialog::searchEngineChanged);

    connect(searchCombo, &QComboBox::currentTextChanged, this, [this](const QString &engine){
        settings->setValue("searchEngine", engine);
    });

    QPushButton* addEngineBtn = new QPushButton(Localization::qget("add_engine"));
    addEngineBtn->setMinimumHeight(35);

    connect(addEngineBtn, &QPushButton::clicked, this, &SettingsDialog::addSearchEngine);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchCombo, 1);

    searchCombo->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(searchCombo, &QComboBox::customContextMenuRequested, this, [this](const QPoint &pos){
        QMenu menu;
        QAction* deleteAction = menu.addAction(Localization::qget("delete_engine"));

        QAction* selectedAction = menu.exec(searchCombo->mapToGlobal(pos));
        if(selectedAction == deleteAction) {
            int index = searchCombo->currentIndex();
            if(index == -1) return;

            QString engine = searchCombo->currentText();

            if(engine == "DuckDuckGo") {
                QMessageBox::information(this, Localization::qget("cannot_delete"), Localization::qget("cannot_delete_desc"));
                return;
            }

            searchCombo->removeItem(index);

            QStringList engines = settings->value("customSearchEngines").toStringList();
            for(int i = 0; i < engines.size(); ++i) {
                QStringList parts = engines[i].split("|");
                if(parts[0] == engine) {
                    engines.removeAt(i);
                    break;
                }
            }
            settings->setValue("customSearchEngines", engines);
        }
    });

    layout->addWidget(searchTitle);
    addDescription(Localization::qget("engine_desc"), layout);
    layout->addLayout(searchLayout);
    layout->addWidget(addEngineBtn);

    // Theme Selection
    QLabel* themeTitle = new QLabel(Localization::qget("appearance_title"));
    themeTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 5px; background: none;");

    QHBoxLayout* themeLayout = new QHBoxLayout();
    themeLayout->setContentsMargins(0, 5, 0, 5);
    QLabel* themeLabel = new QLabel(Localization::qget("theme_label"));
    themeLabel->setFixedWidth(60);
    themeLabel->setStyleSheet("background: none;");
    themeCombo = new QComboBox();
    themeCombo->addItems({Localization::qget("light"), Localization::qget("dark")});
    themeCombo->setMinimumHeight(30);

    themeLayout->addWidget(themeLabel);
    themeLayout->addWidget(themeCombo, 1);

    layout->addWidget(themeTitle);
    addDescription(Localization::qget("theme_desc"), layout);
    layout->addLayout(themeLayout);

    QLabel* languageTitle = new QLabel(Localization::qget("language_title"));
    languageTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 5px; background: none;");

    QHBoxLayout* languageLayout = new QHBoxLayout();
    QLabel* languageLabel = new QLabel(Localization::qget("language_label"));
    languageLabel->setFixedWidth(65);
    languageLabel->setStyleSheet("background: none;");

    QComboBox* languageCombo = new QComboBox();
    languageCombo->setMinimumHeight(30);

    languageCombo->addItem("English", "en");
    languageCombo->addItem("Türkçe", "tr");

    QString savedLang = QString::fromStdString(Localization::currentLanguage());

    int langIndex = languageCombo->findData(savedLang);
    if(langIndex != -1)
        languageCombo->setCurrentIndex(langIndex);

    connect(languageCombo, &QComboBox::currentIndexChanged, this, [this, languageCombo](int i) {

        QString lang = languageCombo->itemData(i).toString();

        QSettings settings("NullA", "Browser");
        settings.setValue("language", lang);

        Localization::loadLanguage(lang.toStdString());

        QMessageBox::information(
            this,
            Localization::qget("language_changed_title"),
            Localization::qget("language_changed_desc")
        );
    });

    languageLayout->addWidget(languageLabel);
    languageLayout->addWidget(languageCombo, 1);

    layout->addWidget(languageTitle);
    addDescription(Localization::qget("language_desc"), layout);
    layout->addLayout(languageLayout);

    QLabel* permissionsTitle = new QLabel(Localization::qget("permissions_title"));
    permissionsTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 5px; background: none;");

    QListWidget* permissionsList = new QListWidget();
    permissionsList->setSelectionMode(QAbstractItemView::SingleSelection);
    permissionsList->setMinimumHeight(120);

    layout->addWidget(permissionsTitle);
    addDescription(Localization::qget("permissions_desc"), layout);
    layout->addWidget(permissionsList);

    QStringList keys = settings->allKeys();
    for(const QString &key : keys) {
        if(!key.startsWith("permissions/")) continue;

        bool allowed = settings->value(key).toBool();

        QStringList parts = key.split("/");
        if(parts.size() < 3) continue;

        QString origin = parts[1];
        QWebEnginePermission::PermissionType type = static_cast<QWebEnginePermission::PermissionType>(parts[2].toInt());

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

        QString displayText = origin + " - " + featureName + " : " + (allowed ? Localization::qget("allowed") : Localization::qget("denied"));

        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, key);

        permissionsList->addItem(item);
    }

    permissionsList->setContextMenuPolicy(Qt::CustomContextMenu);

    QLabel* cookiesTitle = new QLabel(Localization::qget("cookies_title"));
    cookiesTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 5px; background: none;");

    QListWidget* cookiesList = new QListWidget();
    cookiesList->setSelectionMode(QAbstractItemView::SingleSelection);
    cookiesList->setMinimumHeight(120);

    layout->addWidget(cookiesTitle);
    addDescription(Localization::qget("cookies_desc"), layout);
    layout->addWidget(cookiesList);

    // Browser Management
    QLabel* manageTitle = new QLabel(Localization::qget("manage_title"));
    manageTitle->setStyleSheet("font-weight: bold; font-size: 14px; margin-top: 5px; background: none;");

    // WebGL
    QHBoxLayout* webglLayout = new QHBoxLayout();
    QLabel* webglLabel = new QLabel(Localization::qget("webgl_label"));
    webglLabel->setFixedWidth(60);
    webglLabel->setStyleSheet("background: none;");

    QComboBox* webglCombo = new QComboBox();
    webglCombo->addItems({Localization::qget("enabled"), Localization::qget("disabled")});
    webglCombo->setCurrentIndex(settings->value("enableWebGL", true).toBool() ? 0 : 1);

    webglLayout->addWidget(webglLabel);
    webglLayout->addWidget(webglCombo, 1);

    // JS
    QHBoxLayout* jsLayout = new QHBoxLayout();
    QLabel* jsLabel = new QLabel(Localization::qget("javascript_label"));
    jsLabel->setFixedWidth(65);
    jsLabel->setStyleSheet("background: none;");

    QComboBox* jsCombo = new QComboBox();
    jsCombo->addItems({Localization::qget("enabled"), Localization::qget("disabled")});
    jsCombo->setCurrentIndex(settings->value("enableJS", true).toBool() ? 0 : 1);

    jsLayout->addWidget(jsLabel);
    jsLayout->addWidget(jsCombo, 1);

    QHBoxLayout* cacheLayout = new QHBoxLayout();
    QLabel* cacheLabel = new QLabel(Localization::qget("cache_mode_label"));
    cacheLabel->setFixedWidth(90);
    cacheLabel->setStyleSheet("background: none;");

    cacheCombo = new QComboBox();
    cacheCombo->addItems({Localization::qget("cache_none"), Localization::qget("cache_disk"), Localization::qget("cache_memory")}); // 0=No Cache,1=Disk,2=Memory
    int savedCacheMode = settings->value("cacheMode", 1).toInt(); // default Disk
    cacheCombo->setCurrentIndex(savedCacheMode);
    cacheLayout->addWidget(cacheLabel);
    cacheLayout->addWidget(cacheCombo, 1);

    QPushButton* clearCacheBtn = new QPushButton(Localization::qget("clear_cache"));
    QPushButton* resetProfileBtn = new QPushButton(Localization::qget("reset_browser"));
    resetProfileBtn->setObjectName("resetBrowserButton");

    clearCacheBtn->setMinimumHeight(35);
    clearCacheBtn->setVisible(savedCacheMode == 1);
    resetProfileBtn->setMinimumHeight(35);

    layout->addWidget(manageTitle);
    addDescription(Localization::qget("webgl_desc"), layout);
    layout->addLayout(webglLayout);
    addDescription(Localization::qget("javascript_desc"), layout);
    layout->addLayout(jsLayout);
    addDescription(Localization::qget("cache_desc"), layout);
    layout->addLayout(cacheLayout);
    layout->addWidget(clearCacheBtn);
    layout->addWidget(resetProfileBtn);
    layout->addStretch();

    this->setContextMenuPolicy(Qt::CustomContextMenu);

    struct CreditsData {
        int clickCount = 0;
        QTimer* timer = nullptr;
    };
    CreditsData* creditsData = new CreditsData();

    connect(this, &QDialog::customContextMenuRequested, this, [this, creditsData](const QPoint&) {
        if (!creditsData->timer) {
            creditsData->timer = new QTimer(this);
            creditsData->timer->setSingleShot(true);
            connect(creditsData->timer, &QTimer::timeout, [creditsData]() {
                creditsData->clickCount = 0;
            });
        }

        creditsData->clickCount++;
        creditsData->timer->start(500);

        if (creditsData->clickCount >= 3) {
            creditsData->clickCount = 0;
            creditsData->timer->stop();

            QMessageBox::information(this, Localization::qget("credits_title"),
            Localization::qget("credits_text"));
        }
    });

    connect(themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::themeChanged);

    QString cookiePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cookies.json";
    QFile cookieFile(cookiePath);
    if (cookieFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(cookieFile.readAll());
        QJsonArray array = doc.array();

        for (const QJsonValue &val : array) {
            QJsonObject obj = val.toObject();
            QString domain = obj["domain"].toString();
            QString name = obj["name"].toString();

            QString display = QString("%1 | %2").arg(domain).arg(name);
            QListWidgetItem* item = new QListWidgetItem(display);

            item->setData(Qt::UserRole, obj);
            cookiesList->addItem(item);
        }
        cookieFile.close();
    }

    cookiesList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(cookiesList, &QListWidget::customContextMenuRequested, this, [this, cookiesList](const QPoint &pos){
        QListWidgetItem* item = cookiesList->itemAt(pos);
        if(!item) return;

        QMenu menu;
        QAction* deleteAction = menu.addAction(Localization::qget("delete_cookie"));
        QAction* deleteDomainAction = menu.addAction(Localization::qget("delete_domain_cookies"));
        QAction* deleteAllAction = menu.addAction(Localization::qget("delete_all_cookies"));

        QAction* selectedAction = menu.exec(cookiesList->mapToGlobal(pos));
        if(!selectedAction) return;

        if(selectedAction == deleteAction) {
            QJsonObject targetObj = item->data(Qt::UserRole).toJsonObject();
            QString domain = targetObj["domain"].toString();
            QString name = targetObj["name"].toString();

            emit cookieDeleted(domain, name); // OnlY delete cookies
            delete item;
        }
        else if(selectedAction == deleteDomainAction) {
            QJsonObject targetObj = item->data(Qt::UserRole).toJsonObject();
            QString domain = targetObj["domain"].toString();

            for(int i = cookiesList->count()-1; i >= 0; --i) {
                QListWidgetItem* it = cookiesList->item(i);
                QJsonObject obj = it->data(Qt::UserRole).toJsonObject();
                if(obj["domain"].toString() == domain) {
                    emit cookieDeleted(domain, obj["name"].toString());
                    delete it;
                }
            }
        }
        else if(selectedAction == deleteAllAction) {
            for(int i = cookiesList->count()-1; i >= 0; --i) {
                QListWidgetItem* it = cookiesList->item(i);
                QJsonObject obj = it->data(Qt::UserRole).toJsonObject();
                emit cookieDeleted(obj["domain"].toString(), obj["name"].toString());
                delete it;
            }
        }
    });

    connect(permissionsList, &QListWidget::customContextMenuRequested, this, [this, permissionsList](const QPoint &pos){
        QListWidgetItem* item = permissionsList->itemAt(pos);
        if(!item) return;

        QMenu menu;
        QAction* deleteAction = menu.addAction(Localization::qget("delete_permission"));
        QAction* deleteDomainAction = menu.addAction(Localization::qget("delete_domain_permissions"));
        QAction* deleteAllAction = menu.addAction(Localization::qget("delete_all_permissions"));

        QAction* selectedAction = menu.exec(permissionsList->mapToGlobal(pos));
        if(!selectedAction) return;

        if(selectedAction == deleteAction) {
            QString key = item->data(Qt::UserRole).toString();
            if(!key.isEmpty()) {
                settings->remove(key);
                delete item;
            }
        }
        else if(selectedAction == deleteDomainAction) {
            QString currentKey = item->data(Qt::UserRole).toString();
            QString domain = currentKey.split("/")[1];

            for(int i = permissionsList->count()-1; i >= 0; --i) {
                QListWidgetItem* it = permissionsList->item(i);
                QString itKey = it->data(Qt::UserRole).toString();

                if(itKey.startsWith("permissions/" + domain + "/")) {
                    settings->remove(itKey);
                    delete it;
                }
            }
        }
        else if(selectedAction == deleteAllAction) {
            for(int i = permissionsList->count()-1; i >= 0; --i) {
                QListWidgetItem* it = permissionsList->item(i);
                QString itKey = it->data(Qt::UserRole).toString();
                settings->remove(itKey);
                delete it;
            }
        }
    });

    connect(webglCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        bool enabled = (index == 0);
        settings->setValue("enableWebGL", enabled);
        if(m_profile)
            m_profile->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, enabled);
    });

    connect(jsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        bool enabled = (index == 0);
        settings->setValue("enableJS", enabled);
        if(m_profile)
            m_profile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, enabled);
    });

    connect(cacheCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, clearCacheBtn](int newIndex){
        int oldIndex = settings->value("cacheMode", 1).toInt();
        QString path = m_profile->persistentStoragePath() + "/Cache";

        QDir cacheDir(path);
        bool cacheExists = cacheDir.exists() && !cacheDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();

        if(oldIndex == 1 && newIndex != 1 && cacheExists) {
            m_profile->clearHttpCache();
        }

        switch(newIndex) {
            case 0: m_profile->setHttpCacheType(QWebEngineProfile::NoCache); break;
            case 1: m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache); break;
            case 2:
                m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
                m_profile->setHttpCacheMaximumSize(100 * 1024 * 1024);
                break;
        }

        clearCacheBtn->setVisible(newIndex == 1);
        settings->setValue("cacheMode", newIndex);
    });

    connect(clearCacheBtn, &QPushButton::clicked, this, [this]() {
        m_profile->clearHttpCache();
        QMessageBox::information(this, "NullA Browser", Localization::qget("cache_cleared_desc"));
    });

    connect(resetProfileBtn, &QPushButton::clicked, this, [this]() {
        auto result = QMessageBox::warning(this, Localization::qget("warning"),
                                           Localization::qget("reset_warning"),
                                           QMessageBox::Yes | QMessageBox::No);

        if(result == QMessageBox::Yes) {
            QString path = m_profile->persistentStoragePath();

            m_profile->clearHttpCache();
            m_profile->clearAllVisitedLinks();

            QDir dir(path);
            if(dir.exists()) {
                dir.removeRecursively();
            }

            QSettings settings("NullA", "Browser");
            settings.clear();

            QString cookiePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cookies.json";
            QFile::remove(cookiePath);

            QString bookmarkPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bookmarks.json";
            QFile::remove(bookmarkPath);

            settings.setValue("cacheMode", 2);
            m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);

            QMessageBox::information(this, "NullA Browser",
                                     Localization::qget("reset_done_desc"));
            qApp->quit();
        }
    });

    int savedTheme = settings->value("theme", isSystemDarkTheme() ? 1 : 0).toInt();
    themeCombo->setCurrentIndex(savedTheme);
    updateTheme(savedTheme);

    int cacheMode = settings->value("cacheMode", 1).toInt();

    switch(cacheMode) {
        case 0:
            m_profile->setHttpCacheType(QWebEngineProfile::NoCache);
            break;
        case 1:
            m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
            break;
        case 2:
            m_profile->setHttpCacheType(QWebEngineProfile::MemoryHttpCache);
            m_profile->setHttpCacheMaximumSize(100 * 1024 * 1024);
            break;
    }

    bool webglEnabled = settings->value("enableWebGL", true).toBool();
    bool jsEnabled = settings->value("enableJS", true).toBool();

    if(m_profile) {
        m_profile->settings()->setAttribute(QWebEngineSettings::WebGLEnabled, webglEnabled);
        m_profile->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, jsEnabled);
    }
}

void SettingsDialog::updateTheme(int themeIndex) {
    bool isDark = (themeIndex == 1);

    QString bgColor = isDark ? "#2c2c2c" : "#ececec";
    QString textColor = isDark ? "#ffffff" : "#000000";
    QString inputBg = isDark ? "#3d3d3d" : "#ffffff";
    QString borderColor = isDark ? "#555555" : "#cccccc";
    QString buttonBg = isDark ? "#3d3d3d" : "#e0e0e0";
    QString buttonHoverBg = isDark ? "#4d4d4d" : "#d0d0d0";

    setStyleSheet(QString(R"(
        QDialog {
            background-color: %1;
            color: %2;
        }
        QLabel {
            color: %2;
            background: none;
        }
        QComboBox {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 5px;
            min-height: 20px;
        }
        QComboBox:hover {
            border: 1px solid #0078d4;
        }
        QComboBox QAbstractItemView {
            background-color: %3;
            color: %2;
            selection-background-color: #0078d4;
        }
        QPushButton {
            background-color: %5;
            color: %2;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 8px;
        }
        QPushButton:hover {
            background-color: %6;
            border: 1px solid #0078d4;
        }
        QPushButton#resetBrowserButton {
            color: #ff5555;
        }
        QListWidget {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 3px;
            outline: none;
        }
        QListWidget::item {
            padding: 8px;
            border-bottom: 1px solid %4;
            color: %2;
        }
        QListWidget::item:selected, QListWidget::item:selected:active {
            background-color: transparent;
            color: %2;
        }
        QListWidget::item:hover {
            background-color: %6;
        }
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QScrollBar:vertical {
            border: none;
            background: %1;
            width: 10px;
        }
        QScrollBar::handle:vertical {
            background: %4;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
            width: 0px;
            background: none;
            border: none;
        }
        QScrollBar:horizontal {
            border: none;
            background: %1;
            height: 10px;
        }
        QScrollBar::handle:horizontal {
            background: %4;
            min-width: 20px;
            border-radius: 5px;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            height: 0px;
            width: 0px;
            background: none;
            border: none;
        }
    )").arg(bgColor).arg(textColor).arg(inputBg).arg(borderColor).arg(buttonBg).arg(buttonHoverBg));
}

bool SettingsDialog::isSystemDarkTheme() {
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

int SettingsDialog::getSelectedTheme() const {
    return themeCombo->currentIndex();
}

void SettingsDialog::addSearchEngine() {

    bool ok;

    QString name = QInputDialog::getText(
        this,
        Localization::qget("add_search_engine_title"),
        Localization::qget("engine_name"),
        QLineEdit::Normal,
        "",
        &ok
    );

    if(!ok || name.isEmpty())
        return;

    QString url = QInputDialog::getText(
        this,
        Localization::qget("add_search_engine_title"),
        Localization::qget("search_url"),
        QLineEdit::Normal,
        "https://example.com/search?q=%s",
        &ok
    );

    if(!ok || !url.contains("%s")) {
        QMessageBox::warning(this, Localization::qget("error"), Localization::qget("url_must_contain"));
        return;
    }

    searchCombo->addItem(name);

    QStringList engines = settings->value("customSearchEngines").toStringList();
    engines.append(name + "|" + url);

    settings->setValue("customSearchEngines", engines);
}
