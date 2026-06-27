/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#ifndef SAFECOMBOBOX_H
#define SAFECOMBOBOX_H

#pragma once

#include <QComboBox>
#include <QWheelEvent>

class SafeComboBox : public QComboBox {
    Q_OBJECT

public:
    explicit SafeComboBox(QWidget* parent = nullptr);
    ~SafeComboBox() override = default;

protected:
    void wheelEvent(QWheelEvent* event) override;
};

#endif // SAFECOMBOBOX_H
