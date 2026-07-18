/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QWebEngineProfile>
#include <QComboBox>
#include <QSettings>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QDir>
#include <QInputDialog>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWebEngineProfile* profile, QWidget* parent = nullptr);
    void updateTheme(int themeIndex);
    int getSelectedTheme() const;

signals:
    void themeChanged(int index);
    void searchEngineChanged(const QString& engine);
    void cookieDeleted(const QString &domain, const QString &name);
    void adBlockToggled(bool enabled);

private slots:
    void addSearchEngine();

private:
    bool isSystemDarkTheme();

    QWebEngineProfile* m_profile;
    QComboBox* themeCombo;
    QComboBox* searchCombo;
    QComboBox* cacheCombo;
    QPushButton* addEngineBtn;
    QSettings* settings;
};

#endif // SETTINGSDIALOG_H
