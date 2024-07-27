////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/viewport/ViewportMenu.h>
#include <ovito/gui/vpwidget/WidgetViewportWindow.h>
#include <ovito/gui/base/actions/ActionManager.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/viewport/ViewportSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/PluginManager.h>
#include "ViewportsPanel.h"

namespace Ovito {

/******************************************************************************
* The constructor of the viewports panel class.
******************************************************************************/
ViewportsPanel::ViewportsPanel(MainWindow& mainWindow) : _mainWindow(mainWindow)
{
    // Activate the new viewport layout as soon as a new state file is loaded.
    connect(&mainWindow.datasetContainer(), &DataSetContainer::viewportConfigReplaced, this, &ViewportsPanel::onViewportConfigurationReplaced);

    // Track viewport input mode changes.
    connect(mainWindow.viewportInputManager(), &ViewportInputManager::inputModeChanged, this, &ViewportsPanel::onInputModeChanged);

    // Show a context menu when the user clicks a viewport's caption.
    connect(mainWindow.viewportInputManager(), &ViewportInputManager::contextMenuRequested, this, &ViewportsPanel::onViewportMenuRequested);

    // Repaint when auto-key animation mode is toggled.
    connect(mainWindow.actionManager()->getAction(ACTION_AUTO_KEY_MODE_TOGGLE), &QAction::toggled, this, qOverload<>(&ViewportsPanel::update));

    // Repaint the viewport borders when another viewport has been activated.
    connect(&mainWindow.datasetContainer(), &DataSetContainer::activeViewportChanged, this, qOverload<>(&ViewportsPanel::update));

    // Update layout when a viewport has been maximized or restored.
    connect(&mainWindow.datasetContainer(), &DataSetContainer::maximizedViewportChanged, this, &ViewportsPanel::invalidateWindowLayout);

    // Update the viewport window positions when the viewport layout is modified.
    connect(&mainWindow.datasetContainer(), &DataSetContainer::viewportLayoutChanged, this, &ViewportsPanel::invalidateWindowLayout);

    // Prevent the viewports from collpasing and disappearing completely.
    setMinimumSize(40, 40);

    // Set the background color of the panel.
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(80, 80, 80));
    setPalette(std::move(pal));

    // Enable mouse tracking to implement hover effect for splitter handles.
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover);
}

/******************************************************************************
* Factory method which creates a new viewport window widget.
******************************************************************************/
OORef<WidgetViewportWindow> ViewportsPanel::createViewportWindow(Viewport& vp, MainWindow& mainWindow, QWidget* parent)
{
    // Select the viewport window implementation to use.
    QByteArray selectedGraphicsApi = qgetenv("OVITO_VIEWPORT_RENDERER");
#if 0
    QSettings settings;
    if(selectedGraphicsApi.isEmpty())
        selectedGraphicsApi = settings.value("rendering/selected_graphics_api").toString().toUtf8();
#endif

    // Select the viewport window implementation to use.
    OvitoClassPtr windowClass = PluginManager::instance().findClass("OpenGLRendererWindow", "OpenGLViewportWindow");
#ifdef OVITO_BUILD_PROFESSIONAL
    if(selectedGraphicsApi.compare("anari", Qt::CaseInsensitive) == 0) {
        windowClass = PluginManager::instance().findClass("AnariRendererWindow", "AnariViewportWindow");
    }
#endif

    // Instantiate the selected viewport window implementation.
    if(windowClass) {
        OORef<WidgetViewportWindow> window = static_object_cast<WidgetViewportWindow>(windowClass->createInstance());
        window->initializeWindow(&vp, mainWindow, parent);

        // Handle fatal rendering errors by quitting the application.
        connect(window.get(), &ViewportWindow::fatalError, &mainWindow, [&mainWindow](Exception& ex) {
            ex.prependGeneralMessage(tr("An unexpected error occurred while rendering the viewports. The program will quit."));
            mainWindow.exitWithFatalError(ex);
        });

        return window;
    }

    return {};
}

