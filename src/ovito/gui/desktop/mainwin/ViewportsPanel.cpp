////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/gui/base/viewport/BaseViewportWindow.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/gui/base/viewport/ViewportInputManager.h>
#include <ovito/core/viewport/ViewportSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include "ViewportsPanel.h"

namespace Ovito {

/******************************************************************************
* The constructor of the viewports panel class.
******************************************************************************/
ViewportsPanel::ViewportsPanel(MainWindow* mainWindow) : _mainWindow(mainWindow)
{
	// Activate the new viewport layout as soon as a new state file is loaded.
	connect(&mainWindow->datasetContainer(), &DataSetContainer::viewportConfigReplaced, this, &ViewportsPanel::onViewportConfigurationReplaced);
	connect(&mainWindow->datasetContainer(), &DataSetContainer::animationSettingsReplaced, this, &ViewportsPanel::onAnimationSettingsReplaced);

	// Track viewport input mode changes.
	connect(mainWindow->viewportInputManager(), &ViewportInputManager::inputModeChanged, this, &ViewportsPanel::onInputModeChanged);

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
* Factory method which creates a new viewport window widget. Depending on the 
* user's settings this can be either a OpenGL or a Vulkan window.
******************************************************************************/
BaseViewportWindow* ViewportsPanel::createViewportWindow(Viewport* vp, MainWindow* mainWindow, QWidget* parent)
{
	// Select the viewport window implementation to use.
	QSettings settings;
	const QMetaObject* viewportImplementation = nullptr;
	for(const QMetaObject* metaType : ViewportWindowInterface::registry()) {
		if(qstrcmp(metaType->className(), "Ovito::OpenGLViewportWindow") == 0) {
			viewportImplementation = metaType;
		}
		else if(qstrcmp(metaType->className(), "Ovito::VulkanViewportWindow") == 0 && settings.value("rendering/selected_graphics_api").toString() == "Vulkan") {
			viewportImplementation = metaType;
			break;
		}
	}

	qRegisterMetaType<UserInterface*>("UserInterfacePtr");

	if(viewportImplementation)
		return dynamic_cast<BaseViewportWindow*>(viewportImplementation->newInstance(Q_ARG(Viewport*, vp), Q_ARG(UserInterface*, mainWindow), Q_ARG(QWidget*, parent)));

	return nullptr;
}

/******************************************************************************
* Returns the widget that is associated with the given viewport.
******************************************************************************/
QWidget* ViewportsPanel::viewportWidget(Viewport* vp)
{
	OVITO_ASSERT(_viewportConfig != nullptr);

	// Create the viewport window if it hasn't been created for this viewport yet.
	if(!vp->window() && !_graphicsInitializationErrorOccurred) {
		try {
			BaseViewportWindow* viewportWindow = createViewportWindow(vp, _mainWindow, this);
			if(!viewportWindow || !viewportWindow->widget())
				vp->throwException(tr("Failed to create viewport window or there is no realtime graphics implementation available. Please check your OVITO installation and the graphics capabilities of your system."));
			if(_viewportConfig->activeViewport() == vp)
				viewportWindow->widget()->setFocus();
			// Show a context menu when the user clicks the viewport caption.
			connect(vp, &Viewport::contextMenuRequested, this, &ViewportsPanel::onViewportMenuRequested);
		}
		catch(const Exception& ex) {
			_graphicsInitializationErrorOccurred = true;
			ex.reportError(true);
			return nullptr;
		}
	}

	if(BaseViewportWindow* window = dynamic_cast<BaseViewportWindow*>(vp->window()))
		return window->widget();

	return nullptr;
}

/******************************************************************************
* Displays the context menu for a viewport window.
******************************************************************************/
void ViewportsPanel::onViewportMenuRequested(const QPoint& pos)
{
	// Get the viewport that emitted the signal.
	Viewport* viewport = qobject_cast<Viewport*>(sender());
	OVITO_ASSERT(viewport);

	// Get the viewport's window.
	BaseViewportWindow* vpwin = dynamic_cast<BaseViewportWindow*>(viewport->window());
	OVITO_ASSERT(vpwin && vpwin->widget() && vpwin->widget()->parentWidget() == this);

	// Create the context menu for the viewport.
	ViewportMenu contextMenu(viewport, vpwin->widget());

	// Show menu.
	contextMenu.show(pos);
}

/******************************************************************************
* This is called when a new viewport configuration has been loaded.
******************************************************************************/
void ViewportsPanel::onViewportConfigurationReplaced(ViewportConfiguration* newViewportConfiguration)
{
	disconnect(_activeViewportChangedConnection);
	disconnect(_maximizedViewportChangedConnection);
	disconnect(_viewportLayoutChangedConnection);
	_viewportConfig = newViewportConfiguration;
	
	// Create the interactive viewport windows.
	recreateViewportWindows();

	if(_viewportConfig) {
		// Repaint the viewport borders when another viewport has been activated.
		_activeViewportChangedConnection = connect(_viewportConfig, &ViewportConfiguration::activeViewportChanged, this, qOverload<>(&ViewportsPanel::update));
		// Update layout when a viewport has been maximized.
		_maximizedViewportChangedConnection = connect(_viewportConfig, &ViewportConfiguration::maximizedViewportChanged, this, &ViewportsPanel::invalidateWindowLayout);
		// Update the viewport window positions when the viewport layout is modified.
		_viewportLayoutChangedConnection = connect(_viewportConfig, &ViewportConfiguration::viewportLayoutChanged, this, &ViewportsPanel::invalidateWindowLayout);
	}
}

/******************************************************************************
* Destroys all viewport windows in the panel and recreates them.
******************************************************************************/
void ViewportsPanel::recreateViewportWindows()
{
	// Delete all existing viewport widgets first.
	for(QWidget* widget : findChildren<QWidget*>())
		delete widget;

	if(_viewportConfig) {
		// Layout viewport widgets.
		// This function implicitly creates the Qt widgets for all viewports.
		layoutViewports();
	}
}

/******************************************************************************
* This is called when new animation settings have been loaded.
******************************************************************************/
void ViewportsPanel::onAnimationSettingsReplaced(AnimationSettings* newAnimationSettings)
{
	disconnect(_autoKeyModeChangedConnection);
	_animSettings = newAnimationSettings;

	if(newAnimationSettings) {
		_autoKeyModeChangedConnection = connect(newAnimationSettings, &AnimationSettings::autoKeyModeChanged, this, (void (ViewportsPanel::*)())&ViewportsPanel::update);
	}
	update();
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

	for(Viewport* vp : _viewportConfig->viewports()) {
		if(ViewportWindowInterface* window = vp->window()) {
			window->setCursor(cursor);
		}
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
	if(!vp) return;
	QWidget* vpWidget = viewportWidget(vp);
	if(!vpWidget || vpWidget->isHidden()) return;

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
		if(_animSettings && _animSettings->autoKeyMode())
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

	// Delete stale viewport widgets belonging to removed viewports.
	for(QObject* childWidget : children()) {
		OVITO_ASSERT(childWidget->isWidgetType());
		if(boost::algorithm::none_of(viewports, [&](Viewport* vp) { return viewportWidget(vp) == childWidget; })) {
			delete childWidget;
		}
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

	if(_viewportConfig->maximizedViewport() && !_viewportConfig->maximizedViewport()->window()) {
		_viewportConfig->setMaximizedViewport(viewports.empty() ? nullptr : viewports.front());
		_viewportConfig->setActiveViewport(_viewportConfig->maximizedViewport());
	}
	if(_viewportConfig->activeViewport() && !_viewportConfig->activeViewport()->window())
		_viewportConfig->setActiveViewport(viewports.empty() ? nullptr : viewports.front());
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
				_viewportConfig->dataset()->undoStack().beginCompoundOperation(tr("Resize viewports"));
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
		// Temporarily block the viewportLayoutChanged() signal from the ViewportConfiguration to avoid
		// an unnecessary relayout of the viewport windows while resetting the undo operation.
		QSignalBlocker signalBlocker(_viewportConfig);
		_viewportConfig->dataset()->undoStack().resetCurrentCompoundOperation();
		signalBlocker.unblock();

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
			_viewportConfig->dataset()->undoStack().endCompoundOperation();
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
				return region.area.contains(static_cast<QHoverEvent*>(event)->position().toPoint());
#else
				return region.area.contains(static_cast<QHoverEvent*>(event)->pos());
#endif
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
		UndoableTransaction::handleExceptions(_viewportConfig->dataset()->undoStack(), tr("Resize viewports"), [&]() {
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
		OORef<ViewportLayoutCell> newCell = OORef<ViewportLayoutCell>::create(splitter.cell->dataset());
		newCell->setViewport(CloneHelper().cloneObject(adjacentViewport, true));
		UndoableTransaction::handleExceptions(_viewportConfig->dataset()->undoStack(), tr("Insert viewport"), [&]() {
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
		UndoableTransaction::handleExceptions(_viewportConfig->dataset()->undoStack(), tr("Delete viewport(s)"), [&]() {
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
		UndoableTransaction::handleExceptions(_viewportConfig->dataset()->undoStack(), tr("Delete viewport(s)"), [&]() {
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
	QWidget* focusWidget = _mainWindow->focusWidget();
	if(qobject_cast<QAbstractItemView*>(focusWidget))
		return false;

	// Get the viewport the input pertains to.
	Viewport* vp = _viewportConfig ? _viewportConfig->activeViewport() : nullptr;
	if(!vp) return false;

	qreal delta = 1.0;
	if(event->key() == Qt::Key_Left) {
		if(!(event->modifiers() & Qt::ShiftModifier))
			_mainWindow->viewportInputManager()->orbitMode()->discreteStep(vp->window(), QPointF(-delta, 0));
		else
			_mainWindow->viewportInputManager()->panMode()->discreteStep(vp->window(), QPointF(-delta, 0));
		return true;
	}
	else if(event->key() == Qt::Key_Right) {
		if(!(event->modifiers() & Qt::ShiftModifier))
			_mainWindow->viewportInputManager()->orbitMode()->discreteStep(vp->window(), QPointF(delta, 0));
		else
			_mainWindow->viewportInputManager()->panMode()->discreteStep(vp->window(), QPointF(delta, 0));
		return true;
	}
	else if(event->key() == Qt::Key_Up) {
		if(!(event->modifiers() & Qt::ShiftModifier))
			_mainWindow->viewportInputManager()->orbitMode()->discreteStep(vp->window(), QPointF(0, -delta));
		else
			_mainWindow->viewportInputManager()->panMode()->discreteStep(vp->window(), QPointF(0, -delta));
		return true;
	}
	else if(event->key() == Qt::Key_Down) {
		if(!(event->modifiers() & Qt::ShiftModifier))
			_mainWindow->viewportInputManager()->orbitMode()->discreteStep(vp->window(), QPointF(0, delta));
		else
			_mainWindow->viewportInputManager()->panMode()->discreteStep(vp->window(), QPointF(0, delta));
		return true;
	}
	else if(event->matches(QKeySequence::ZoomIn)) {
		_mainWindow->viewportInputManager()->zoomMode()->zoom(vp, 50);
		return true;
	}
	else if(event->matches(QKeySequence::ZoomOut)) {
		_mainWindow->viewportInputManager()->zoomMode()->zoom(vp, -50);
		return true;
	}
	return false;
}

}	// End of namespace
