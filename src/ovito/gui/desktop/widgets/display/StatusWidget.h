////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/dataset/pipeline/PipelineStatus.h>

namespace Ovito {

/**
 * \brief A widget that displays a PipelineStatus structure.
 */
class OVITO_GUI_EXPORT StatusWidget : public QLabel
{
    Q_OBJECT

public:

    /// Constructor.
    explicit StatusWidget(QWidget* parent = nullptr);

    /// Sets the status to be displayed by the widget.
    void setStatus(const PipelineStatus& status);

    /// Resets the widget to not display any status.
    void clearStatus() { setStatus({}); }

    /// Returns the color indicating the current status.
    [[nodiscard]] QColor statusColor() const;

    /// Returns the default color indicating a success status.
    [[nodiscard]] QColor defaultColor() const;

Q_SIGNALS:

    /// Emitted when the user clicks on a link in the status text.
    void linkActivated(const QString& link);

private Q_SLOTS:

    /// Shows the tooltip popup.
    void showTooltipPopup();

protected:
    /// Returns the minimum size of the widget.
    [[nodiscard]] virtual QSize minimumSizeHint() const override;

    /// Returns the recommended size of the widget.
    [[nodiscard]] virtual QSize sizeHint() const override;

    /// Calculate the height of the widget based on the font
    [[nodiscard]] int calculateHeight() const;

    // Apply the widget height
    void setHeight();

    /// Calculate the position of the overlay label.
    [[nodiscard]] QPoint calculateOverlayLabelPosition() const;

    // Sets the visibility of the overlay label based on the status text.
    bool toggleOverlayLabelVisibility();

    /// Updates the widget's palette based on the current status type.
    void updatePalette();

    /// Paints the widget's border.
    virtual void paintEvent(QPaintEvent* event) override;

    /// Event filter to watch the internal label and the tooltip popup for
    /// enter/leave events.
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// Open the tooltip immediately when the widget is clicked.
    virtual void mousePressEvent(QMouseEvent* event) override;

    /// Handle widget resize to reposition overlay label.
    virtual void resizeEvent(QResizeEvent* event) override;

    /// Overlay label cannot be placed in the constructor because the
    /// the widget's size is not yet set. Deferred placement here.
    virtual void showEvent(QShowEvent* e) override;

    /// Handles widget state changes.
    virtual void changeEvent(QEvent* event) override;

    [[nodiscard]] virtual bool hasHeightForWidth() const override { return false; }

private:
    /// The current status type displayed by the widget.
    PipelineStatus::StatusType _statusType = PipelineStatus::StatusType::Success;

    // Tooltip support
    QTimer* _tooltipTimer = nullptr;
    QWidget* _tooltipPopup = nullptr;
    QLabel* _tooltipPopupLabel = nullptr;
    static constexpr int _tooltipDelayMs = 250;  ///< hover delay in milliseconds

    // Overlay label
    QLabel* _overlayLabel = nullptr;

    bool _inUpdatePalette = false;  ///< Flag to avoid recursive palette updates
};

}   // End of namespace
