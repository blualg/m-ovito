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

#include <ovito/core/Core.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/dataset/DataSet.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ViewportConfiguration);
DEFINE_VECTOR_REFERENCE_FIELD(ViewportConfiguration, viewports);
DEFINE_REFERENCE_FIELD(ViewportConfiguration, activeViewport);
DEFINE_REFERENCE_FIELD(ViewportConfiguration, maximizedViewport);
DEFINE_REFERENCE_FIELD(ViewportConfiguration, layoutRootCell);

/******************************************************************************
* Is called when the value of a reference field of this RefMaker changes.
******************************************************************************/
void ViewportConfiguration::referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex)
{
    if(field == PROPERTY_FIELD(layoutRootCell)) {
        if(!isBeingLoaded() && !isBeingDeleted())
            updateListOfViewports();
    }
    RefTarget::referenceReplaced(field, oldTarget, newTarget, listIndex);
}

/******************************************************************************
* Is called when a RefTarget referenced by this object generated an event.
******************************************************************************/
bool ViewportConfiguration::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::TargetChanged) {
        if(source == layoutRootCell() && !isBeingLoaded() && !isBeingDeleted()) {
            updateListOfViewports();
        }
    }
    return RefTarget::referenceEvent(source, event);
}

/******************************************************************************
* Zooms all viewports to the extents of the currently selected nodes.
******************************************************************************/
void ViewportConfiguration::zoomToSelectionExtents()
{
    // Forward request to the associated viewport window(s).
    for(Viewport* vp : viewports())
        vp->notifyDependents(Viewport::ZoomToSelectionExtentsRequested);
}

/******************************************************************************
* Zooms all viewports to the extents of the scene when all scene pipelines
* have been fully evaluated and the extents are known.
******************************************************************************/
void ViewportConfiguration::zoomToSceneExtentsWhenReady()
{
    // Forward request to the associated viewport window(s).
    for(Viewport* vp : viewports())
        vp->notifyDependents(Viewport::ZoomToSceneExtentsWhenReadyRequested);
}

/******************************************************************************
* Helper function for recursively gathering all viewports in a layout tree.
******************************************************************************/
static void gatherViewportsFromLayout(const ViewportLayoutCell* cell, std::vector<Viewport*>& viewportList)
{
    if(cell) {
        if(cell->viewport())
            viewportList.push_back(cell->viewport());
        for(const ViewportLayoutCell* child : cell->children())
            gatherViewportsFromLayout(child, viewportList);
    }
}

/******************************************************************************
* Rebuilds the linear list of all viewports that are part of the current viewport layout tree.
******************************************************************************/
void ViewportConfiguration::updateListOfViewports()
{
    std::vector<Viewport*> viewportList;
    gatherViewportsFromLayout(layoutRootCell(), viewportList);
    _viewports.setTargets(this, PROPERTY_FIELD(viewports), std::move(viewportList));
}

/******************************************************************************
* This method is called once for this object after it has been completely
* loaded from a stream.
******************************************************************************/
void ViewportConfiguration::loadFromStreamComplete(ObjectLoadStream& stream)
{
    RefTarget::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.5.4:
    // Create a standard viewport layout for the linear list of viewports loaded from the old session state.
    if(!layoutRootCell()) {
        OVITO_ASSERT(viewports().size() == 4);

        OORef<ViewportLayoutCell> rootCell = OORef<ViewportLayoutCell>::create();
        rootCell->setSplitDirection(ViewportLayoutCell::Horizontal);
        rootCell->addChild(OORef<ViewportLayoutCell>::create());
        rootCell->addChild(OORef<ViewportLayoutCell>::create());

        rootCell->children()[0]->setSplitDirection(ViewportLayoutCell::Vertical);
        rootCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create());
        rootCell->children()[0]->addChild(OORef<ViewportLayoutCell>::create());
        rootCell->children()[0]->children()[0]->setViewport(viewports().size() > 0 ? viewports()[0] : nullptr); // Upper left
        rootCell->children()[0]->children()[1]->setViewport(viewports().size() > 2 ? viewports()[2] : nullptr); // Lower left

        rootCell->children()[1]->setSplitDirection(ViewportLayoutCell::Vertical);
        rootCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create());
        rootCell->children()[1]->addChild(OORef<ViewportLayoutCell>::create());
        rootCell->children()[1]->children()[0]->setViewport(viewports().size() > 1 ? viewports()[1] : nullptr); // Upper right
        rootCell->children()[1]->children()[1]->setViewport(viewports().size() > 3 ? viewports()[3] : nullptr); // Lower right
        setLayoutRootCell(std::move(rootCell));
    }
}

}   // End of namespace
