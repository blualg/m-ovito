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

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefMaker.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/TextPrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/rendering/RenderingJob.h>
#include <ovito/core/dataset/scene/ScenePreparation.h>

namespace Ovito {

/**
 * \brief A viewport window provides the connection between the non-visual Viewport class and the GUI layer.
 */
class OVITO_CORE_EXPORT ViewportWindow : public QObject, public RefMaker
{
    Q_OBJECT
	OVITO_CLASS(ViewportWindow)

public:

    /// Data structure returned by the ViewportWindow::pick() method,
    /// holding information on the object that has been picked in a viewport window.
    class OVITO_CORE_EXPORT PickResult
    {
    public:

        /// Constructor.
        PickResult(OORef<Pipeline> pipeline, OORef<ObjectPickInfo> pickInfo, const Point3& hitLocation, quint32 subobjectId)
            : _pipeline(std::move(pipeline)), _pickInfo(std::move(pickInfo)), _hitLocation(hitLocation), _subobjectId(subobjectId) {}

        /// Returns the pipeline that has been picked.
        const OORef<Pipeline>& pipeline() const { return _pipeline; }

        /// Returns the object-specific data at the pick location.
        const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

        /// Returns the coordinates of the hit point in world space.
        const Point3& hitLocation() const { return _hitLocation; }

        /// Returns the sub-object that was picked.
        quint32 subobjectId() const { return _subobjectId; }

    private:

        /// The pipeline that was picked.
        OORef<Pipeline> _pipeline;

        /// The object-specific data at the pick location.
        OORef<ObjectPickInfo> _pickInfo;

        /// The coordinates of the hit point in world space.
        Point3 _hitLocation;

        /// The subobject that was picked.
        quint32 _subobjectId = 0;
    };

public:

    /// Associates this window with a viewport.
    void setViewport(Viewport* vp, UserInterface& userInterface);

    /// This method is called after the reference counter of this object has reached zero
    /// and before the object is being finally deleted.
    virtual void aboutToBeDeleted() override;

    /// Returns the abstract user interface hosting this viewport window.
    UserInterface& userInterface() const { OVITO_ASSERT(_userInterface); return *_userInterface; }

    /// Returns the object responsible for evaluating all pipelines in the scene to prepare interactive rendering.
    ScenePreparation& scenePreparation() { OVITO_ASSERT(_scenePreparation); return *_scenePreparation; }

    /// Creates and returns the rendering job that renders the contents of the viewport window.
    const OORef<RenderingJob>& renderingJob() {
        if(!_renderingJob)
            setRenderingJob(createRenderingJob());
        return _renderingJob;
    }

    /// Asks the window to handle any pending update request now after viewport updates were temporarily suspended.
    void resumeViewportUpdates();

    /// Returns the future representing the asynchronous operation of updating the viewport contents (only if an update is in progress).
    Future<void>& frameFuture() { return _frameFuture; }

    /// Return the current 3D projection used to render the contents of the viewport window.
    const ViewProjectionParameters& projectionParams() const { return _projParams; }

    /// Returns whether the viewport window is using a perspective projection.
    bool isPerspectiveProjection() const { return projectionParams().isPerspective; }

    /// Indicates whether the window is currently shown or not.
    virtual bool isVisible() const = 0;

    /// Returns the current size of the viewport window (in device pixels).
    virtual QSize viewportWindowDeviceSize() const = 0;

    /// Returns the current size of the viewport window (in device-independent pixels).
    virtual QSize viewportWindowDeviceIndependentSize() const = 0;

    /// Returns the device pixel ratio of the viewport window's canvas.
    virtual qreal devicePixelRatio() const = 0;

    /// Returns the visibility of the orientation indicator.
    bool isOrientationIndicatorVisible() const { return _showOrientationIndicator; }

    /// Controls the visibility of the orientation indicator.
    void setOrientationIndicatorVisible(bool visible) { _showOrientationIndicator = visible; }

    /// Returns whether the viewport caption is displayed.
    bool isViewportTitleVisible() const { return _showViewportTitle; }

    /// Sets whether the viewport caption is shown.
    void setViewportTitleVisible(bool visible) { _showViewportTitle = visible; }

    /// Indicates whether the mouse cursor is currently positioned inside the
    /// viewport window area that activates the context menu.
    bool cursorInContextMenuArea() const { return _cursorInContextMenuArea; }

    /// Sets a flag indicating whether the mouse cursor is currently located in the
    /// viewport window area that activates the context menu.
    void setCursorInContextMenuArea(bool flag);

