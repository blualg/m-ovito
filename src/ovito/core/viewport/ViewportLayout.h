////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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

namespace Ovito {

/**
 * \brief A node in the kd-tree layout of viewport windows.
 */
class OVITO_CORE_EXPORT ViewportLayoutCell : public RefTarget
{
	OVITO_CLASS(ViewportLayoutCell)

public:

	enum SplitDirection {
		None,
		Horizontal,
		Vertical
	};
	Q_ENUM(SplitDirection);

	/// Constructor.
	Q_INVOKABLE ViewportLayoutCell(ObjectInitializationFlags flags);

	/// Inserts a sub-cell into this cell's list of children.
	void addChild(OORef<ViewportLayoutCell> child, FloatType weight = 1.0);

	/// Inserts a sub-cell into this cell's list of children.
	void insertChild(qsizetype index, OORef<ViewportLayoutCell> child, FloatType weight);

	/// Inserts a sub-cell into this cell's list of children.
	/// This is an overload of the method above, which is used in the Python binding layer.
	void insertChild(qsizetype index, OORef<ViewportLayoutCell> child) {
		insertChild(index, std::move(child), 1.0);
	}

	/// Removes a sub-cell from this cell's list of children.
	void removeChild(qsizetype index);

	/// Returns true if this cell's children have all the same size.
	bool isEvenlySubdivided() const;

	/// Returns the sum of all weights of the child cells.
	FloatType totalChildWeights() const;

	/// Removes non-leaf nodes from the layout tree which have only a single child node.
	void pruneViewportLayoutTree();

	/// Returns the parent layout cell.
	ViewportLayoutCell* parentCell() const;

	/// Determines the effective rectangles for all the viewports in the layout hierarchy.
	void getViewportRectangles(const QRectF& rect, std::vector<std::pair<Viewport*, QRectF>>& viewportRectangles, const QSizeF& borderSize = QSizeF(0,0)) const;

protected:

	/// Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
	virtual void referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex) override;

	/// Is called when a RefTarget has been removed from a VectorReferenceField of this RefMaker.
	virtual void referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex) override;

private:

	/// The viewport occupying this layout cell. May be null for non-leaf cells.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<Viewport>, viewport, setViewport, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES);

	/// The first child of this layout cell. May be null for leaf cells.
	DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(OORef<ViewportLayoutCell>, children, setChildren);

	/// Split direction if this cell has children.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(SplitDirection, splitDirection, setSplitDirection);

	/// Relative widths of the child cells.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(std::vector<FloatType>, childWeights, setChildWeights);
};

}	// End of namespace
