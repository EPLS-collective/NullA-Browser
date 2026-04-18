/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/StartPage.h"
#include <QKeyEvent>
#include <QTimer>

StartPage::StartPage(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setAlignment(Qt::AlignCenter);
    titleLayout->setSpacing(2);

    nullLabel = new QLabel("Null");
    nullLabel->setStyleSheet("font-size: 32px; font-weight: bold; background: none;");

    iconLabel = new QLabel();
    QPixmap iconPix(":/nulla_icon.png");
    if(!iconPix.isNull()) {
        iconLabel->setPixmap(iconPix.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        iconLabel->setText("A");
        iconLabel->setStyleSheet("font-size: 32px; background: none;");
    }

    browserLabel = new QLabel(" Browser");
    browserLabel->setStyleSheet("font-size: 32px; font-weight: bold; background: none;");

    titleLayout->addWidget(nullLabel);
    titleLayout->addWidget(iconLabel);
    titleLayout->addWidget(browserLabel);

    // The searchEdit is designed as a visual trigger rather than a standard input field.
    // Key events are intercepted to redirect the focus or handle navigation logic via internal event filters.
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Search...");
    searchEdit->setFixedSize(500, 40);

    searchEdit->setReadOnly(false);

    layout->addStretch();
    layout->addLayout(titleLayout);
    layout->addSpacing(20);
    layout->addWidget(searchEdit, 0, Qt::AlignCenter);
    layout->addStretch();

    searchEdit->installEventFilter(this);

    connect(searchEdit, &QLineEdit::returnPressed, this, [this]() {
        emit navigateToCurrentUrl();
    });
}

bool StartPage::eventFilter(QObject* obj, QEvent* event) {
    if (obj == searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            emit navigateToCurrentUrl();
            return true;
        }

        QString keyText = keyEvent->text();

        if (!keyText.isEmpty() && keyText[0].isPrint()) {
            emit focusUrlBarAndType(keyText);
            return true;
        }

        if (keyEvent->key() == Qt::Key_Left ||
            keyEvent->key() == Qt::Key_Right ||
            keyEvent->key() == Qt::Key_Home ||
            keyEvent->key() == Qt::Key_End) {
            emit forwardKeyToUrlBar(keyEvent->key());
        return true;
            }
    }

    return QWidget::eventFilter(obj, event);
}

void StartPage::setSearchTemplate(const QString& templateUrl) {
    searchTemplate = templateUrl;
}

void StartPage::applyTheme(bool isDark) {
    QString textColor = isDark ? "#ffffff" : "#000000";
    QString bgColor = isDark ? "#1e1e1e" : "#f5f5f5";
    QString searchBg = isDark ? "#2d2d2d" : "#ffffff";
    QString searchBorder = isDark ? "#555555" : "#cccccc";
    QString searchText = isDark ? "#ffffff" : "#000000";

    nullLabel->setStyleSheet(QString("font-size: 32px; font-weight: bold; color: %1; background: none;").arg(textColor));
    browserLabel->setStyleSheet(QString("font-size: 32px; font-weight: bold; color: %1; background: none;").arg(textColor));
    iconLabel->setStyleSheet(QString("font-size: 32px; color: %1; background: none;").arg(textColor));

    setStyleSheet(QString("QWidget { background-color: %1; }").arg(bgColor));

    searchEdit->setStyleSheet(QString(R"(
        QLineEdit {
            background-color: %1;
            color: %2;
            border: 2px solid %3;
            border-radius: 20px;
            padding: 0 15px;
            font-size: 14px;
            selection-background-color: #0078d4;
        }
        QLineEdit:focus {
            border: 2px solid #0078d4;
        }
        QLineEdit::placeholder {
            color: %4;
        }
    )").arg(searchBg).arg(searchText).arg(searchBorder).arg(isDark ? "#888888" : "#666666"));
}
