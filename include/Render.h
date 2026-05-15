/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef RENDER_H
#define RENDER_H

#pragma once

#include <QObject>

class RenderController : public QObject {
    Q_OBJECT

public:
    explicit RenderController(QObject *parent = nullptr);

    void request();
    void enable(bool on);

public slots:
    void process();

signals:
    void frameReady();

private:
    bool enabled;
    bool pending;
    bool scheduled;
};

#endif // RENDER_H