/******************************************************************************
* Returns the window that is associated with the given viewport (if any).
******************************************************************************/
WidgetViewportWindow* ViewportsPanel::viewportWindow(Viewport* vp)
{
    // Check if the viewport already has an associated window.
    for(const auto& window : _viewportWindows) {
        if(window->viewport() == vp)
            return window;
    }
    return nullptr;
}

/******************************************************************************
* Returns the widget that hosts the given viewport.
* The widget is automatically created if it does not exist yet.
******************************************************************************/
QWidget* ViewportsPanel::viewportWidget(Viewport* vp)
{
    // Check if the viewport already has an associated window.
    if(WidgetViewportWindow* window = viewportWindow(vp))
        return window->widget();

    // Create the viewport window if it hasn't been created for this viewport yet.
    if(!_graphicsInitializationErrorOccurred) {
        try {
            OORef<WidgetViewportWindow> viewportWindow = createViewportWindow(*vp, _mainWindow, this);
            if(!viewportWindow || !viewportWindow->widget())
                throw Exception(tr("Failed to create viewport window or there is no realtime graphics implementation available. Please check your OVITO installation and the graphics capabilities of your system."));
            if(_viewportConfig && _viewportConfig->activeViewport() == vp)
                viewportWindow->widget()->setFocus();
            _viewportWindows.push_back(viewportWindow);
            return viewportWindow->widget();
        }
        catch(const Exception& ex) {
            _graphicsInitializationErrorOccurred = true;
            _mainWindow.reportError(ex, true);
        }
    }

    return nullptr;
}

/******************************************************************************
* Displays the context menu for a viewport window.
******************************************************************************/
void ViewportsPanel::onViewportMenuRequested(ViewportWindow* viewportWindow, const QPoint& pos)
{
    // Get the viewport's widget.
    if(QWidget* widget = viewportWidget(viewportWindow->viewport())) {

        // Create the context menu for the viewport.
        ViewportMenu contextMenu(_mainWindow, viewportWindow, widget);

        // Show menu.
        contextMenu.show(pos);
    }
}

/******************************************************************************
* This is called when a new viewport configuration has been loaded.
******************************************************************************/
void ViewportsPanel::onViewportConfigurationReplaced(ViewportConfiguration* newViewportConfiguration)
{
    _viewportConfig = newViewportConfiguration;

    // Create the interactive viewport windows.
    recreateViewportWindows();
}

/******************************************************************************
* Destroys all viewport windows in the panel and recreates them.
******************************************************************************/
void ViewportsPanel::recreateViewportWindows()
{
    // Delete all widgets of the existing viewport windows immediately.
    for(auto& window : _viewportWindows)
        delete window->widget();
    // Release all viewport windows.
    _viewportWindows.clear();

    // Now all child widgets should be gone.
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    OVITO_ASSERT(findChildren<QWidget*>(Qt::FindDirectChildrenOnly).empty());
#else
    OVITO_ASSERT(findChildren<QWidget*>(QString{}, Qt::FindDirectChildrenOnly).empty());
#endif

    if(_viewportConfig) {
        // Layout viewport widgets.
        // This function implicitly creates the windows for all viewports.
        layoutViewports();
    }
}

/******************************************************************************
* This is called when the current viewport input mode has changed.
******************************************************************************/
void ViewportsPanel::onInputModeChanged(ViewportInputMode* oldMode, ViewportInputMode* newMode)
{
    disconnect(_activeModeCursorChangedConnection);
    if(newMode) {
        _activeModeCursorChangedConnection = connect(newMode, &ViewportInputMode::curserChanged, this, &ViewportsPanel::onViewportModeCursorChanged);
        onViewportModeCursorChanged(newMode->cursor());
    }
    else onViewportModeCursorChanged(cursor());
}

/******************************************************************************
* This is called when the mouse cursor of the active input mode has changed.
******************************************************************************/
void ViewportsPanel::onViewportModeCursorChanged(const QCursor& cursor)
{
    if(!_viewportConfig) return;

    for(const auto& window : _viewportWindows) {
        window->setCursor(cursor);
    }
}

