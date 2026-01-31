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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/mainwin/AvailableModifiersModel.h>
#include "ModifierGalleryPopup.h"

namespace Ovito {

/******************************************************************************
* Constructor for ClickableModifierLabel.
******************************************************************************/
ClickableModifierLabel::ClickableModifierLabel(ModifierAction* action, QWidget* parent)
    : QWidget(parent), _action(action)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setContentsMargins(6, 3, 6, 3);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    // Remove this label when the action is destroyed (e.g., when a modifier template is deleted).
    connect(action, &QObject::destroyed, this, &QObject::deleteLater);
}

/******************************************************************************
* Returns the preferred size for this widget.
******************************************************************************/
QSize ClickableModifierLabel::sizeHint() const
{
    QFontMetrics fm(font());
    QMargins m = contentsMargins();
    QString text = _action ? _action->text() : QString();
    return QSize(fm.horizontalAdvance(text) + m.left() + m.right(),
                 fm.height() + m.top() + m.bottom());
}

/******************************************************************************
* Paints the label with optional hover highlighting.
******************************************************************************/
void ClickableModifierLabel::paintEvent(QPaintEvent*)
{
    // Safety check: action may have been destroyed.
    if(!_action) return;

    QPainter p(this);
    QMargins m = contentsMargins();

    // Check if the action is enabled
    bool enabled = _action->isEnabled();

    if(_hovered && enabled) {
        p.fillRect(rect(), palette().highlight());
        p.setPen(palette().highlightedText().color());
    }
    else if(!enabled) {
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
    }
    else {
        p.setPen(palette().text().color());
    }

    p.drawText(rect().adjusted(m.left(), m.top(), -m.right(), -m.bottom()),
               Qt::AlignLeft | Qt::AlignVCenter, _action->text());
}

/******************************************************************************
* Called when the mouse enters the widget area.
******************************************************************************/
void ClickableModifierLabel::enterEvent(QEnterEvent*)
{
    _hovered = true;
    update();
    Q_EMIT hovered(_action);
}

/******************************************************************************
* Called when the mouse leaves the widget area.
******************************************************************************/
void ClickableModifierLabel::leaveEvent(QEvent*)
{
    _hovered = false;
    update();
    Q_EMIT hovered(nullptr);
}

/******************************************************************************
* Called when the mouse button is pressed.
******************************************************************************/
void ClickableModifierLabel::mousePressEvent(QMouseEvent* event)
{
    if(_action && _action->isEnabled()) {
        Q_EMIT clicked(_action);
    }
    QWidget::mousePressEvent(event);
}

/******************************************************************************
* Constructor for ModifierGalleryPopup.
******************************************************************************/
ModifierGalleryPopup::ModifierGalleryPopup(AvailableModifiersModel* model, QWidget* parent)
    : QFrame(parent, Qt::Popup), _model(model)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // Container for card columns
    _cardsContainer = new QWidget(this);
    mainLayout->addWidget(_cardsContainer);

    // Separator line above the status bar
    QFrame* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(separator);

    // Bottom bar with status label and "Get more modifiers..." button
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 4, 0, 0);

    _statusLabel = new QLabel(this);
    _statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    bottomLayout->addWidget(_statusLabel, 1);

    _getMoreButton = new QPushButton(tr("Get more modifiers..."), this);
    connect(_getMoreButton, &QPushButton::clicked, this, [this]() {
        hide();
        Q_EMIT getMoreModifiersClicked();
    });
    _getMoreButton->installEventFilter(this);
    bottomLayout->addWidget(_getMoreButton);

    mainLayout->addLayout(bottomLayout);

    // Build initial content
    populateCards();
}

/******************************************************************************
* Positions the popup below the given anchor widget and shows it.
******************************************************************************/
void ModifierGalleryPopup::showBelow(QWidget* anchor)
{
    adjustSize();

    // Position below the anchor widget
    QPoint pos = anchor->mapToGlobal(QPoint(0, anchor->height()));

    // Try to right-align with the anchor if popup is wider
    if(width() > anchor->width()) {
        pos.setX(pos.x() + anchor->width() - width());
    }

    // Ensure popup stays within screen bounds
    QScreen* screen = anchor->screen();
    if(screen) {
        QRect screenGeom = screen->availableGeometry();

        // Adjust horizontal position
        if(pos.x() < screenGeom.left()) {
            pos.setX(screenGeom.left());
        }
        else if(pos.x() + width() > screenGeom.right()) {
            pos.setX(screenGeom.right() - width());
        }

        // If popup would extend below screen, show it above the anchor
        if(pos.y() + height() > screenGeom.bottom()) {
            pos.setY(anchor->mapToGlobal(QPoint(0, 0)).y() - height());
        }
    }

    move(pos);
    show();
}

/******************************************************************************
* Updates the popup content from the model.
******************************************************************************/
void ModifierGalleryPopup::updateContent()
{
    populateCards();
    adjustSize();
}

