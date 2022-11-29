////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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


#include <ovito/core/Core.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportLayout.h>

namespace Ovito {

/**
 * \brief This class holds a collection of Viewport objects.
 *
 * It also keeps track of the current viewport and the maximized viewport.
 */
class OVITO_CORE_EXPORT ViewportConfiguration : public RefTarget
{
	OVITO_CLASS(ViewportConfiguration)

#ifdef OVITO_QML_GUI
	Q_PROPERTY(Ovito::Viewport* activeViewport READ activeViewport WRITE setActiveViewport NOTIFY activeViewportChanged)
	Q_PROPERTY(Ovito::Viewport* maximizedViewport READ maximizedViewport WRITE setMaximizedViewport NOTIFY maximizedViewportChanged)
#endif

public:

	/// Constructor.
	Q_INVOKABLE ViewportConfiguration(ObjectCreationParams params);

	/// \brief Immediately repaints all viewports that have been scheduled for an update using updateViewports().
	/// \sa updateViewports()
	void processViewportUpdates();

	/// \brief A call to this method suspends redrawing of the viewports.
	///
	/// To resume redrawing of viewports call resumeViewportUpdates().
	///
	/// Calling updateViewports() while redrawing is suspended will update the
	/// viewports as soon as redrawing is resumed.
	///
	/// Normally you should use the ViewportSuspender helper class to suspend viewport update.
	/// It has the advantage of being exception-safe.
	///
	/// \sa resumeViewportUpdates(), isSuspended(), ViewportSuspender
	void suspendViewportUpdates() { _viewportSuspendCount++; }

	/// \brief This will resume redrawing of the viewports after a call to suspendViewportUpdates().
	/// \sa suspendViewportUpdates(), isSuspended()
	void resumeViewportUpdates();

	/// \brief Returns whether viewport updates are suspended.
	/// \return \c true if suspendViewportUpdates() has been called to suspend any viewport updates.
	/// \sa suspendViewportUpdates(), resumeViewportUpdates()
	bool isSuspended() const { return _viewportSuspendCount > 0; }

	/// \brief Returns whether any of the viewports
	///        is currently being updated.
	/// \return \c true if there is currently a rendering operation going on.
	bool isRendering() const;

	/// Returns the current location around which the viewport camera orbits.
	Point3 orbitCenter(Viewport* vp);

	/// Registers a viewport with the configuration object so that it takes part in the automatic viewport refresh mechanism.
	/// This method is currently used in the implementation of the Viewport.create_qt_widget() Python method.
	void registerViewport(Viewport* vp) { _viewports.push_back(this, PROPERTY_FIELD(viewports), vp); }

	/// Determines the effective rectangles for all the viewports in the layout hierarchy.
	std::vector<std::pair<Viewport*, QRectF>> getViewportRectangles(const QRectF& rect = QRectF(0,0,1,1), const QSizeF& borderSize = QSizeF(0,0)) const {
		std::vector<std::pair<Viewport*, QRectF>> viewportRects;
		if(layoutRootCell())
			layoutRootCell()->getViewportRectangles(rect, viewportRects, borderSize);
		return viewportRects;
	}

public Q_SLOTS:

	/// \brief Zooms all viewports to the extents of the currently selected nodes.
	void zoomToSelectionExtents();

	/// \brief Zooms to the extents of the scene.
	void zoomToSceneExtents();

	/// \brief Zooms all viewports to the extents of the currently selected nodes 
	///        when the pipeline has been fully evaluated and the extents are known.
	void zoomToSelectionExtentsWhenReady();

	/// \brief This will flag all viewports for redrawing.
	///
	/// This function does not cause an immediate repaint of the viewports; instead it schedules a
	/// paint event for processing when Qt returns to the main event loop. You can call this method as often
	/// as you want; it will return immediately and will cause only one viewport repaint when Qt returns to the
	/// main event loop.
	///
	/// To update only a single viewport, Viewport::updateViewport() should be used.
	///
	/// To redraw all viewports immediately without waiting for the paint event to be processed,
	/// call processViewportUpdates() subsequently.
	void updateViewports();

Q_SIGNALS:

	/// This signal is emitted when another viewport became active.
	void activeViewportChanged(Viewport* activeViewport);

	/// This signal is emitted when one of the viewports has been maximized.
	void maximizedViewportChanged(Viewport* maximizedViewport);

	/// This signal is emitted whenever the updating of viewports is resumed.
	void viewportUpdateResumed();

	/// This signal is sent whenver the layout of the viewports changes.
	void viewportLayoutChanged();

protected:

	/// Is called when the value of a reference field of this RefMaker changes.
	virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

	/// Is called when a RefTarget referenced by this object has generated an event.
	virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

	/// This method is called once for this object after it has been completely loaded from a stream.
	virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:

	/// Rebuilds the linear list of all viewports that are part of the current viewport layout tree. 
	void updateListOfViewports();

	/// List of all viewports which get automatically refreshed whenever their scene changes.
	DECLARE_VECTOR_REFERENCE_FIELD_FLAGS(Viewport*, viewports, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_WEAK_REF);

	/// The active viewport. May be null.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(Viewport*, activeViewport, setActiveViewport, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_WEAK_REF);

	/// The maximized viewport if any.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(Viewport*, maximizedViewport, setMaximizedViewport, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_WEAK_REF);

	/// The viewport layout tree's root node.
	DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<ViewportLayoutCell>, layoutRootCell, setLayoutRootCell);

	/// This counter is for suspending the viewport updates.
	int _viewportSuspendCount = 0;

	/// Indicates that the viewports have been invalidated while updates were suspended.
	bool _viewportsNeedUpdate = false;
};

/**
 * \brief RAII helper class that suspends viewport redrawing while it exists.
 *
 * Use this to make your code exception-safe.
 * Just create an instance of this class on the stack to suspend viewport updates
 * during the lifetime of the class instance.
 */
class OVITO_CORE_EXPORT ViewportSuspender 
{
public:
	ViewportSuspender(UserInterface& userInterface) noexcept;
};

}	// End of namespace
