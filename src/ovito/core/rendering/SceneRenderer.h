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
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/viewport/ViewProjectionParameters.h>
#include <ovito/core/viewport/Viewport.h>
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

	/// Constructor.
	using RefTarget::RefTarget;

    /// Lets the renderer perform post-processing of a newly generated frame graph.
    virtual void postprocessFrameGraph(FrameGraph& frameGraph) {}

	/// Prepares the renderer for rendering one or more frames.
	virtual void startRender(const QSize& frameBufferSize) {}

	/// Renders a single frame into the frame buffer.
	virtual void renderFrame(const FrameGraph& frameGraph, const QRect& viewportRect, FrameBuffer* frameBuffer) = 0;

	/// Is called after rendering of one or more frames has finished.
	virtual void endRender() {}

	/// This may be called on a renderer before startRender() to control its supersampling level.
	virtual void setMultisamplingLevel(int multisamplingLevel) {}

	/// Returns the multisampling level currently used by the renderer.
	virtual int multisamplingLevel() const { return 1; }

	/// Registers a range of sub-IDs belonging to the current object being rendered.
	quint32 registerSubObjectIDs(quint32 subObjectCount, const ConstDataBufferPtr& indices = {});

#if 0
	/// When picking mode is active, this registers an object being rendered.
	quint32 beginPickObject(const Pipeline* pipeline, ObjectPickInfo* pickInfo = nullptr);

	/// Call this when rendering of a pickable object is finished.
	void endPickObject();

	/// Resets the picking buffer and clears the stored object records.
	virtual void resetPickingBuffer();

	/// Given an object picking ID, looks up the corresponding record.
	const ObjectPickingRecord* lookupObjectPickingRecord(quint32 objectID) const;
#endif

	/// Returns the best format for QImage to be used when creating an ImagePrimitive.
	virtual QImage::Format preferredImageFormat() const { return QImage::Format_ARGB32_Premultiplied; }

#ifdef OVITO_BUILD_BASIC
	/// Creates an image serving as watermark for demo versions of scene renderers.
    QImage createWatermark(const QSize& size);
#endif

private:

#if 0
	/// The next available object record for picking.
	ObjectPickingRecord _currentObjectPickingRecord;

	/// The next available object ID for object picking.
	quint32 _nextAvailablePickingID;

	/// The list of registered objects for picking.
	std::vector<ObjectPickingRecord> _objectPickingRecords;
#endif
};

/*
 * Data structure returned by the ViewportWindow::pick() method,
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
