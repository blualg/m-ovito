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
#include <ovito/gui/desktop/widgets/general/ElidedTextLabel.h>
#include "ActionCardsPopup.h"

namespace Ovito {

/******************************************************************************
* Constructor for ClickableActionLabel.
******************************************************************************/
ClickableActionLabel::ClickableActionLabel(QAction* action, QWidget* parent)
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
QSize ClickableActionLabel::sizeHint() const
{
    QFontMetrics fm(_action ? _action->font() : font());
    QMargins m = contentsMargins();
    QString text = _action ? _action->text() : QString();
    return QSize(fm.horizontalAdvance(text) + m.left() + m.right(),
                 fm.height() + m.top() + m.bottom());
}

/******************************************************************************
* Programmatically sets the hovered state (used by keyboard search).
******************************************************************************/
void ClickableActionLabel::setHovered(bool isHovered)
{
    if(_hovered != isHovered) {
        _hovered = isHovered;
        update();
        Q_EMIT hovered(isHovered ? _action : nullptr);
    }
}

/******************************************************************************
* Paints the label with optional hover highlighting.
******************************************************************************/
void ClickableActionLabel::paintEvent(QPaintEvent*)
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
    p.setFont(_action->font());

    p.drawText(rect().adjusted(m.left(), m.top(), -m.right(), -m.bottom()),
               Qt::AlignLeft | Qt::AlignVCenter, _action->text());
}

/******************************************************************************
* Called when the mouse enters the widget area.
******************************************************************************/
void ClickableActionLabel::enterEvent(QEnterEvent*)
{
    if(_action && _action->isEnabled())
        setHovered(true);
}

/******************************************************************************
* Called when the mouse leaves the widget area.
******************************************************************************/
void ClickableActionLabel::leaveEvent(QEvent*)
{
    if(_action && _action->isEnabled())
        setHovered(false);
}

/******************************************************************************
* Called when the mouse is moved within the widget area.
******************************************************************************/
void ClickableActionLabel::mouseMoveEvent(QMouseEvent* event)
{
    if(_action && _action->isEnabled())
        setHovered(true);

    QWidget::mouseMoveEvent(event);
}

/******************************************************************************
* Called when the mouse button is pressed.
******************************************************************************/
void ClickableActionLabel::mousePressEvent(QMouseEvent* event)
{
    if(_action && _action->isEnabled()) {
        Q_EMIT clicked(_action);
    }
    QWidget::mousePressEvent(event);
}