    /// Returns the zone in the upper left corner of the viewport where the context menu can be activated by the user.
    const QRectF& contextMenuArea() const { return _contextMenuArea; }

    /// Determines the object located under the given mouse cursor position.
    virtual std::optional<PickResult> pick(const QPointF& pos) = 0;

    /// Returns the list of gizmos to render in the viewport.
    virtual std::vector<ViewportGizmo*> viewportGizmos() { return {}; }

    /// Sets the mouse cursor shape for the window.
    virtual void setCursor(const QCursor& cursor) {}

    /// Returns the current position of the mouse cursor relative to the viewport window.
    virtual QPoint getCurrentMousePos() const { return QPoint(); }

    /// \brief Computes a point in the given coordinate system based on the given screen position and the current snapping settings.
    /// \param[in] screenPoint A point relative to the upper left corner of the viewport window.
    /// \param[out] snapPoint The resulting point in the coordinate system specified by \a snapSystem. If the method returned
    ///                       \c false then the value of this output variable is undefined.
    /// \param[in] snapSystem Specifies the coordinate system in which the snapping point should be determined.
    /// \return \c true if a snapping point has been found; \c false if no snapping point was found for the given screen position.
    bool snapPoint(const QPointF& screenPoint, Point3& snapPoint, const AffineTransformation& snapSystem) const;

    /// \brief Computes a point in the grid coordinate system based on a screen position and the current snap settings.
    /// \param[in] screenPoint A point relative to the upper left corner of the viewport window.
    /// \param[out] snapPoint The resulting snap point in the viewport's grid coordinate system. If the method returned
    ///                       \c false then the value of this output variable is undefined.
    /// \return \c true if a snapping point has been found; \c false if no snapping point was found for the given screen position.
    bool snapPoint(const QPointF& screenPoint, Point3& snapPoint) const;

    /// \brief Computes a ray in world space going through a pixel of the viewport window.
    /// \param screenPoint A screen point relative to the upper left corner of the viewport window.
    /// \return The ray that goes from the camera point through the specified pixel of the viewport window.
    Ray3 screenRay(const QPointF& screenPoint) const;

    /// Computes the geometry of the render preview frame, i.e., the cutout region of the interactive viewport window that
    /// will be visible in a rendered image. The returned rectangle is given in window coordinates.
    QRect previewFrameGeometry(DataSet* dataset, const QSize& windowSize) const;

    /// \brief Computes the intersection point of a ray going through a point in the
    ///        viewport plane with the construction grid plane.
    /// \param[in] viewportPosition A 2d point in viewport coordinates (in the range [-1,+1]).
    /// \param[out] intersectionPoint The coordinates of the intersection point in grid plane coordinates.
    ///                               The point can be transformed to world coordinates using the gridMatrix() transform.
    /// \param[in] epsilon This threshold value is used to test whether the ray is parallel to the grid plane.
    /// \return \c true if an intersection has been found; \c false if not.
    bool computeConstructionPlaneIntersection(const Point2& viewportPosition, Point3& intersectionPoint, FloatType epsilon = FLOATTYPE_EPSILON);

    /// \brief Zooms to the extents of the given bounding box.
    void zoomToBox(const Box3& box);

public:

    /// Returns the list of available interactive viewport window implementations.
    static std::vector<std::tuple<QString, QString, OvitoClassPtr>> listInteractiveWindowImplementations();

    /// Returns a string identifying the interactive viewport window implementation currently selected by the user.
    static QString getInteractiveWindowImplementationName();

    /// Sets the interactive viewport window implementation currently selected by the user.
    static bool setInteractiveWindowImplementationName(const QString& name);

    /// Returns a pointer to the runtime class to be used for creating interactive viewport windows.
    static OvitoClassPtr getInteractiveWindowImplementationClass();

    /// Switches back to the default renderer for the interactive viewport windows.
    static bool revertToDefaultInteractiveWindowImplementation();

public Q_SLOTS:

    /// Releases the renderer resources held by the viewport window and the renderer.
    virtual void releaseResources();

    /// Schedules a refresh for this window.
    virtual void requestUpdate(bool isPreliminaryUpdate);

    /// Zooms to the extents of the scene.
    void zoomToSceneExtents();

    /// Zooms to the extents of the currently selected scene nodes.
    void zoomToSelectionExtents();

    /// Zooms to the extents of the scene once all scene pipelines have been computed.
    void zoomToSceneExtentsWhenReady();

Q_SIGNALS:

