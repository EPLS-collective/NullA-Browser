/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/Localization.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QCoreApplication>

static std::unordered_map<std::string, std::string> loadJsonTranslations(const std::string& langCode) {
    std::unordered_map<std::string, std::string> map;
    QString path = ":/locales/" + QString::fromStdString(langCode) + ".json";

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return map;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return map;
    }

    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        map[it.key().toStdString()] = it.value().toString().toStdString();
    }

    return map;
}

bool Localization::loadLanguage(const std::string& langCode) {
    auto& store = instance();
    std::lock_guard<std::mutex> lock(store.mutex);

    store.translations.clear();
    store.defaultTranslations.clear();

    store.defaultTranslations = loadJsonTranslations("en");

    if (langCode != "en") {
        store.translations = loadJsonTranslations(langCode);
    }

    store.currentLang = langCode;
    return true;
}

std::string Localization::get(const std::string& key) {
    auto& store = instance();
    std::lock_guard<std::mutex> lock(store.mutex);

    auto it = store.translations.find(key);
    if (it != store.translations.end()) {
        return it->second;
    }

    auto defaultIt = store.defaultTranslations.find(key);
    if (defaultIt != store.defaultTranslations.end()) {
        return defaultIt->second;
    }

    return key;
}

QString Localization::qget(const std::string& key) {
    return QString::fromStdString(get(key));
}

std::string Localization::currentLanguage() {
    auto& store = instance();
    std::lock_guard<std::mutex> lock(store.mutex);
    return store.currentLang;
}
