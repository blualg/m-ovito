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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

/**
 * The container widget for the viewports in OVITO's main window.
 */
class ViewportsPanel : public QWidget
{
	Q_OBJECT

public:

	/// Constructs the viewport panel.
	ViewportsPanel(MainWindow* parent);

	/// Factory method which creates a new viewport window widget. Depending on the 
	/// user's settings this can be either a OpenGL or a Vulkan window.
	static BaseViewportWindow* createViewportWindow(Viewport* vp, MainWindow* mainWindow, QWidget* parent);

	/// Returns the widget that is associated with the given viewport.
	QWidget* viewportWidget(Viewport* vp);

	/// Returns the current viewport configuration object.
	ViewportConfiguration* viewportConfiguration() const { return _viewportConfig; }
	
	/// Handles keyboard input for the viewport windows.
	bool onKeyShortcut(QKeyEvent* event);

public Q_SLOTS:

	/// Requests a relayout of the viewport windows.
	void invalidateWindowLayout();

	/// Performs the layout of the viewports in the panel.
	void layoutViewports();

	/// Destroys all viewport windows in the panel and recreates them.
	void recreateViewportWindows();

protected:

	/// Renders the borders around the viewports.
	virtual void paintEvent(QPaintEvent* event) override;

	/// Handles size event for the window.
	virtual void resizeEvent(QResizeEvent* event) override;

	/// Handles mouse input events.
	virtual void mousePressEvent(QMouseEvent* event) override;

	/// Handles mouse input events.
	virtual void mouseMoveEvent(QMouseEvent* event) override;

	/// Handles mouse input events.
	virtual void mouseReleaseEvent(QMouseEvent* event) override;

	/// Handles general events of the widget.
	virtual bool event(QEvent* event) override;

private Q_SLOTS:

	/// Displays the context menu for a viewport window.
	void onViewportMenuRequested(const QPoint& pos);

	/// This is called when a new viewport configuration has been loaded.
	void onViewportConfigurationReplaced(ViewportConfiguration* newViewportConfiguration);

	/// This is called when new animation settings have been loaded.
	void onAnimationSettingsReplaced(AnimationSettings* newAnimationSettings);

	/// This is called when the current viewport input mode has changed.
	void onInputModeChanged(ViewportInputMode* oldMode, ViewportInputMode* newMode);

	/// This is called when the mouse cursor of the active input mode has changed.
	void onViewportModeCursorChanged(const QCursor& cursor);

private:
	
	struct SplitterRectangle 
	{
		QRect area;
		ViewportLayoutCell* cell;
		size_t childCellIndex;
		FloatType dragFactor;
	};

	/// Recursive helper function for laying out the viewport windows.
	void layoutViewportsRecursive(ViewportLayoutCell* layoutCell, const QRect& rect);

	/// Displays the context menu associated with a splitter handle.
	void showSplitterContextMenu(const SplitterRectangle& splitter, const QPoint& mousePos);

	QMetaObject::Connection _activeViewportChangedConnection;
	QMetaObject::Connection _maximizedViewportChangedConnection;
	QMetaObject::Connection _viewportLayoutChangedConnection;
	QMetaObject::Connection _autoKeyModeChangedConnection;
	QMetaObject::Connection _activeModeCursorChangedConnection;

	OORef<ViewportConfiguration> _viewportConfig;
	OORef<AnimationSettings> _animSettings;
	MainWindow* _mainWindow;
	bool _graphicsInitializationErrorOccurred = false; 

	static constexpr int _splitterSize = 2;
	static constexpr int _windowInset = 2;

	bool _relayoutRequested = false;
	std::vector<SplitterRectangle> _splitterRegions;
	int _hoveredSplitter = -1;
	bool _highlightSplitter = false;
	int _draggedSplitter = -1;
	QPoint _dragStartPos;
	QBasicTimer _highlightSplitterTimer;
};

}	// End of namespace
