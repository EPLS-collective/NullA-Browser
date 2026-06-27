/*
 * Copyright (c) 2025-2026 EPLS (Electus Progressive Liberation Software)
 * SPDX-License-Identifier: LicenseRef-EPLS-1.1
 * Distributed under the EPLS (Electus Progressive Liberation Software) License.
 * See LICENSE file in the project root for full terms.
 */

#include "../include/SafeComboBox.h"

SafeComboBox::SafeComboBox(QWidget* parent)
: QComboBox(parent) {
    // We will use this section if we need a specific setting for this combobox in the future.
}

void SafeComboBox::wheelEvent(QWheelEvent* event) {
    event->ignore();
}
