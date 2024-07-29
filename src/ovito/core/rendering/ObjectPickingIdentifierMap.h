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

	/// Prepares the mapping.
	void prepare(const FrameGraph& frameGraph, quint32 startObjectID = 1);

	/// Post-processes the mapping after acquisition.
	void postprocess();

	/// Releases all data held by the object.
	virtual void reset() {
		_pickingGroups.clear();
	}

	/// Registers a range of unique IDs for the current object picking group being rendered.
	quint32 allocateObjectPickingIDs(int pickingGroupID, quint32 objectCount, const ConstDataBufferPtr& indices = {});

	/// Finds the picked object at the given frame buffer pixel position.
	std::optional<ViewportWindow::PickResult> pickAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const;

    /// Returns the frame buffer object ID at the given frame buffer location.
    virtual quint32 objectIdentifierAt(const QPoint& frameBufferLocation) const { return 0; }

    /// Returns the z-value at the given frame buffer location.
    virtual FloatType depthAt(const QPoint& frameBufferLocation) const { return 0; }

    /// Computes the 3d world-space location corresponding to the given 2d window position.
    Point3 worldPositionAt(const QPoint& frameBufferLocation, const ViewProjectionParameters& projectionParams, const QSize& framebufferSize) const;

	/// Returns the informational text to be displayed in the status bar for a pickable scene object.
	QString pickableObjectInformationText(quint32 objectID) const;

private:

    /// A copy of a ObjectPickingGroup that has been encoded as a range of object IDs in the frame buffer.
	class MappedObjectGroup : public FrameGraph::ObjectPickingGroup
	{
	public:

		/// Constructor.
		using FrameGraph::ObjectPickingGroup::ObjectPickingGroup;

		/// Returns the base object ID at which the rendering primitives of this group start.
		quint32 baseObjectID() const { return _baseObjectID; }

		/// Sets the base object ID at which the rendering primitives of this group start.
		void setBaseObjectID(quint32 id) { _baseObjectID = id; }

		/// Registers a range of indexed rendering primitives.
		void addIndexedRange(const ConstDataBufferPtr& buffer, quint32 baseIndex) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
			_indexedRanges.emplace_back(buffer, baseIndex);
#else
			_indexedRanges.push_back(std::make_pair(buffer, baseIndex));
#endif
		}

		/// If the global object ID is within the range of this picking group, resolve it to the local object ID.
		quint32 resolveObjectID(quint32 objectID) const {
			OVITO_ASSERT(objectID >= baseObjectID());
			quint32 localID = objectID - baseObjectID();
			for(const auto& range : _indexedRanges) {
				if(localID >= range.second && localID < range.second + range.first->size()) {
					localID = range.second + BufferReadAccess<int32_t>(range.first).get(localID - range.second);
					break;
				}
			}
			return localID;
		}

	private:

		/// If the renderer uses an indexed drawing command, this information allows mapping the rendered primitive indices
		/// back to the original indices of the data object.
		QVarLengthArray<std::pair<ConstDataBufferPtr, quint32>, 1> _indexedRanges;

		/// The base object ID at which the rendering primitives of this group start.
		quint32 _baseObjectID = 0;
	};

	/// Given an frame buffer object ID, looks up the corresponding picking group.
	const MappedObjectGroup* lookupPickingGroupFromObjectId(quint32 objectID) const;

	/// The object picking groups.
	std::vector<MappedObjectGroup> _pickingGroups;

	/// The next available frame buffer object ID to be used.
	quint32 _nextAvailablePickingID = 0;
};

}   // End of namespace