/******************************************************************************
* Renders the borders of the viewports.
******************************************************************************/
void ViewportsPanel::paintEvent(QPaintEvent* event)
{
    if(!_viewportConfig) return;

    // Get the active viewport and its associated Qt widget.
    Viewport* vp = _viewportConfig->activeViewport();
    if(!vp)
        return;
    QWidget* vpWidget = viewportWidget(vp);
    if(!vpWidget || vpWidget->isHidden())
        return;

    QPainter painter(this);

    // Highlight the splitter handle that is currently under the mouse cursor.
    if(_hoveredSplitter != -1 && _draggedSplitter == -1) {
        OVITO_ASSERT(_hoveredSplitter < _splitterRegions.size());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(_highlightSplitter ? QColor(0x4B, 0x7A, 0xC9) : QColor(120, 120, 120)));
        painter.drawRect(_splitterRegions[_hoveredSplitter].area);
    }

    if(_hoveredSplitter == -1 || !_highlightSplitter) {
        // Choose a color for the viewport border.
        Color borderColor;
        if(_mainWindow.actionManager()->getAction(ACTION_AUTO_KEY_MODE_TOGGLE)->isChecked())
            borderColor = Viewport::viewportColor(ViewportSettings::COLOR_ANIMATION_MODE);
        else
            borderColor = Viewport::viewportColor(ViewportSettings::COLOR_ACTIVE_VIEWPORT_BORDER);

        // Render a border around the active viewport.
        painter.setPen((QColor)borderColor);
        painter.setBrush(Qt::NoBrush);
        QRect rect = vpWidget->geometry();
        rect.adjust(-1, -1, 0, 0);
        painter.drawRect(rect);
        rect.adjust(-1, -1, 1, 1);
        painter.drawRect(rect);
    }

    // Highlight the splitter handle that is currently being dragged.
    if(_draggedSplitter != -1) {
        OVITO_ASSERT(_draggedSplitter < _splitterRegions.size());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(QColor(0x4B, 0x7A, 0xC9)));
        painter.drawRect(_splitterRegions[_draggedSplitter].area);
    }
}

/******************************************************************************
* Handles size event for the window.
******************************************************************************/
void ViewportsPanel::resizeEvent(QResizeEvent* event)
{
    layoutViewports();
}

/******************************************************************************
* Requests a relayout of the viewport windows.
******************************************************************************/
void ViewportsPanel::invalidateWindowLayout()
{
    if(!_relayoutRequested) {
        _relayoutRequested = true;
        QMetaObject::invokeMethod(this, "layoutViewports", Qt::QueuedConnection);
    }
}

/******************************************************************************
* Performs the layout of the viewport windows.
******************************************************************************/
void ViewportsPanel::layoutViewports()
{
    _relayoutRequested = false;
    _splitterRegions.clear();
    _hoveredSplitter = -1;
    _highlightSplitter = false;
    _highlightSplitterTimer.stop();
    if(!_viewportConfig)
        return;

    // Get the list of all viewports.
    const auto& viewports = _viewportConfig->viewports();

    // Delete stale viewport windows belonging to removed viewports.
    std::erase_if(_viewportWindows, [&](const auto& window) {
        return boost::algorithm::none_of(viewports, [&](Viewport* vp) { return window->viewport() == vp; });
    });

    // Create new viewport windows for viewports that don't have one yet.
    for(Viewport* viewport : viewports) {
        viewportWidget(viewport);
    }

    // Get the viewport that is currently maximized.
    if(Viewport* maximizedViewport = _viewportConfig->maximizedViewport()) {
        // If there is a maximized viewport, hide all other viewport windows.
        for(Viewport* viewport : viewports) {
            if(QWidget* widget = viewportWidget(viewport)) {
                if(widget->parentWidget() == this) {
                    widget->setVisible(maximizedViewport == viewport);
                    if(maximizedViewport == viewport) {
                        // Fill the entire panel with the maximized viewport window.
                        QRect r = rect().adjusted(_windowInset, _windowInset, -_windowInset, -_windowInset);
                        if(widget->geometry() != r) {
                            widget->setGeometry(r);
                            update();
                        }
                    }
                }
            }
        }
    }
    else {
        // Perform a reculation of the nested layout.
        layoutViewportsRecursive(_viewportConfig->layoutRootCell(), rect());
    }

    // Automatically activate a different viewport if the currently active one has been hidden.
    if(_viewportConfig->maximizedViewport() && !viewportWindow(_viewportConfig->maximizedViewport())) {
        _mainWindow.handleExceptions([&] {
            _viewportConfig->setMaximizedViewport(viewports.empty() ? nullptr : viewports.front());
            _viewportConfig->setActiveViewport(_viewportConfig->maximizedViewport());
        });
    }
    if(_viewportConfig->activeViewport() && !viewportWindow(_viewportConfig->activeViewport())) {
        _mainWindow.handleExceptions([&] {
            _viewportConfig->setActiveViewport(viewports.empty() ? nullptr : viewports.front());
        });
    }
}

