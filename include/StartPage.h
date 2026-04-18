/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef STARTPAGE_H
#define STARTPAGE_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QUrl>

class StartPage : public QWidget {
    Q_OBJECT
public:
    explicit StartPage(QWidget* parent = nullptr);
    void setSearchTemplate(const QString& templateUrl);
    void applyTheme(bool isDark);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void navigateTo(const QUrl& url);
    void navigateToCurrentUrl();
    void focusUrlBarAndType(const QString& text);
    void forwardKeyToUrlBar(int key);

private:
    QLineEdit* searchEdit;
    QLabel* nullLabel;
    QLabel* iconLabel;
    QLabel* browserLabel;
    QString searchTemplate;
};

#endif // STARTPAGE_H
