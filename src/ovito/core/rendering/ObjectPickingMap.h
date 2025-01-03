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
#include <ovito/core/rendering/FrameGraph.h>
#include <ovito/core/viewport/ViewportWindow.h>

namespace Ovito {

/**
 * \brief A mapping of frame buffer object IDs to picking groups.
 */
class OVITO_CORE_EXPORT ObjectPickingMap
{
public:

	/// Finds the picked object at the given frame buffer pixel position.
	virtual std::optional<ViewportWindow::PickResult> pickAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const = 0;

    /// Returns the z-value at the given frame buffer location.
    virtual FloatType depthAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const { return 0; }

    /// Computes the 3d world-space location corresponding to the given 2d window position.
    Point3 worldPositionAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const;

	/// Releases all data held by the object.
	virtual void reset() {
		_pickingRecords.clear();
	}

protected:

    /// Describes a pickable rendering primitive that has been encoded as a range of object IDs in the frame buffer.
	class PickingRecord
	{
	public:

		/// Constructor.
		explicit PickingRecord(const FrameGraph::RenderingCommand& command, ConstDataBufferPtr indices = {}, uint32_t rendererFlags = 0) :
			_sceneNode(command.sceneNode()), _pickInfo(command.pickInfo()), _pickElementOffset(command.pickElementOffset()), _indices(std::move(indices)), _rendererFlags(rendererFlags) {}

		/// Returns the picked pipeline scene node.
		const OORef<const SceneNode>& sceneNode() const { return _sceneNode; }

		/// Returns an optional object that knows what high-level data was picked.
		const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

		/// Returns the renderer-specific flags associated with this picking record.
		/// This flags field may be set by the renderer and then interpreted by the ObjectPickingMap sub-class.
		uint32_t rendererFlags() const { return _rendererFlags; }

		/// Resolves the given 0-based consecutive sub-object ID to an original sub-object ID of a ObjectPickInfo.
		uint32_t resolveSubObjectID(uint32_t objectID) const {
			if(_indices) {
				OVITO_ASSERT(objectID >= 0 && objectID < _indices->size());
				objectID = BufferReadAccess<int32_t>(_indices).get(objectID);
			}
			return objectID + _pickElementOffset;
		}

	private:

		/// If the renderer uses an indexed drawing command, this information allows mapping the packed object IDs in the frame buffer
		/// back to the original indices of the rendering primitive.
		ConstDataBufferPtr _indices;

		/// The pipeline scene node to which the picked rendering command belongs.
		/// Note: may be null in rare cases, e.g., when the AmbientOcclusionModifier renders particles using false colors.
		OORef<const SceneNode> _sceneNode;

		/// An optional object that knows what high-level data is being represented by this render command and which sub-elements it consists of.
		OORef<ObjectPickInfo> _pickInfo;

		/// If this rendering command is part of a composite object that requires multiple rendering commands,
		/// then this offset indicates where this command's primitive elements start in the composite range.
		uint32_t _pickElementOffset;

		/// Renderer-specific flags that may be used to store additional information from the renderer.
		/// This flags field may be interpreted by a ObjectPickingMap sub-class belonging to the renderer.
		uint32_t _rendererFlags = 0;
	};

	/// Given a linear object ID, looks up the corresponding picking record.
	std::pair<uint32_t, const PickingRecord*> lookupPickingRecordFromLinearId(uint32_t objectID) const;

	/// The picking infos for the rendered graphics primitives, indexed by base object ID.
	std::map<uint32_t, PickingRecord> _pickingRecords;
};

}   // End of namespace
