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
class OVITO_CORE_EXPORT ObjectPickingIdentifierMap
{
public:

	/// Releases all data held by the object.
	virtual void reset() {
		_nextAvailablePickingID = 1;
		_pickingRecords.clear();
	}

	/// Registers a range of unique IDs for the current object picking group being rendered.
	uint32_t allocateObjectPickingIDs(const FrameGraph::RenderingCommand& command, uint32_t objectCount, const ConstDataBufferPtr& indices = {}, uint32_t rendererFlags = 0);

	/// Finds the picked object at the given frame buffer pixel position.
	std::optional<ViewportWindow::PickResult> pickAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const;

    /// Returns the frame buffer object ID at the given frame buffer location.
    virtual uint32_t objectIdentifierAt(const QPoint& frameBufferLocation) const { return 0; }

    /// Returns the z-value at the given frame buffer location.
    virtual FloatType depthAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const { return 0; }

    /// Computes the 3d world-space location corresponding to the given 2d window position.
    Point3 worldPositionAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const;

	/// Returns the informational text to be displayed in the status bar for a pickable scene object.
	QString pickableObjectInformationText(uint32_t objectID) const;

protected:

    /// Describes a pickable rendering primitive that has been encoded as a range of object IDs in the frame buffer.
	class PickingRecord
	{
	public:

		/// Constructor.
		explicit PickingRecord(uint32_t baseObjectID, const ConstDataBufferPtr& indices, const FrameGraph::RenderingCommand& command, uint32_t rendererFlags) :
			_baseObjectID(baseObjectID), _indices(indices), _pipeline(command.pipeline()), _pickInfo(command.pickInfo()), _pickElementOffset(command.pickElementOffset()), _rendererFlags(rendererFlags) {}

		/// Returns the base object ID at which the rendering primitives start.
		uint32_t baseObjectID() const { return _baseObjectID; }

		/// Returns the picked scene pipeline.
		const OORef<const Pipeline>& pipeline() const { return _pipeline; }

		/// Returns an optional object that knows what high-level data was picked.
		const OORef<ObjectPickInfo>& pickInfo() const { return _pickInfo; }

		/// Returns the renderer-specific flags associated with this picking record.
		/// This flags field may be set by the renderer and then interpreted by the ObjectPickingIdentifierMap sub-class.
		uint32_t rendererFlags() const { return _rendererFlags; }

		/// If the global object ID is within the range of this picking group, resolve it to the local object ID.
		uint32_t resolveObjectID(uint32_t objectID) const {
			OVITO_ASSERT(objectID >= baseObjectID());
			uint32_t localID = objectID - baseObjectID();
			if(_indices) {
				OVITO_ASSERT(localID >= 0 && localID < _indices->size());
				localID = BufferReadAccess<int32_t>(_indices).get(localID);
			}
			return localID + _pickElementOffset;
		}

	private:

		/// If the renderer uses an indexed drawing command, this information allows mapping the packed object IDs in the frame buffer
		/// back to the original indices of the rendering primitive.
		ConstDataBufferPtr _indices;

		/// The scene pipeline to which this rendering command belongs.
		/// Note: may be null in rare cases, e.g., when the AmbientOcclusionModifier renders particles using false colors.
		OORef<const Pipeline> _pipeline;

		/// An optional object that knows what high-level data is being represented by this render command and which sub-elements it consists of.
		OORef<ObjectPickInfo> _pickInfo;

		/// The base object ID at which the rendering primitives of this group start.
		uint32_t _baseObjectID;

		/// If this rendering command is part of a composite object that requires multiple rendering commands,
		/// then this offset indicates where this command's primitive elements start in the composite range.
		uint32_t _pickElementOffset;

		/// Renderer-specific flags that may be used to store additional information from the renderer.
		/// This flags field may be interpreted by a ObjectPickingIdentifierMap sub-class belonging to the renderer.
		uint32_t _rendererFlags = 0;
	};

	/// Given an frame buffer object ID, looks up the corresponding picking record.
	const PickingRecord* lookupPickingRecordFromObjectId(uint32_t objectID) const;

	/// The picking infos of rendered primitives.
	std::vector<PickingRecord> _pickingRecords;

	/// The next available frame buffer object ID to be used.
	uint32_t _nextAvailablePickingID = 1;
};

}   // End of namespace