/******************************************************************************
* Recursive helper function for laying out the viewport windows.
******************************************************************************/
void ViewportsPanel::layoutViewportsRecursive(ViewportLayoutCell* layoutCell, const QRect& rect)
{
    if(!layoutCell)
        return;

    if(layoutCell->viewport()) {
        if(QWidget* widget = viewportWidget(layoutCell->viewport())) {
            QRect r = rect.adjusted(_windowInset, _windowInset, -_windowInset, -_windowInset);
            if(widget->geometry() != r) {
                widget->setGeometry(r);
                update();
            }
            widget->setVisible(true);
        }
    }
    else if(!layoutCell->children().empty()) {
        size_t index = 0;
        QRect childRect = rect;
        int effectiveAvailableSpace = (layoutCell->splitDirection() == ViewportLayoutCell::Horizontal) ? rect.width() : rect.height();
        effectiveAvailableSpace -= _splitterSize * (layoutCell->children().size() - 1);
        FloatType totalChildWeights = layoutCell->totalChildWeights();
        if(totalChildWeights <= 0.0) totalChildWeights = 1.0;
        FloatType dragFactor = totalChildWeights / std::max(1, effectiveAvailableSpace);
        FloatType x = 0.0;
        for(ViewportLayoutCell* child : layoutCell->children()) {
            if(index != layoutCell->children().size() - 1) {
                FloatType weight = 0;
                if(index < layoutCell->childWeights().size())
                    weight = layoutCell->childWeights()[index];
                QRect splitterArea = childRect;
                if(layoutCell->splitDirection() == ViewportLayoutCell::Horizontal) {
                    int base = rect.left() + index * _splitterSize;
                    childRect.setLeft(base + effectiveAvailableSpace * (x / totalChildWeights));
                    childRect.setWidth(effectiveAvailableSpace * (weight / totalChildWeights));
                    splitterArea.moveLeft(childRect.right()+1 - _windowInset);
                    splitterArea.setWidth(_splitterSize + 2 * _windowInset);
                }
                else {
                    int base = rect.top() + index * _splitterSize;
                    childRect.setTop(base + effectiveAvailableSpace * (x / totalChildWeights));
                    childRect.setHeight(effectiveAvailableSpace * (weight / totalChildWeights));
                    splitterArea.moveTop(childRect.bottom()+1 - _windowInset);
                    splitterArea.setHeight(_splitterSize + 2*_windowInset);
                }
                _splitterRegions.push_back({ splitterArea, layoutCell, index, dragFactor });
                x += weight;
            }
            else {
                if(layoutCell->splitDirection() == ViewportLayoutCell::Horizontal) {
                    int base = rect.left() + index * _splitterSize;
                    childRect.setLeft(base + effectiveAvailableSpace * (x / totalChildWeights));
                    childRect.setRight(rect.right());
                }
                else {
                    int base = rect.top() + index * _splitterSize;
                    childRect.setTop(base + effectiveAvailableSpace * (x / totalChildWeights));
                    childRect.setBottom(rect.bottom());
                }
            }
            layoutViewportsRecursive(child, childRect);
            index++;
        }
    }
}