/******************************************************************************
* Rebuilds the card layout from the model data.
******************************************************************************/
void ModifierGalleryPopup::populateCards()
{
    // Clear existing layout
    if(_cardsContainer->layout()) {
        QLayoutItem* item;
        while((item = _cardsContainer->layout()->takeAt(0)) != nullptr) {
            if(item->widget())
                item->widget()->deleteLater();
            delete item;
        }
        delete _cardsContainer->layout();
    }

    int numCategories = _model->categoryCount();
    if(numCategories == 0) return;

    // Determine number of columns based on category count
    int numColumns;
    if(numCategories <= 4)
        numColumns = numCategories;
    else if(numCategories <= 7)
        numColumns = 3;
    else
        numColumns = 4;

    // Create horizontal layout for columns
    QHBoxLayout* hLayout = new QHBoxLayout(_cardsContainer);
    hLayout->setSpacing(12);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setAlignment(Qt::AlignTop);

    // Create column containers
    std::vector<QWidget*> columns(numColumns);
    std::vector<QVBoxLayout*> columnLayouts(numColumns);
    std::vector<int> columnHeights(numColumns, 0);

    for(int c = 0; c < numColumns; ++c) {
        columns[c] = new QWidget;
        columnLayouts[c] = new QVBoxLayout(columns[c]);
        columnLayouts[c]->setSpacing(12);
        columnLayouts[c]->setContentsMargins(0, 0, 0, 0);
        columnLayouts[c]->setAlignment(Qt::AlignTop);
        hLayout->addWidget(columns[c], 0, Qt::AlignTop);
    }

    // Sort categories by size (descending) for better column balancing
    std::vector<int> order(numCategories);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [this](int a, int b) {
        return _model->categoryActions(a).size() > _model->categoryActions(b).size();
    });

    // Assign each category to the shortest column
    for(int idx : order) {
        const std::vector<ModifierAction*>& categoryActions = _model->categoryActions(idx);
        if(categoryActions.empty()) continue;

        // Find column with minimum height
        int minCol = 0;
        for(int c = 1; c < numColumns; ++c) {
            if(columnHeights[c] < columnHeights[minCol])
                minCol = c;
        }

        // Create card widget for this category with rounded corners and subtle styling
        QFrame* card = new QFrame;
        card->setFrameShape(QFrame::NoFrame);

        // Get theme-adaptive colors
        QPalette pal = card->palette();
        QColor cardBg = pal.color(QPalette::Base);
        QColor borderColor = pal.color(QPalette::Mid);
        // Compute header color with guaranteed contrast: darken in light mode, lighten in dark mode
        bool isDarkMode = cardBg.lightness() < 128;
        QColor headerBg = isDarkMode ? cardBg.lighter(130) : cardBg.darker(108);

        // Apply stylesheet for rounded corners and subtle border
        card->setStyleSheet(QStringLiteral(
            "QFrame { background-color: %1; border: 1px solid %2; border-radius: 6px; }")
            .arg(cardBg.name(), borderColor.name()));

        QVBoxLayout* cardLayout = new QVBoxLayout(card);
        cardLayout->setSpacing(0);
        cardLayout->setContentsMargins(1, 1, 1, 4);  // 1px inner margin to avoid overlap with card border

        // Category header with rounded top corners and bottom border for separation
        QLabel* header = new QLabel(_model->categoryName(idx));
        QFont headerFont = header->font();
        headerFont.setBold(true);
        header->setFont(headerFont);
        header->setStyleSheet(QStringLiteral(
            "QLabel { background-color: %1; border-top-left-radius: 5px; border-top-right-radius: 5px; "
            "border: 0px; padding: 4px 6px; }")
            .arg(headerBg.name()));
        cardLayout->addWidget(header);

        // Modifier items
        for(ModifierAction* action : categoryActions) {
            ClickableModifierLabel* itemLabel = new ClickableModifierLabel(action, card);
            connect(itemLabel, &ClickableModifierLabel::clicked, this, [this](ModifierAction* action) {
                hide();
                Q_EMIT modifierSelected(action);
            });
            connect(itemLabel, &ClickableModifierLabel::hovered, this, &ModifierGalleryPopup::onModifierHovered);
            cardLayout->addWidget(itemLabel);
        }

        columnLayouts[minCol]->addWidget(card);
        columnHeights[minCol] += static_cast<int>(categoryActions.size()) + 1; // +1 for header
    }

    // Add stretch to each column to keep cards at top
    for(int c = 0; c < numColumns; ++c) {
        columnLayouts[c]->addStretch();
    }
}

/******************************************************************************
* Updates the status label when a modifier is hovered.
******************************************************************************/
void ModifierGalleryPopup::onModifierHovered(ModifierAction* action)
{
    if(action)
        _statusLabel->setText(action->statusTip());
    else
        _statusLabel->clear();
}

/******************************************************************************
* Handles hover events for the "Get more modifiers..." button.
******************************************************************************/
bool ModifierGalleryPopup::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == _getMoreButton) {
        if(event->type() == QEvent::Enter) {
            _statusLabel->setText(tr("Browse and install additional modifiers from the OVITO extensions repository."));
        }
        else if(event->type() == QEvent::Leave) {
            _statusLabel->clear();
        }
    }
    return QFrame::eventFilter(watched, event);
}

}   // End of namespace
