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

/**
 * \file SceneRenderer.h
 * \brief Contains the definition of the Ovito::SceneRenderer class.
 */

#pragma once


#include <ovito/core/Core.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/scene/Pipeline.h>
#include <ovito/core/dataset/scene/Scene.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/utilities/MixedKeyCache.h>
#include "LinePrimitive.h"
#include "ParticlePrimitive.h"
#include "TextPrimitive.h"
#include "ImagePrimitive.h"
#include "CylinderPrimitive.h"
#include "MeshPrimitive.h"
#include "MarkerPrimitive.h"
#include "RendererResourceCache.h"

namespace Ovito {

/**
 * Abstract base class for object-specific information used in the object picking system.
 */
class OVITO_CORE_EXPORT ObjectPickInfo : public OvitoObject
{
	OVITO_CLASS(ObjectPickInfo)

protected:

	/// Constructor of abstract class.
	ObjectPickInfo() = default;

public:

	/// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
	virtual QString infoString(Pipeline* pipeline, quint32 subobjectId) { return {}; }
};

/**
 * Abstract base class for scene renderers, which produce a picture of the three-dimensional scene.
 */
class OVITO_CORE_EXPORT SceneRenderer : public RefTarget
{
	OVITO_CLASS(SceneRenderer)

public:

	struct ObjectPickingRecord {
		quint32 baseObjectID;
		OORef<Pipeline> pipeline;
		OORef<ObjectPickInfo> pickInfo;
		std::vector<std::pair<ConstDataBufferPtr, quint32>> indexedRanges;
	};

	/// A special exception type thrown by a scene renderer from one of its renderXXX() methods
	/// to indicate that something went wrong. The error will interrupt the rendering process and
	/// will be shown to the user.
	class OVITO_CORE_EXPORT RendererException : public Exception {
	public:
		using Exception::Exception;
	};

	/// This may be called on a renderer before startRender() to control its supersampling level.
	virtual void setAntialiasingHint(int antialiasingLevel) {}

	/// Prepares the renderer for rendering and sets the dataset to be rendered.
	virtual bool startRender(const RenderSettings* settings, const QSize& frameBufferSize, MixedKeyCache& visCache);

	/// Returns the general rendering settings.
	/// This information is only available between calls to startRender() and endRender().
	const RenderSettings& renderSettings() const { OVITO_ASSERT(_renderSettings != nullptr); return *_renderSettings; }

	/// Is called after rendering has finished.
	virtual void endRender();

	/// Returns the view projection parameters.
	const ViewProjectionParameters& projParams() const { return _projParams; }

	/// Changes the view projection parameters.
	void setProjParams(const ViewProjectionParameters& params) { _projParams = params; }

	/// Returns the animation time being rendered.
	AnimationTime time() const { return _time; }

	/// Returns the animation frame being rendered.
	int frame() const { return _time.frame(); }

	/// Returns the viewport whose contents are currently being rendered.
	/// This may be NULL.
	Viewport* viewport() const { return _viewport; }

	/// Returns the framebuffer we are rendering into (is null for interactive renderers).
	FrameBuffer* frameBuffer() const { return _frameBuffer; }

	/// Returns the rectangular region of the framebuffer we are rendering into (in device coordinates).
	const QRect& viewportRect() const { return _viewportRect; }

	/// Returns the scene currently being rendered. Only valid between calls to beginFrame()/endFrame().
	Scene* scene() const { OVITO_ASSERT(_scene); return _scene; }

	/// Returns the device pixel ratio of the output device we are rendering to.
	virtual qreal devicePixelRatio() const;

	/// \brief Computes the bounding box of the entire scene to be rendered.
	/// \return An axis-aligned box in the world coordinate system that contains
	///         everything to be rendered.
	Box3 computeSceneBoundingBox(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp);

	/// Sets the view projection parameters, the animation frame to render,
	/// and the viewport being rendered.
	virtual void beginFrame(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer);

	/// Renders the current animation frame.
	/// Returns false if the operation has been canceled by the user.
	virtual bool renderFrame(const QRect& viewportRect) = 0;

	/// Renders the overlays/underlays of the viewport into the framebuffer.
	/// Returns false if the operation has been canceled by the user.
	virtual bool renderOverlays(bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect);

	/// This method is called after renderFrame() has been called.
	virtual void endFrame(bool renderingSuccessful, const QRect& viewportRect);

	/// Returns the data cache to be used by visualization elements.
	MixedKeyCache& visCache() const { OVITO_ASSERT(_visCache != nullptr); return *_visCache; }

