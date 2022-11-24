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

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewportLayout.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ViewportLayoutCell);
DEFINE_REFERENCE_FIELD(ViewportLayoutCell, viewport);
DEFINE_VECTOR_REFERENCE_FIELD(ViewportLayoutCell, children);
DEFINE_PROPERTY_FIELD(ViewportLayoutCell, splitDirection);
DEFINE_PROPERTY_FIELD(ViewportLayoutCell, childWeights);

/******************************************************************************
* Constructor.
******************************************************************************/
ViewportLayoutCell::ViewportLayoutCell(ObjectCreationParams params) : RefTarget(params),
	_splitDirection(SplitDirection::None)
{
}

/******************************************************************************
* Inserts a sub-cell into this cell's list of children.
******************************************************************************/
void ViewportLayoutCell::addChild(OORef<ViewportLayoutCell> child, FloatType weight)
{ 
	_children.push_back(this, PROPERTY_FIELD(children), std::move(child));
	auto weights = childWeights();
	OVITO_ASSERT(weights.size() == children().size());
	weights.back() = weight;
	setChildWeights(std::move(weights));
	OVITO_ASSERT(childWeights().size() == children().size());
}

/******************************************************************************
* Inserts a sub-cell into this cell's list of children.
******************************************************************************/
void ViewportLayoutCell::insertChild(int index, OORef<ViewportLayoutCell> child, FloatType weight)
{ 
	OVITO_ASSERT(index >= 0 && index <= children().size());
	_children.insert(this, PROPERTY_FIELD(children), index, std::move(child));
	auto weights = childWeights();
	OVITO_ASSERT(weights.size() == children().size());
	weights[index] = weight;
	setChildWeights(std::move(weights));
	OVITO_ASSERT(childWeights().size() == children().size());
}

/******************************************************************************
* Removes a sub-cell from this cell's list of children.
******************************************************************************/
void ViewportLayoutCell::removeChild(int index) 
{ 
	OVITO_ASSERT(index >= 0 && index < children().size());
	_children.remove(this, PROPERTY_FIELD(children), index); 
	OVITO_ASSERT(childWeights().size() == children().size());
}

/******************************************************************************
* Is called when a RefTarget has been added to a VectorReferenceField of this RefMaker.
******************************************************************************/
void ViewportLayoutCell::referenceInserted(const PropertyFieldDescriptor* field, RefTarget* newTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(children) && !isBeingLoaded() && !isUndoingOrRedoing()) {
		auto weights = childWeights();
		OVITO_ASSERT(weights.size() + 1 == children().size());
		weights.insert(weights.begin() + listIndex, 1.0);
		setChildWeights(std::move(weights));
		OVITO_ASSERT(childWeights().size() == children().size());
	}
	RefTarget::referenceInserted(field, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget has been removed from a VectorReferenceField of this RefMaker.
******************************************************************************/
void ViewportLayoutCell::referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex)
{
	if(field == PROPERTY_FIELD(children) && !isBeingLoaded() && !isAboutToBeDeleted() && !isUndoingOrRedoing()) {
		auto weights = childWeights();
		OVITO_ASSERT(weights.size() == children().size() + 1);
		weights.erase(weights.begin() + listIndex);
		setChildWeights(std::move(weights));
		OVITO_ASSERT(childWeights().size() == children().size());
	}
	RefTarget::referenceRemoved(field, oldTarget, listIndex);
}

/******************************************************************************
* Returns the sum of all weights of the child cells.
******************************************************************************/
FloatType ViewportLayoutCell::totalChildWeights() const
{
	OVITO_ASSERT(childWeights().size() == children().size());
	return std::accumulate(childWeights().cbegin(), childWeights().cend(), FloatType(0.0));
}

/******************************************************************************
* Returns true if this cell's children have all the same size.
******************************************************************************/
bool ViewportLayoutCell::isEvenlySubdivided() const
{
	if(children().size() >= 2) {
		for(FloatType w : childWeights()) {
			if(!qFuzzyCompare(w, childWeights().front()))
				return false;
		}
	}
	return true;
}

