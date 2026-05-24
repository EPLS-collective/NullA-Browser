/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/TabBar.h"
#include "../include/TabPage.h"
#include "../include/Localization.h"
#include <QMenu>
#include <QTabWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

TabBar::TabBar(QWidget* parent) : QTabBar(parent), plusButton(nullptr), hoveredCloseIndex(-1) {
    setTabsClosable(false);
    setMovable(true);
    setDrawBase(false);
    setAutoFillBackground(true);
    setExpanding(false);
    setMouseTracking(true);
    setMinimumHeight(32);

    plusButton = new QPushButton("+", this);
    plusButton->setCursor(Qt::PointingHandCursor);
    plusButton->setFocusPolicy(Qt::NoFocus);
    plusButton->setVisible(false);

    // Using a zero-ms timer to defer the initial button positioning.
    // This ensures the layout has completed its first pass and tab geometries are valid before calculations.
    QTimer::singleShot(0, this, &TabBar::updatePlusButtonPosition);
}

void TabBar::updatePlusButtonPosition() {
    if (!plusButton) return;
    if (count() == 0) {
        plusButton->setVisible(false);
        return;
    }

    QRect lastRect = tabRect(count() - 1);
    if (!lastRect.isValid() || lastRect.width() <= 0) {
        QTimer::singleShot(50, this, &TabBar::updatePlusButtonPosition);
        return;
    }

    int h = height();
    if (h <= 0) h = 32;
    plusButton->setFixedSize(h, h);

    int x = width() - h;
    if (lastRect.right() < x) {
        x = lastRect.right();
    }

    plusButton->move(x, 0);
    if (!plusButton->isVisible()) plusButton->show();
    plusButton->raise();
}

void TabBar::tabLayoutChange() {
    QTabBar::tabLayoutChange();
    updatePlusButtonPosition();
}

void TabBar::resizeEvent(QResizeEvent* event) {
    QTabBar::resizeEvent(event);
    updatePlusButtonPosition();
}

void TabBar::contextMenuEvent(QContextMenuEvent* event) {
    int index = tabAt(event->pos());
    if (index == -1) return;

    QTabWidget* tw = qobject_cast<QTabWidget*>(parentWidget());
    TabPage* page = tw ? qobject_cast<TabPage*>(tw->widget(index)) : nullptr;

    QMenu menu(this);
    QAction* reloadAction = menu.addAction(Localization::qget("reload_tab"));

    bool isMuted = false;
    if (page && page->webView() && page->webView()->page()) {
        isMuted = page->webView()->page()->isAudioMuted();
    }

    QAction* muteAction = menu.addAction(isMuted ? Localization::qget("unmute_tab") : Localization::qget("mute_tab"));
    menu.addSeparator();
    QAction* closeAction = menu.addAction(Localization::qget("close_tab"));
    QAction* closeOthersAction = menu.addAction(Localization::qget("close_other_tabs"));

    QAction* selectedAction = menu.exec(event->globalPos());

    if (selectedAction == reloadAction) {
        emit reloadTabRequested(index);
    } else if (selectedAction == muteAction) {
        emit muteTabRequested(index, !isMuted);
    } else if (selectedAction == closeAction) {
        emit tabCloseRequested(index);
    } else if (selectedAction == closeOthersAction) {
        for (int i = count() - 1; i >= 0; --i) {
            if (i != index) emit tabCloseRequested(i);
        }
    }
}

QRect TabBar::getCloseButtonRect(int index) const {
    QRect tRect = tabRect(index);
    if (tRect.width() < 48) return QRect();

    int tabHeight = this->height();
    int buttonSize = qMin(16, tabHeight - 8);
    int y = tRect.top() + (tRect.height() - buttonSize) / 2;

    return QRect(tRect.right() - buttonSize - 6, y, buttonSize, buttonSize);
}

void TabBar::paintEvent(QPaintEvent* event) {
    QTabBar::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int tabHeight = this->height();
    int buttonSize = qMin(16, tabHeight - 8);

    for (int i = 0; i < count(); ++i) {
        QRect closeButtonRect = getCloseButtonRect(i);
        if (closeButtonRect.isNull()) continue;

        int padding = buttonSize / 4;

        if (i == hoveredCloseIndex) {
            painter.setBrush(QColor(255, 85, 85, 200));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(closeButtonRect);

            painter.setPen(QPen(Qt::white, 2));
        } else {
            painter.setPen(QPen(QColor(150, 150, 150), 1.5));
            painter.setBrush(Qt::NoBrush);
        }

        int x1 = closeButtonRect.left() + padding;
        int y1 = closeButtonRect.top() + padding;
        int x2 = closeButtonRect.right() - padding;
        int y2 = closeButtonRect.bottom() - padding;
        painter.drawLine(x1, y1, x2, y2);
        painter.drawLine(x1, y2, x2, y1);
    }
}

void TabBar::mousePressEvent(QMouseEvent* event) {
    for (int i = 0; i < count(); ++i) {
        if (getCloseButtonRect(i).contains(event->pos())) {
            emit tabCloseRequested(i);
            event->accept();
            return;
        }
    }
    QTabBar::mousePressEvent(event);
}

void TabBar::mouseMoveEvent(QMouseEvent* event) {
    int newIndex = -1;
    for (int i = 0; i < count(); ++i) {
        if (getCloseButtonRect(i).contains(event->pos())) {
            newIndex = i;
            break;
        }
    }

    if (hoveredCloseIndex != newIndex) {
        hoveredCloseIndex = newIndex;
        update();
    }

    setCursor(hoveredCloseIndex != -1 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QTabBar::mouseMoveEvent(event);
}

void TabBar::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        int index = tabAt(event->pos());
        if (index != -1) emit tabCloseRequested(index);
    }
    QTabBar::mouseReleaseEvent(event);
}

void TabBar::leaveEvent(QEvent* event) {
    hoveredCloseIndex = -1;
    setCursor(Qt::ArrowCursor);
    update();
    QTabBar::leaveEvent(event);
}

QSize TabBar::tabSizeHint(int index) const {
    int tabCount = count();
    if (tabCount <= 0) return QTabBar::tabSizeHint(index);

    int btnWidth = height();
    int availableWidth = width() - btnWidth;

    int normalWidth = 180;
    int minWidth = 32;

    if (tabCount * normalWidth <= availableWidth) {
        return QSize(normalWidth, height());
    } else {
        int baseWidth = availableWidth / tabCount;
        int remainder = availableWidth % tabCount;
        if (baseWidth < minWidth) baseWidth = minWidth;
        int finalWidth = (index < remainder) ? (baseWidth + 1) : baseWidth;
        return QSize(finalWidth, height());
    }
}