/******************************************************************************
* Handles mouse input events.
******************************************************************************/
void ViewportsPanel::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        OVITO_ASSERT(_draggedSplitter == -1);
        int index = 0;
        for(const auto& region : _splitterRegions) {
            if(region.area.contains(event->pos())) {
                _draggedSplitter = index;
                _hoveredSplitter = index;
                _undoTransaction.begin(_mainWindow, tr("Resize viewports"));
                _dragStartPos = event->pos();
                update(region.area);
                break;
            }
            index++;
        }
    }
}

/******************************************************************************
* Handles mouse input events.
******************************************************************************/
void ViewportsPanel::mouseMoveEvent(QMouseEvent* event)
{
    if(_draggedSplitter != -1) {
        // Temporarily block the viewportLayoutChanged() signal from the DataSetContainer to avoid
        // an unnecessary relayout of the viewport windows while resetting the undo operation.
        QSignalBlocker signalBlocker(&_mainWindow.datasetContainer());
        _undoTransaction.revert();
        signalBlocker.unblock();

        _mainWindow.performActions(_undoTransaction, [&] {
            const SplitterRectangle& splitter = _splitterRegions[_draggedSplitter];
            ViewportLayoutCell* parentCell = splitter.cell;

            // Convert mouse motion from pixels to relative size coordinates.
            FloatType delta;
            if(parentCell->splitDirection() == ViewportLayoutCell::Horizontal)
                delta = (event->pos().x() - _dragStartPos.x()) * splitter.dragFactor;
            else
                delta = (event->pos().y() - _dragStartPos.y()) * splitter.dragFactor;

            // Minimum size a cell may have.
            FloatType minWeight = 0.1 * parentCell->totalChildWeights();

            // Apply movement within bounds.
            auto childWeights = parentCell->childWeights();
            OVITO_ASSERT(childWeights.size() > splitter.childCellIndex + 1);
            delta = qBound(minWeight - childWeights[splitter.childCellIndex], delta, childWeights[splitter.childCellIndex + 1] - minWeight);
            childWeights[splitter.childCellIndex] += delta;
            childWeights[splitter.childCellIndex + 1] -= delta;

            // Set the new split weights.
            parentCell->setChildWeights(std::move(childWeights));
        });
    }
    else if(event->button() == Qt::NoButton) {
        int index = 0;
        for(const auto& region : _splitterRegions) {
            if(region.area.contains(event->pos())) {
                if(_hoveredSplitter != index) {
                    if(_hoveredSplitter != -1)
                        update(_splitterRegions[_hoveredSplitter].area);
                    _hoveredSplitter = index;
                    update(region.area);
                    _highlightSplitterTimer.start(500, this);
                }
                break;
            }
            index++;
        }
        if(index == _splitterRegions.size() && _hoveredSplitter != -1) {
            const auto& region = _splitterRegions[_hoveredSplitter];
            _hoveredSplitter = -1;
            _highlightSplitter = false;
            _highlightSplitterTimer.stop();
            update(region.area);
        }
    }
}

/******************************************************************************
* Handles mouse input events.
******************************************************************************/
void ViewportsPanel::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        if(_draggedSplitter != -1) {
            const auto& region = _splitterRegions[_draggedSplitter];
            _hoveredSplitter = _draggedSplitter;
            _draggedSplitter = -1;
            _undoTransaction.commit();
            update(region.area);
        }
    }
    else if(event->button() == Qt::RightButton) {
        for(const auto& region : _splitterRegions) {
            if(region.area.contains(event->pos())) {
                showSplitterContextMenu(region, event->pos());
            }
        }
    }
}

