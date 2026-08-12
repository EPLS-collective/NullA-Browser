/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "ExtensionStore.h"
#include "../include/Localization.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QApplication>
#include <algorithm>

#ifdef Q_OS_WIN
#include <QSettings>
#endif

const QString ExtensionStore::kIndexUrl =
    "https://electus2000.github.io/nulla-extensions/extensions.json";

const QString ExtensionStore::kMetaFileName = ".nulla_store_meta.json";

ExtensionStore::ExtensionStore(QWidget *parent)
: QDialog(parent)
, m_nam(new QNetworkAccessManager(this))
{
    setWindowTitle(Localization::qget("extension_title"));
    resize(560, 420);

    auto *layout = new QVBoxLayout(this);

    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText(Localization::qget("extension_search_placeholder"));
    connect(m_searchBox, &QLineEdit::textChanged, this, &ExtensionStore::filterList);
    layout->addWidget(m_searchBox);

    m_statusLabel = new QLabel(Localization::qget("extension_loading"), this);
    layout->addWidget(m_statusLabel);

    m_list = new QListWidget(this);
    m_list->setSpacing(4);
    layout->addWidget(m_list);

    QSettings settings("NullA", "Browser");
    int savedTheme = settings.value("theme", isSystemDarkTheme() ? 1 : 0).toInt();
    applyTheme(savedTheme == 1);

    loadInstalledLocally();
    populateList();

    fetchIndex();
}

bool ExtensionStore::isSystemDarkTheme() const
{
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

void ExtensionStore::applyTheme(bool isDark)
{
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
        QLineEdit {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 6px;
        }
        QLineEdit:focus {
            border: 1px solid #0078d4;
        }
        QPushButton {
            background-color: %5;
            color: %2;
            border: 1px solid %4;
            border-radius: 4px;
            padding: 6px;
        }
        QPushButton:hover {
            background-color: %6;
            border: 1px solid #0078d4;
        }
        QPushButton:disabled {
            color: #888888;
        }
        QListWidget {
            background-color: %3;
            color: %2;
            border: 1px solid %4;
            border-radius: 3px;
            outline: none;
        }
        QListWidget::item {
            border-bottom: 1px solid %4;
            color: %2;
        }
        QListWidget::item:selected, QListWidget::item:selected:active {
            background-color: transparent;
            color: %2;
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
    )").arg(bgColor, textColor, inputBg, borderColor, buttonBg, buttonHoverBg));
}

QString ExtensionStore::extensionsRoot() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/extensions";
}

bool ExtensionStore::isInstalled(const QString &id) const
{
    return QDir(extensionsRoot() + "/" + id).exists();
}

bool ExtensionStore::isExtensionEnabled(const QString &id) const
{
    QSettings settings("NullA", "Browser");
    return !settings.value("extensions/disabled/" + id, false).toBool();
}

int ExtensionStore::indexOfExtension(const QString &id) const
{
    for (int i = 0; i < m_extensions.size(); ++i) {
        if (m_extensions[i].id == id) return i;
    }
    return -1;
}

void ExtensionStore::loadInstalledLocally()
{
    QDir root(extensionsRoot());
    if (!root.exists()) return;

    const QStringList dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &dirName : dirs) {
        ExtensionInfo info;
        info.id = dirName;
        info.name = dirName;

        QString extPath = extensionsRoot() + "/" + dirName;

        QFile metaFile(extPath + "/" + kMetaFileName);
        if (metaFile.open(QIODevice::ReadOnly)) {
            QJsonObject obj = QJsonDocument::fromJson(metaFile.readAll()).object();
            info.id = obj.value("id").toString(info.id);
            info.name = obj.value("name").toString(info.name);
            info.description = obj.value("description").toString();
            info.version = obj.value("version").toString();
            info.author = obj.value("author").toString();
            metaFile.close();
        } else {
            QFile manifestFile(extPath + "/manifest.json");
            if (manifestFile.open(QIODevice::ReadOnly)) {
                QJsonObject obj = QJsonDocument::fromJson(manifestFile.readAll()).object();
                info.name = obj.value("name").toString(info.name);
                info.version = obj.value("version").toString();
                info.description = obj.value("description").toString();
                info.author = obj.value("author").toString();
                manifestFile.close();
            }
        }

        m_extensions.append(info);
    }
}

void ExtensionStore::fetchIndex()
{
    QNetworkRequest req{QUrl(kIndexUrl)};
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onIndexReplyFinished(reply);
    });
}

