/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/TabWidget.h"

TabWidget::TabWidget(QWidget* parent) : QTabWidget(parent) {
    TabBar* tabBar = new TabBar(this);
    setTabBar(tabBar);
    setTabsClosable(false);
    setMovable(true);
    setDocumentMode(true);
    setContentsMargins(0, 0, 0, 0);

    connect(tabBar, &TabBar::tabCloseRequested, this, &TabWidget::tabCloseRequested);
}

TabBar* TabWidget::getTabBar() const {
    return qobject_cast<TabBar*>(tabBar());
}