    /// Is emitted by the window when it gets hidden (e.g. minimized) or completely closed.
    void viewportWindowHidden();

    /// Is emitted when a rendering of the complete scene (all fully evaluated pipelines)
    /// has been finished and was displayed to the user.
    void frameRenderComplete();

    /// Is emitted by the window when a fatal error has occurred, which prevents the window
    /// from displaying any further content.
    void fatalError(Exception& ex);

protected:

    /// Tells the window implementation to present a rendered frame on screen.
    virtual void presentFrame() = 0;

    /// Handles timer events for this object.
    virtual void timerEvent(QTimerEvent* event) override;

    /// Creates the rendering job that renders the contents of the viewport window.
    virtual OORef<RenderingJob> createRenderingJob() = 0;

    /// Newly renders the window contents after the frame graph has been regenerated.
    virtual Future<void> renderFrameGraph(OORef<FrameGraph> frameGraph) = 0;

    /// Is called when a RefTarget referenced by this object generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Replaces the rendering job used by this window to render its contents.
    void setRenderingJob(OORef<RenderingJob> job) { _renderingJob = std::move(job); }

    /// Modifies the projection such that the render preview frame painted over the 3d scene exactly matches the true visible area.
    void adjustProjectionForRenderPreviewFrame(DataSet* dataset, ViewProjectionParameters& params, const QSize& windowSize);

    /// Render the axis tripod symbol in the corner of the viewport that indicates
    /// the coordinate system orientation.
    void renderOrientationIndicator(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, const QSize& windowSize);

    /// Paints the rectangular frame on top of the scene to indicate the visible image area.
    void renderPreviewFrame(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup, DataSet* dataset, const QSize& windowSize);

    /// Renders the viewport caption text.
    QRectF renderViewportTitle(FrameGraph& frameGraph, FrameGraph::RenderingCommandGroup& commandGroup);

	/// Renders the visual representation of the modifiers in a pipeline.
	void renderPipelineModifiers(Pipeline* pipeline, FrameGraph& frameGraph);

	/// Determines the range of the construction grid to display.
	std::tuple<FloatType, Box2I> determineConstructionGridRange();

	/// Renders the construction grid in a viewport.
	void renderConstructionGrid(FrameGraph& frameGraph);

private:

    /// Generates the frame graph for this viewport window and calls the attached RenderingJob to render the frame.
    Future<void> buildAndRenderFrameGraph();

    /// Is called once the frame has been rendered by the window's RenderingJob.
    void frameGraphRenderingFinished(Task& task) noexcept;

    /// Is called when a rendered frame needs to be presented on screen.
    void becameReadyForPresentation();

private:

    /// The viewport associated with this window.
    DECLARE_REFERENCE_FIELD_FLAGS(Viewport*, viewport, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM | PROPERTY_FIELD_WEAK_REF | PROPERTY_FIELD_NO_UNDO);

    /// The abstract user interface hosting this viewport window.
    UserInterface* _userInterface = nullptr;

#ifdef OVITO_DEBUG
    /// Counts how often this viewport has been rendered during the current program session.
    int _renderDebugCounter = 0;
#endif

    /// The rendering job that renders the contents of the viewport window.
    OORef<RenderingJob> _renderingJob;

    /// Object responsible for evaluating all pipelines in the scene to prepare interactive rendering.
    OORef<ScenePreparation> _scenePreparation;

    /// Indicates that the program has requested an update of this viewport window.
    bool _updateNeeded = false;

    /// The future object representing the asynchronous operation of updating the viewport contents.
    Future<void> _frameFuture;

    /// Indicates that this viewport window can now present a new image on screen.
    bool _readyForPresentation = false;

    /// Used to refresh the viewport after a short waiting period.
    QBasicTimer _preliminaryUpdateTimer;

    /// Used to present the viewport contents after a short waiting period.
    QBasicTimer _presentTimer;

    /// Controls the visibility of the orientation indicator.
    bool _showOrientationIndicator = true;

    /// Controls the visibility of the viewport caption.
    bool _showViewportTitle = true;

    /// The zone in the upper left corner of the viewport window where
    /// the context menu can be activated by the user.
    QRectF _contextMenuArea;

    /// Indicates that the mouse cursor is currently positioned inside the
    /// viewport window area that activates the context menu.
    bool _cursorInContextMenuArea = false;

    /// The current 3D projection for rendering the contents of the viewport window.
    ViewProjectionParameters _projParams;
};

}   // End of namespace

#include <ovito/core/viewport/Viewport.h>
