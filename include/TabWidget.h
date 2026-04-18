/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <QTabWidget>
#include "TabBar.h"

class TabWidget : public QTabWidget {
    Q_OBJECT
public:
    explicit TabWidget(QWidget* parent = nullptr);
    TabBar* getTabBar() const;
};

#endif // TABWIDGET_H