/******************************************************************************
* Handles general events of the widget.
******************************************************************************/
bool ViewportsPanel::event(QEvent* event)
{
    if(event->type() == QEvent::HoverLeave) {
        if(_hoveredSplitter != -1) {
            const auto& region = _splitterRegions[_hoveredSplitter];
            _hoveredSplitter = -1;
            _highlightSplitter = false;
            _highlightSplitterTimer.stop();
            update(region.area);
        }
    }
    else if(event->type() == QEvent::HoverMove) {
        if(_draggedSplitter == -1 && _hoveredSplitter != -1) {
            if(boost::algorithm::none_of(_splitterRegions, [&](const auto& region) {
                return region.area.contains(static_cast<QHoverEvent*>(event)->position().toPoint());
            })) {
                const auto& region = _splitterRegions[_hoveredSplitter];
                _hoveredSplitter = -1;
                _highlightSplitter = false;
                _highlightSplitterTimer.stop();
                update(region.area);
            }
        }
    }
    else if(event->type() == QEvent::Timer) {
        _highlightSplitterTimer.stop();
        if(_hoveredSplitter != -1) {
            _highlightSplitter = true;
            update(_splitterRegions[_hoveredSplitter].area);
        }
    }
    return QWidget::event(event);
}

/******************************************************************************
* Displays the context menu associated with a splitter handle.
******************************************************************************/
void ViewportsPanel::showSplitterContextMenu(const SplitterRectangle& splitter, const QPoint& mousePos)
{
    // Create the context menu for the splitter handle.
    QMenu contextMenu(this);

    // Action that resets the size of all sub-cells to evenly distribute the splitter positions.
    QAction* distributeEvenlyAction = contextMenu.addAction(tr("Resize evenly"));
    distributeEvenlyAction->setEnabled(!splitter.cell->isEvenlySubdivided());
    connect(distributeEvenlyAction, &QAction::triggered, this, [&]() {
        _mainWindow.performTransaction(tr("Resize viewports"), [&]() {
            splitter.cell->setChildWeights(std::vector<FloatType>(splitter.cell->children().size(), 1.0));
        });
    });
    contextMenu.addSeparator();

    // Action that inserts a new viewport into the layout.
    QAction* insertViewAction = contextMenu.addAction(tr("Insert new viewport"));
    connect(insertViewAction, &QAction::triggered, this, [&]() {
        Viewport* adjacentViewport = nullptr;
        ViewportLayoutCell* adjacentCell = splitter.cell->children()[splitter.childCellIndex];
        while(adjacentCell && !adjacentViewport) {
            adjacentViewport = adjacentCell->viewport();
            if(!adjacentCell->children().empty())
                adjacentCell = adjacentCell->children().back();
        }
        _mainWindow.performTransaction(tr("Insert viewport"), [&]() {
            OORef<ViewportLayoutCell> newCell = OORef<ViewportLayoutCell>::create();
            newCell->setViewport(CloneHelper::cloneSingleObject(adjacentViewport, true));
            _viewportConfig->setActiveViewport(newCell->viewport());
            splitter.cell->insertChild(splitter.childCellIndex + 1, std::move(newCell), splitter.cell->childWeights()[splitter.childCellIndex]);
        });
    });
    contextMenu.addSeparator();

    // Action that removes the child cell on one side.
    QAction* deleteCell1Action = new QAction(&contextMenu);
    if(splitter.cell->children()[splitter.childCellIndex] && !splitter.cell->children()[splitter.childCellIndex]->viewport())
        deleteCell1Action->setText((splitter.cell->splitDirection() == ViewportLayoutCell::Horizontal) ? tr("Delete viewports on left") : tr("Delete viewports above"));
    else
        deleteCell1Action->setText((splitter.cell->splitDirection() == ViewportLayoutCell::Horizontal) ? tr("Delete viewport on left") : tr("Delete viewport above"));
    contextMenu.addAction(deleteCell1Action);
    connect(deleteCell1Action, &QAction::triggered, this, [&]() {
        _mainWindow.performTransaction(tr("Delete viewport(s)"), [&]() {
            splitter.cell->removeChild(splitter.childCellIndex);
        });
    });

    // Action that removes the child cell on the other side.
    QAction* deleteCell2Action = new QAction(&contextMenu);
    if(splitter.cell->children()[splitter.childCellIndex+1] && !splitter.cell->children()[splitter.childCellIndex+1]->viewport())
        deleteCell2Action->setText((splitter.cell->splitDirection() == ViewportLayoutCell::Horizontal) ? tr("Delete viewports on right") : tr("Delete viewports below"));
    else
        deleteCell2Action->setText((splitter.cell->splitDirection() == ViewportLayoutCell::Horizontal) ? tr("Delete viewport on right") : tr("Delete viewport below"));
    contextMenu.addAction(deleteCell2Action);
    connect(deleteCell2Action, &QAction::triggered, this, [&]() {
        _mainWindow.performTransaction(tr("Delete viewport(s)"), [&]() {
            splitter.cell->removeChild(splitter.childCellIndex + 1);
            _viewportConfig->layoutRootCell()->pruneViewportLayoutTree();
        });
    });

    // Show menu.
    contextMenu.exec(mapToGlobal(mousePos));
}