/******************************************************************************
* Constructor for ActionCardsPopup.
******************************************************************************/
ActionCardsPopup::ActionCardsPopup(QAbstractItemModel* model, const QString& getMoreButtonText, QWidget* parent)
    : QFrame(parent, Qt::Popup), _model(model)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setFocusPolicy(Qt::StrongFocus);

    // Timer that resets the keyboard search string after some time of inactivity.
    _searchResetTimer = new QTimer(this);
    _searchResetTimer->setSingleShot(true);
    _searchResetTimer->setInterval(QApplication::keyboardInputInterval());
    connect(_searchResetTimer, &QTimer::timeout, this, [this]() {
        _searchString.clear();
    });

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

    _statusLabel = new ElidedTextLabel(Qt::ElideRight, this);
    _statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    _statusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bottomLayout->addWidget(_statusLabel, 1);

    _getMoreButton = new QPushButton(getMoreButtonText, this);
    connect(_getMoreButton, &QPushButton::clicked, this, [this]() {
        hide();
        Q_EMIT getMoreActionsClicked();
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
void ActionCardsPopup::showBelow(QWidget* anchor)
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
    setFocus();
}

/******************************************************************************
* Updates the popup content from the model.
******************************************************************************/
void ActionCardsPopup::updateContent()
{
    populateCards();
    adjustSize();
}

/******************************************************************************
* Rebuilds the card layout from the model data.
******************************************************************************/
void ActionCardsPopup::populateCards()
{
    // Reset keyboard search state whenever cards are rebuilt.
    _allLabels.clear();
    _highlightedLabel.clear();
    _searchString.clear();
    if(_searchResetTimer)
        _searchResetTimer->stop();

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

    int numCategories = _model->rowCount();
    if(numCategories == 0) return;

    // Determine number of columns based on category count
    int numColumns;
    if(numCategories <= 1)
        numColumns = numCategories;
    else if(numCategories <= 4)
        numColumns = 2;
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

    // Get category sizes and sort them.
    std::vector<int> actionsPerCategory(numCategories);
    for(int i = 0; i < numCategories; ++i) {
        actionsPerCategory[i] = _model->rowCount(_model->index(i, 0));
    }

    // Sort categories by size (descending) for better column balancing
    std::vector<int> order(numCategories);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return actionsPerCategory[a] > actionsPerCategory[b];
    });

    // Assign each category to the shortest column
    for(int idx : order) {
        if(actionsPerCategory[idx] == 0)
            continue;

        std::vector<QAction*> categoryActions(actionsPerCategory[idx]);
        for(int row = 0; row < actionsPerCategory[idx]; ++row) {
            QModelIndex actionIdx = _model->index(row, 0, _model->index(idx, 0));
            categoryActions[row] = _model->data(actionIdx, Qt::UserRole).value<QAction*>();
            OVITO_ASSERT(categoryActions[row] != nullptr);
        }

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
        QLabel* header = new QLabel(_model->data(_model->index(idx, 0), Qt::DisplayRole).toString());
        QFont headerFont = header->font();
        headerFont.setBold(true);
        header->setFont(headerFont);
        header->setStyleSheet(QStringLiteral(
            "QLabel { background-color: %1; border-top-left-radius: 5px; border-top-right-radius: 5px; "
            "border: 0px; padding: 4px 6px; }")
            .arg(headerBg.name()));
        cardLayout->addWidget(header);

        // Action items
        for(QAction* action : categoryActions) {
            ClickableActionLabel* itemLabel = new ClickableActionLabel(action, card);
            connect(itemLabel, &ClickableActionLabel::clicked, this, [this](QAction* action) {
                hide();
                action->trigger();
            });
            connect(itemLabel, &ClickableActionLabel::hovered, this, &ActionCardsPopup::onActionHovered);
            cardLayout->addWidget(itemLabel);
            _allLabels.push_back(itemLabel);
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
* Updates the status label when an action is hovered.
******************************************************************************/
void ActionCardsPopup::onActionHovered(QAction* action)
{
    // When the mouse enters a new item, dismiss any previous highlight.
    if(_highlightedLabel && _highlightedLabel->action() != action) {
        _highlightedLabel->setHovered(false);
    }

    if(action)
        _statusLabel->setText(action->statusTip());
    else
        _statusLabel->clear();

    if(action)
        _highlightedLabel = qobject_cast<ClickableActionLabel*>(QObject::sender());
    else
        _highlightedLabel = nullptr;
}

/******************************************************************************
* Handles hover events for the "Get more XXX..." button.
******************************************************************************/
bool ActionCardsPopup::eventFilter(QObject* watched, QEvent* event)
{
    if(watched == _getMoreButton) {
        if(event->type() == QEvent::Enter) {
            _statusLabel->setText(tr("Browse and install additional extensions from the OVITO extensions repository."));
        }
        else if(event->type() == QEvent::Leave) {
            _statusLabel->clear();
        }
    }
    return QFrame::eventFilter(watched, event);
}

/******************************************************************************
* Moves the keyboard highlight to the given label, or clears it if nullptr.
******************************************************************************/
void ActionCardsPopup::setKeyboardHighlight(ClickableActionLabel* label)
{
    if(_highlightedLabel == label)
        return;
    if(label) {
        label->setHovered(true);
        OVITO_ASSERT(_highlightedLabel == label);
    }
    else if(_highlightedLabel) {
        _highlightedLabel->setHovered(false);
        OVITO_ASSERT(_highlightedLabel == nullptr);
    }
}

/******************************************************************************
* Searches all labels for the first enabled item whose text starts with
* _searchString (case-insensitive) and highlights it.
******************************************************************************/
void ActionCardsPopup::performKeyboardSearch()
{
    if(_searchString.isEmpty()) {
        setKeyboardHighlight(nullptr);
        return;
    }
    for(ClickableActionLabel* label : _allLabels) {
        if(!label) continue;
        QAction* action = label->action();
        if(!action || !action->isEnabled()) continue;
        if(action->text().startsWith(_searchString, Qt::CaseInsensitive)) {
            setKeyboardHighlight(label);
            return;
        }
    }
    // No match: clear the highlight to give visual feedback.
    setKeyboardHighlight(nullptr);
}

/******************************************************************************
* Handles keyboard input for quick item search and activation.
******************************************************************************/
void ActionCardsPopup::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if(_highlightedLabel) {
            QAction* action = _highlightedLabel->action();
            if(action && action->isEnabled()) {
                hide();
                action->trigger();
                return;
            }
        }
        event->accept();
        return;
    }

    if(event->key() == Qt::Key_Backspace) {
        if(!_searchString.isEmpty()) {
            _searchString.chop(1);
            _searchResetTimer->start();
            performKeyboardSearch();
        }
        event->accept();
        return;
    }

    const QString text = event->text();
    if(!text.isEmpty() && text.at(0).isPrint()) {
        _searchString += text;
        _searchResetTimer->start();
        performKeyboardSearch();
        event->accept();
        return;
    }

    QFrame::keyPressEvent(event);
}

}   // End of namespace