	/// Changes the current local-to-world transformation matrix.
	void setWorldTransform(const AffineTransformation& tm) {
		_modelWorldTM = tm;
		_modelViewTM = projParams().viewMatrix * tm;
	}

	/// Returns the current local-to-world transformation matrix.
	const AffineTransformation& worldTransform() const { return _modelWorldTM; }

	/// Returns the current model-to-view transformation matrix.
	const AffineTransformation& modelViewTM() const { return _modelViewTM; }

	/// Renders the line geometry stored in the given buffer.
	virtual void renderLines(const LinePrimitive& primitive) {}

	/// Renders the particles stored in the given primitive buffer.
	virtual void renderParticles(const ParticlePrimitive& primitive) {}

	/// Renders the marker geometry stored in the given buffer.
	virtual void renderMarkers(const MarkerPrimitive& primitive) {}

	/// Renders the text stored in the given primitive buffer.
	virtual void renderText(const TextPrimitive& primitive) {}

	/// Renders the image stored in the given primitive buffer.
	virtual void renderImage(const ImagePrimitive& primitive) {}

	/// Renders the cylinder or arrow elements stored in the given buffer.
	virtual void renderCylinders(const CylinderPrimitive& primitive) {}

	/// Renders the triangle mesh stored in the given buffer.
	virtual void renderMesh(const MeshPrimitive& primitive) {}

	/// Renders a 2d polyline or polygon into an interactive viewport.
	void render2DPolyline(const Point2* points, int count, const ColorA& color, bool closed);

	/// Returns whether this renderer is rendering an interactive viewport.
	/// \return true if rendering a real-time viewport; false if rendering a static image.
	bool isInteractive() const { return _isInteractive; }

	/// Sets the interactive mode of the scene renderer.
	void setInteractive(bool isInteractive) { _isInteractive = isInteractive; }

	/// Returns whether object picking information is recorded during the current rendering pass.
	bool isPickingPass() const { return _isPickingPass; }

	/// Sets whether whether object picking information is recorded during the current rendering pass.
	void setPickingPass(bool enable) { _isPickingPass = enable; }

	/// Returns whether a visual image is being produced during the current rendering pass (i.e., not just recording object picking information).
	bool isImagePass() const { return _isImagePass; }

	/// Sets whether a visual image is being produced during the current rendering pass (i.e., not just recording object picking information).
	void setImagePass(bool enable) { _isImagePass = enable; }

	/// Returns whether bounding box calculation pass is active.
	bool isBoundingBoxPass() const { return _isBoundingBoxPass; }

	/// Adds a bounding box given in local coordinates to the global bounding box.
	/// This method must be called during the bounding box render pass.
	void addToLocalBoundingBox(const Box3& bb) {
		_sceneBoundingBox.addBox(bb.transformed(worldTransform()));
	}

	/// Adds a point given in local coordinates to the global bounding box.
	/// This method must be called during the bounding box render pass.
	void addToLocalBoundingBox(const Point3& p) {
		_sceneBoundingBox.addPoint(worldTransform() * p);
	}

	/// When picking mode is active, this registers an object being rendered.
	quint32 beginPickObject(const Pipeline* pipeline, ObjectPickInfo* pickInfo = nullptr);

	/// Registers a range of sub-IDs belonging to the current object being rendered.
	quint32 registerSubObjectIDs(quint32 subObjectCount, const ConstDataBufferPtr& indices = {});

	/// Call this when rendering of a pickable object is finished.
	void endPickObject();

	/// Resets the picking buffer and clears the stored object records.
	virtual void resetPickingBuffer();

	/// Given an object picking ID, looks up the corresponding record.
	const ObjectPickingRecord* lookupObjectPickingRecord(quint32 objectID) const;

	/// Returns the line rendering width to use in object picking mode.
	virtual FloatType defaultLinePickingWidth();

	/// Temporarily enables/disables the depth test while rendering.
	/// This method is mainly used with the interactive viewport renderer.
	virtual void setDepthTestEnabled(bool enabled) {}

	/// Activates the special highlight rendering mode.
	/// This method is mainly used with the interactive viewport renderer.
	virtual void setHighlightMode(int pass) {}

	/// Computes the world size of an object that should appear one pixel wide in the rendered image.
	FloatType projectedPixelSize(const Point3& worldPosition) const;

	/// Indicates whether the scene renderer is allowed to block execution until long-running
	/// operations, e.g. data pipeline evaluation, complete. By default, this method returns
	/// true if isInteractive() returns false and vice versa.
	virtual bool waitForLongOperationsEnabled() const { return !isInteractive(); }

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const { return QImage::Format_ARGB32_Premultiplied; }

protected:

