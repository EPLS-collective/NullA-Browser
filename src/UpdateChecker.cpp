/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/UpdateChecker.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QUrl>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTextStream>
#include <QProcess>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0-dev"
#endif

namespace {
    constexpr const char* kLatestReleaseUrl =
    "https://api.github.com/repos/EPLS-collective/NullA-Browser/releases/latest";
}

UpdateChecker::UpdateChecker(QObject* parent)
: QObject(parent), m_manager(new QNetworkAccessManager(this)) {
    connect(m_manager, &QNetworkAccessManager::finished, this, &UpdateChecker::onReplyFinished);
}

QString UpdateChecker::currentVersion() const {
    return QString::fromUtf8(APP_VERSION);
}

QString UpdateChecker::normalizeVersion(QString v) {
    if (v.startsWith('v', Qt::CaseInsensitive)) v.remove(0, 1);
    return v.trimmed();
}

int UpdateChecker::compareVersions(const QString &a, const QString &b) {
    const QVersionNumber va = QVersionNumber::fromString(normalizeVersion(a));
    const QVersionNumber vb = QVersionNumber::fromString(normalizeVersion(b));
    return QVersionNumber::compare(va, vb);
}

void UpdateChecker::checkForUpdates() {
    QNetworkRequest request((QUrl(QString::fromUtf8(kLatestReleaseUrl))));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("NullA-Browser/%1").arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    m_manager->get(request);
}

void UpdateChecker::onReplyFinished(QNetworkReply* reply) {
    if (reply == m_downloadReply) return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit checkFailed(reply->errorString());
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        emit checkFailed(QStringLiteral("Invalid response from update server"));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString tag = obj.value("tag_name").toString();
    const QString releaseUrl = obj.value("html_url").toString();
    const QString notes = obj.value("body").toString();

    if (tag.isEmpty()) {
        emit checkFailed(QStringLiteral("No release information found"));
        return;
    }

    QString downloadUrl;
    const QJsonArray assets = obj.value("assets").toArray();
    for (const QJsonValue &v : assets) {
        const QString name = v.toObject().value("name").toString();
        #if defined(Q_OS_WIN)
        if (name.endsWith(".zip", Qt::CaseInsensitive)) {
        #else
        if (name.endsWith(".tar.gz", Qt::CaseInsensitive)) {
        #endif
            downloadUrl = v.toObject().value("browser_download_url").toString();
            break;
        }
    }
    if (downloadUrl.isEmpty() && !assets.isEmpty())
        downloadUrl = assets.first().toObject().value("browser_download_url").toString();

    if (compareVersions(tag, currentVersion()) > 0) {
        emit updateAvailable(normalizeVersion(tag), releaseUrl, notes, downloadUrl);
    } else {
        emit upToDate();
    }
}

void UpdateChecker::downloadUpdate(const QString &downloadUrl) {
    if (downloadUrl.isEmpty()) {
        emit downloadFailed(QStringLiteral("No downloadable installation file was found."));
        return;
    }

    QNetworkRequest request((QUrl(downloadUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString("NullA-Browser/%1").arg(currentVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_downloadReply = m_manager->get(request);
    connect(m_downloadReply, &QNetworkReply::downloadProgress,
            this, &UpdateChecker::downloadProgress);

    connect(m_downloadReply, &QNetworkReply::finished, this, [this, downloadUrl]() {
        QNetworkReply* reply = m_downloadReply;
        m_downloadReply = nullptr;

        if (reply->error() != QNetworkReply::NoError) {
            emit downloadFailed(reply->errorString());
            reply->deleteLater();
            return;
        }

        const QString fileName = QUrl(downloadUrl).fileName();
        const QString dirPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString filePath = QDir(dirPath).filePath(fileName);

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit downloadFailed(QStringLiteral("The update file could not be saved."));
            reply->deleteLater();
            return;
        }
        file.write(reply->readAll());
        file.close();
        reply->deleteLater();
        emit downloadFinished(filePath);
    });
}

void UpdateChecker::installUpdate(const QString &archivePath) {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appExe = QCoreApplication::applicationFilePath();
    const QString tempBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString extractDir = QDir(tempBase).filePath("nulla_update_extracted");

    QDir(extractDir).removeRecursively();

    #if defined(Q_OS_WIN)
    const QString scriptPath = QDir(tempBase).filePath("nulla_update.bat");
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit downloadFailed(QStringLiteral("Update script could not be created."));
        return;
    }
    QTextStream out(&script);
    out << "@echo off\r\n";
    out << "timeout /t 2 /nobreak >nul\r\n";
    out << "mkdir \"" << QDir::toNativeSeparators(extractDir) << "\"\r\n";
    out << "tar -xf \"" << QDir::toNativeSeparators(archivePath) << "\" -C \"" << QDir::toNativeSeparators(extractDir) << "\"\r\n";
    out << "robocopy \"" << QDir::toNativeSeparators(extractDir) << "\\NullA\" \"" << QDir::toNativeSeparators(appDir) << "\" /E /IS /IT >nul\r\n";
    out << "start \"\" \"" << QDir::toNativeSeparators(appExe) << "\"\r\n";
    out << "rmdir /s /q \"" << QDir::toNativeSeparators(extractDir) << "\"\r\n";
    out << "del \"%~f0\"\r\n";
    script.close();

    QProcess::startDetached("cmd.exe", {"/c", scriptPath});
    #else
    const QString scriptPath = QDir(tempBase).filePath("nulla_update.sh");
    QFile script(scriptPath);
    if (!script.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit downloadFailed(QStringLiteral("Update script could not be created."));
        return;
    }
    QTextStream out(&script);
    out << "#!/bin/sh\n";
    out << "sleep 2\n";
    out << "mkdir -p \"" << extractDir << "\"\n";
    out << "tar -xzf \"" << archivePath << "\" -C \"" << extractDir << "\"\n";
    out << "cp -rf \"" << extractDir << "/NullA/.\" \"" << appDir << "/\"\n";
    out << "chmod +x \"" << appExe << "\"\n";
    out << "rm -rf \"" << extractDir << "\"\n";
    out << "\"" << appExe << "\" &\n";
    out << "rm -- \"$0\"\n";
    script.close();
    script.setPermissions(script.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    QProcess::startDetached("/bin/sh", {scriptPath});
    #endif

    QCoreApplication::quit();
}


