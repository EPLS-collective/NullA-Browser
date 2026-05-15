/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef LOCALIZATION_H
#define LOCALIZATION_H

#pragma once

#include <QString>
#include <string>
#include <unordered_map>
#include <mutex>

class Localization {
public:
    static bool loadLanguage(const std::string& langCode);
    static std::string get(const std::string& key);
    static QString qget(const std::string& key);
    static std::string currentLanguage();

private:
    struct Storage {
        std::unordered_map<std::string, std::string> translations;
        std::string currentLang = "en";
        std::mutex mutex;
    };

    static Storage& instance() {
        static Storage inst;
        return inst;
    }
};

#endif // LOCALIZATION_H
