/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/Render.h"
#include <QMetaObject>

RenderController::RenderController(QObject *parent):
QObject(parent),
enabled(true),
pending(false),
scheduled(false)
{
}

void RenderController::request()
{
    if (!enabled)
        return;

    pending = true;

    if (!scheduled) {
        scheduled = true;

        QMetaObject::invokeMethod(
            this,
            "process",
            Qt::QueuedConnection
        );
    }
}

void RenderController::process()
{
    scheduled = false;
    if (!enabled || !pending)
        return;

    pending = false;
    emit frameReady();
}

void RenderController::enable(bool on)
{
    enabled = on;

    if (!enabled) {
        pending = false;
        scheduled = false;
    }
}