/******************************************************************************
* Handles keyboard input for the viewport windows.
******************************************************************************/
bool ViewportsPanel::onKeyShortcut(QKeyEvent* event)
{
    // Suppress viewport navigation shortcuts when a list/table widget has the focus.
    QWidget* focusWidget = _mainWindow.focusWidget();
    if(qobject_cast<QAbstractItemView*>(focusWidget))
        return false;

    // Get the viewport window the input pertains to.
    ViewportWindow* vpwin = viewportWindow(_viewportConfig ? _viewportConfig->activeViewport() : nullptr);
    if(!vpwin)
        return false;

    qreal delta = 1.0;
    if(event->key() == Qt::Key_Left) {
        _mainWindow.performTransaction(tr("Move camera"), [&] {
            if(!(event->modifiers() & Qt::ShiftModifier))
                _mainWindow.viewportInputManager()->orbitMode()->discreteStep(vpwin, QPointF(-delta, 0));
            else
                _mainWindow.viewportInputManager()->panMode()->discreteStep(vpwin, QPointF(-delta, 0));
        });
        return true;
    }
    else if(event->key() == Qt::Key_Right) {
        _mainWindow.performTransaction(tr("Move camera"), [&] {
            if(!(event->modifiers() & Qt::ShiftModifier))
                _mainWindow.viewportInputManager()->orbitMode()->discreteStep(vpwin, QPointF(delta, 0));
            else
                _mainWindow.viewportInputManager()->panMode()->discreteStep(vpwin, QPointF(delta, 0));
        });
        return true;
    }
    else if(event->key() == Qt::Key_Up) {
        _mainWindow.performTransaction(tr("Move camera"), [&] {
            if(!(event->modifiers() & Qt::ShiftModifier))
                _mainWindow.viewportInputManager()->orbitMode()->discreteStep(vpwin, QPointF(0, -delta));
            else
                _mainWindow.viewportInputManager()->panMode()->discreteStep(vpwin, QPointF(0, -delta));
        });
        return true;
    }
    else if(event->key() == Qt::Key_Down) {
        _mainWindow.performTransaction(tr("Move camera"), [&] {
            if(!(event->modifiers() & Qt::ShiftModifier))
                _mainWindow.viewportInputManager()->orbitMode()->discreteStep(vpwin, QPointF(0, delta));
            else
                _mainWindow.viewportInputManager()->panMode()->discreteStep(vpwin, QPointF(0, delta));
        });
        return true;
    }
    else if(event->matches(QKeySequence::ZoomIn)) {
        _mainWindow.viewportInputManager()->zoomMode()->zoom(vpwin->viewport(), 50, _mainWindow);
        return true;
    }
    else if(event->matches(QKeySequence::ZoomOut)) {
        _mainWindow.viewportInputManager()->zoomMode()->zoom(vpwin->viewport(), -50, _mainWindow);
        return true;
    }
    return false;
}

}   // End of namespace