/******************************************************************************
* Removes non-leaf nodes from the layout tree which have only a single child node.
******************************************************************************/
void ViewportLayoutCell::pruneViewportLayoutTree()
{
	for(ViewportLayoutCell* child : children())
		child->pruneViewportLayoutTree();
		
	if(children().size() == 1) {
		OORef<ViewportLayoutCell> singleChild = children().front();
		OVITO_ASSERT(singleChild->children().size() != 1);
		OVITO_ASSERT(singleChild->childWeights().size() == singleChild->children().size());
		setChildren(singleChild->children());
		setChildWeights(singleChild->childWeights());
		singleChild->setChildren({});
		setViewport(singleChild->viewport());
		singleChild->setViewport(nullptr);
		setSplitDirection(singleChild->splitDirection());
		OVITO_ASSERT(childWeights().size() == children().size());
	}
}

/******************************************************************************
* Returns the parent layout cell. 
******************************************************************************/
ViewportLayoutCell* ViewportLayoutCell::parentCell() const
{
	ViewportLayoutCell* result = nullptr;
	visitDependents([&](RefMaker* dependent) {
		if(ViewportLayoutCell* cell = dynamic_object_cast<ViewportLayoutCell>(dependent)) {
			OVITO_ASSERT(cell->children().contains(this));
			OVITO_ASSERT(!result);
			result = cell;
		}
	});
	return result;
}

/******************************************************************************
* Determines the effective rectangles for all the viewports in the layout hierarchy.
******************************************************************************/
void ViewportLayoutCell::getViewportRectangles(const QRectF& rect, std::vector<std::pair<Viewport*, QRectF>>& viewportRectangles, const QSizeF& borderSize) const
{
	if(viewport()) {
		viewportRectangles.push_back({ viewport(), rect });
	}
	else if(!children().empty()) {
		size_t index = 0;
		QRectF childRect = rect;
		qreal borderSizeDir = (splitDirection() == ViewportLayoutCell::Horizontal) ? borderSize.width() : borderSize.height();
		qreal effectiveAvailableSpace = (splitDirection() == ViewportLayoutCell::Horizontal) ? rect.width() : rect.height();
		effectiveAvailableSpace -= borderSizeDir * (children().size() - 1);
		if(effectiveAvailableSpace < 0) effectiveAvailableSpace = 0;
		FloatType totalChildWeights = this->totalChildWeights();
		if(totalChildWeights <= 0.0) totalChildWeights = 1.0;
		qreal x = 0;
		for(ViewportLayoutCell* child : children()) {
			if(index != children().size() - 1) {
				FloatType weight = 0;
				if(index < childWeights().size())
					weight = childWeights()[index];
				if(splitDirection() == ViewportLayoutCell::Horizontal) {
					qreal base = rect.left() + index * borderSizeDir;
					childRect.setLeft(base + effectiveAvailableSpace * (x / totalChildWeights));
					childRect.setWidth(effectiveAvailableSpace * (weight / totalChildWeights));
				}
				else {
					qreal base = rect.top() + index * borderSizeDir;
					childRect.setTop(base + effectiveAvailableSpace * (x / totalChildWeights));
					childRect.setHeight(effectiveAvailableSpace * (weight / totalChildWeights));
				}
				x += weight;
			}
			else {
				if(splitDirection() == ViewportLayoutCell::Horizontal) {
					qreal base = rect.left() + index * borderSizeDir;
					childRect.setLeft(base + effectiveAvailableSpace * (x / totalChildWeights));
					childRect.setRight(rect.right());
				}
				else {
					qreal base = rect.top() + index * borderSizeDir;
					childRect.setTop(base + effectiveAvailableSpace * (x / totalChildWeights));
					childRect.setBottom(rect.bottom());
				}
			}
			if(child)
				child->getViewportRectangles(childRect, viewportRectangles, borderSize);
			index++;
		}
	}
}

}	// End of namespace