	/// Constructor.
	using RefTarget::RefTarget;

	/// \brief Renders all nodes in the scene.
	virtual bool renderScene();

	/// \brief Render a scene node (and all its children).
	virtual bool renderNode(SceneNode* node);

	/// \brief This virtual method is responsible for rendering additional content that is only
	///       visible in the interactive viewports.
	virtual void renderInteractiveContent();

	/// \brief Renders the visual representation of the modifiers.
	void renderModifiers(bool renderOverlay);

	/// \brief Renders the visual representation of the modifiers.
	void renderModifiers(Pipeline* pipeline, bool renderOverlay);

	/// \brief Gets the trajectory of motion of a node. The returned data buffer stores an array of
	///        Point3 (if the node's position is animated) or a null pointer (if the node's position is static).
	ConstDataBufferPtr getNodeTrajectory(const SceneNode* node);

	/// \brief Renders the trajectory of motion of a node in the interactive viewports.
	void renderNodeTrajectory(const SceneNode* node);

	/// Determines the range of the construction grid to display.
	std::tuple<FloatType, Box2I> determineGridRange(Viewport* vp);

	/// Renders the construction grid in a viewport.
	void renderGrid();

	/// Renders a text primitive by means of a cached image primitive.
	void renderTextDefaultImplementation(const TextPrimitive& primitive);

private:

	/// Renders a data object and all its sub-objects.
	void renderDataObject(const DataObject* dataObj, const Pipeline* pipeline, const PipelineFlowState& state, ConstDataObjectPath& dataObjectPath);

	/// The render settings for the current rendering pass.
	const RenderSettings* _renderSettings = nullptr;

	/// The scene being rendered in the current frame.
	OORef<Scene> _scene;

	/// The viewport whose contents are currently being rendered.
	OORef<Viewport> _viewport;

	/// The framebuffer we are rendering into (is null for interactive renderers).
	FrameBuffer* _frameBuffer = nullptr;

	/// The view projection parameters.
	ViewProjectionParameters _projParams;

	/// The current model-to-world transformation matrix.
	AffineTransformation _modelWorldTM = AffineTransformation::Identity();

	/// The current model-to-view transformation matrix.
	AffineTransformation _modelViewTM = AffineTransformation::Identity();

	/// The animation time being rendered.
	AnimationTime _time;

	/// The data cache to be used by visualization elements.
	MixedKeyCache* _visCache = nullptr;

	/// Indicates that object picking information is being recorded during the current rendering pass.
	bool _isPickingPass = false;

	/// Indicates that this is a rendering pass that produces a visual image (not just for recording object picking information).
	bool _isImagePass = true;

	/// Indicates that this is a real-time renderer for an interactive viewport.
	bool _isInteractive = false;

	/// Indicates that a bounding box pass is active.
	bool _isBoundingBoxPass = false;

	/// The rectangular region of the framebuffer we are rendering into (in device coordinates).
	QRect _viewportRect;

	/// Working variable used for computing the bounding box of the entire scene.
	Box3 _sceneBoundingBox;

	/// The next available object record for picking.
	ObjectPickingRecord _currentObjectPickingRecord;

	/// The next available object ID for object picking.
	quint32 _nextAvailablePickingID;

	/// The list of registered objects for picking.
	std::vector<ObjectPickingRecord> _objectPickingRecords;
};

/*
 * Data structure returned by the ViewportWindowInterface::pick() method,
 * holding information about the object that was picked in a viewport at the current cursor location.
 */
class OVITO_CORE_EXPORT ViewportPickResult
{
public:

	/// Indicates whether an object was picked or not.
	bool isValid() const { return (bool)_pipeline; }

	/// Returns the pipeline that has been picked.
	Pipeline* pipeline() const { return _pipeline; }

	/// Sets the pipeline that has been picked.
	void setPipeline(Pipeline* pipeline) { _pipeline = pipeline; }

	/// Returns the object-specific data at the pick location.
	ObjectPickInfo* pickInfo() const { return _pickInfo; }

	/// Sets the object-specific data at the pick location.
	void setPickInfo(ObjectPickInfo* info) { _pickInfo = info; }

	/// Returns the coordinates of the hit point in world space.
	const Point3& hitLocation() const { return _hitLocation; }

	/// Sets the coordinates of the hit point in world space.
	void setHitLocation(const Point3& location) { _hitLocation = location; }

	/// Returns the subobject that was picked.
	quint32 subobjectId() const { return _subobjectId; }

	/// Sets the subobject that was picked.
	void setSubobjectId(quint32 id) { _subobjectId = id; }

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

}	// End of namespace
