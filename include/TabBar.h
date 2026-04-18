/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef TABBAR_H
#define TABBAR_H
#include <QTabBar>
#include <QMouseEvent>
#include <QPainter>
#include <QContextMenuEvent>
#include <QPushButton>

class TabBar : public QTabBar {
    Q_OBJECT
public:
    explicit TabBar(QWidget* parent = nullptr);
    QSize tabSizeHint(int index) const override;

    QPushButton* plusButton;

signals:
    void tabCloseRequested(int index);
    void reloadTabRequested(int index);
    void muteTabRequested(int index, bool mute);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

    void resizeEvent(QResizeEvent* event) override;
    void tabLayoutChange() override;

private:
    int hoveredCloseIndex = -1;
    QRect getCloseButtonRect(int index) const;
    void updatePlusButtonPosition();
};

#endif // TABBAR_H
