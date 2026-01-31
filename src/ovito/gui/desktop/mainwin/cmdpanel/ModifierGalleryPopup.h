////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
//  OVITO is free software; you can redistribute it and/or modify it either under the
//  terms of the GNU General Public License version 3 as published by the Free Software
//  Foundation (the "GPL") or, at your option, under the terms of the MIT License.
//  If you do not alter this notice, a recipient may use your version of this
//  file under either the GPL or the MIT License.
//
//  You should have received a copy of the GPL along with this program in a
//  file LICENSE.GPL.txt.  You should have received a copy of the MIT License along
//  with this program in a file LICENSE.MIT.txt
//
//  This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND,
//  either express or implied. See the GPL or the MIT License for the specific language
//  governing rights and limitations.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

class AvailableModifiersModel;
class ModifierAction;

/**
 * A clickable label widget for modifier items in the card popup.
 * Displays the modifier name with hover highlighting.
 */
class ClickableModifierLabel : public QWidget
{
    Q_OBJECT

public:

    /// Constructor.
    ClickableModifierLabel(ModifierAction* action, QWidget* parent = nullptr);

    /// Returns the preferred size for this widget.
    QSize sizeHint() const override;

Q_SIGNALS:

    /// Emitted when the label is clicked.
    void clicked(ModifierAction* action);

    /// Emitted when the mouse enters/leaves this label.
    void hovered(ModifierAction* action);

protected:

    /// Paints the label with optional hover highlighting.
    void paintEvent(QPaintEvent* event) override;

    /// Called when the mouse enters the widget area.
    void enterEvent(QEnterEvent* event) override;

    /// Called when the mouse leaves the widget area.
    void leaveEvent(QEvent* event) override;

    /// Called when the mouse button is pressed.
    void mousePressEvent(QMouseEvent* event) override;

private:

    /// The modifier action associated with this label.
    QPointer<ModifierAction> _action;

    /// Whether the mouse is currently hovering over this label.
    bool _hovered = false;
};

/**
 * A popup widget that displays available modifiers in a multi-column card layout,
 * grouped by category.
 */
class ModifierGalleryPopup : public QFrame
{
    Q_OBJECT

public:

    /// Constructor.
    ModifierGalleryPopup(AvailableModifiersModel* model, QWidget* parent = nullptr);

    /// Positions the popup below the given anchor widget and shows it.
    void showBelow(QWidget* anchor);

    /// Updates the popup content from the model.
    void updateContent();

protected:

    /// Handles hover events for the "Get more modifiers..." button.
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:

    /// Emitted when a modifier is selected.
    void modifierSelected(ModifierAction* action);

    /// Emitted when the "Get more modifiers..." button is clicked.
    void getMoreModifiersClicked();

private Q_SLOTS:

    /// Updates the status label when a modifier is hovered.
    void onModifierHovered(ModifierAction* action);

private:

    /// Rebuilds the card layout from the model data.
    void populateCards();

    /// The model providing the available modifiers.
    AvailableModifiersModel* _model;

    /// Container widget for the card columns.
    QWidget* _cardsContainer = nullptr;

    /// Status label displaying the hovered modifier's description.
    QLabel* _statusLabel = nullptr;

    /// The "Get more modifiers..." button.
    QPushButton* _getMoreButton = nullptr;
};

}   // End of namespace