void ExtensionStore::onIndexReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText(Localization::qget("extension_fetch_failed").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        m_statusLabel->setText(Localization::qget("extension_invalid_index"));
        return;
    }

    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        ExtensionInfo info;
        info.id = o.value("id").toString();
        info.name = o.value("name").toString(info.id);
        info.description = o.value("description").toString();
        info.version = o.value("version").toString();
        info.author = o.value("author").toString();
        info.downloadUrl = o.value("download_url").toString();

        if (info.id.isEmpty() || info.downloadUrl.isEmpty()) continue;

        int idx = indexOfExtension(info.id);
        if (idx >= 0) {
            m_extensions[idx].name = info.name;
            m_extensions[idx].description = info.description;
            m_extensions[idx].version = info.version;
            m_extensions[idx].author = info.author;
            m_extensions[idx].downloadUrl = info.downloadUrl;
        } else {
            m_extensions.append(info);
        }
    }

    m_statusLabel->setText(Localization::qget("extension_count").arg(m_extensions.size()));
    populateList();
}

void ExtensionStore::populateList()
{
    m_list->clear();

    std::stable_sort(m_extensions.begin(), m_extensions.end(),
                      [this](const ExtensionInfo &a, const ExtensionInfo &b) {
        bool ia = isInstalled(a.id);
        bool ib = isInstalled(b.id);
        return ia && !ib;
    });

    for (const ExtensionInfo &info : m_extensions) {
        auto *item = new QListWidgetItem(m_list);
        auto *row = new QWidget();
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(6, 6, 6, 6);

        bool installed = isInstalled(info.id);

        auto *textLayout = new QVBoxLayout();
        QString headerHtml = (info.version.isEmpty() && info.author.isEmpty())
            ? QString("<b>%1</b>").arg(info.name.toHtmlEscaped())
            : QString("<b>%1</b> <span style='color:gray'>v%2 &middot; %3</span>")
                  .arg(info.name.toHtmlEscaped(), info.version.toHtmlEscaped(), info.author.toHtmlEscaped());
        auto *nameLabel = new QLabel(headerHtml);
        textLayout->addWidget(nameLabel);

        if (!info.description.isEmpty()) {
            auto *descLabel = new QLabel(info.description);
            descLabel->setWordWrap(true);
            textLayout->addWidget(descLabel);
        }

        auto *actionLayout = new QVBoxLayout();
        actionLayout->setAlignment(Qt::AlignRight);

        auto *button = new QPushButton(installed ? Localization::qget("extension_uninstall")
                                                   : Localization::qget("extension_install"));
        button->setFixedWidth(100);
        actionLayout->addWidget(button);

        if (installed) {
            bool enabled = isExtensionEnabled(info.id);
            auto *toggleButton = new QPushButton(enabled ? Localization::qget("extension_disable")
                                                           : Localization::qget("extension_enable"));
            toggleButton->setFixedWidth(100);
            actionLayout->addWidget(toggleButton);

            connect(toggleButton, &QPushButton::clicked, this, [this, info, toggleButton]() {
                bool nowEnabled = !isExtensionEnabled(info.id);
                QSettings settings("NullA", "Browser");
                settings.setValue("extensions/disabled/" + info.id, !nowEnabled);
                emit extensionToggleRequested(info.id, nowEnabled);
                toggleButton->setText(nowEnabled ? Localization::qget("extension_disable")
                                                  : Localization::qget("extension_enable"));
            });
        }

        rowLayout->addLayout(textLayout, 1);
        rowLayout->addLayout(actionLayout);

        item->setSizeHint(row->sizeHint());
        m_list->setItemWidget(item, row);

        connect(button, &QPushButton::clicked, this, [this, info, button]() {
            if (isInstalled(info.id)) {
                QSettings settings("NullA", "Browser");
                settings.remove("extensions/disabled/" + info.id);
                emit extensionUninstallRequested(info.id);
                populateList();
            } else {
                startInstall(info, button);
            }
        });
    }
}

void ExtensionStore::filterList(const QString &text)
{
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const ExtensionInfo &info = m_extensions.at(i);
        bool match = info.name.contains(text, Qt::CaseInsensitive)
                  || info.description.contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

void ExtensionStore::startInstall(const ExtensionInfo &info, QPushButton *button)
{
    button->setEnabled(false);
    button->setText(Localization::qget("extension_installing"));

    QNetworkRequest req{QUrl(info.downloadUrl)};
    QNetworkReply *reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, info, button]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(this, Localization::qget("extension_title"),
                                  Localization::qget("extension_download_failed").arg(reply->errorString()));
            button->setEnabled(true);
            button->setText(Localization::qget("extension_install"));
            return;
        }

        QString tempDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(tempDir);
        QString zipPath = tempDir + "/" + info.id + ".zip";

        QFile f(zipPath);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(reply->readAll());
            f.close();
        }

        QSettings settings("NullA", "Browser");
        settings.setValue("extensions/disabled/" + info.id, false);

        emit extensionInstallRequested(zipPath, info.id, info.name, info.description,
                                        info.version, info.author);

        populateList();
    });
}
