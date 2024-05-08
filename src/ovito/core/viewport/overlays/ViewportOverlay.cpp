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
#include <ovito/core/viewport/overlays/ViewportOverlay.h>
#include <ovito/core/dataset/scene/SelectionSet.h>
#include <ovito/core/viewport/Viewport.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ViewportOverlay);
DEFINE_REFERENCE_FIELD(ViewportOverlay, pipeline);
SET_PROPERTY_FIELD_LABEL(ViewportOverlay, pipeline, "Data source");
SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(ViewportOverlay, pipeline, "sourceNode"); // For backward compatibility with OVITO 3.9.2
//SET_PROPERTY_FIELD_ALIAS_IDENTIFIER(ViewportOverlay, pipeline, "dataSource"); // For backward compatibility with OVITO 3.9.2

/******************************************************************************
* Constructor.
******************************************************************************/
ViewportOverlay::ViewportOverlay(ObjectInitializationFlags flags) : ActiveObject(flags)
{
}

/******************************************************************************
* Is called when the overlay is being newly attached to a viewport.
******************************************************************************/
void ViewportOverlay::initializeOverlay(Viewport* viewport)
{
    // Automatically connect to the currently selected pipeline.
    if(!pipeline() && viewport->scene())
        setPipeline(dynamic_object_cast<Pipeline>(viewport->scene()->selection()->firstNode()));
}

/******************************************************************************
* Is called when the overlay is being newly attached to a viewport.
******************************************************************************/
void ViewportOverlay::sceneNodeAdded(SceneNode* node)
{
    // Automatically connect to the new pipeline.
    if(!pipeline())
        setPipeline(dynamic_object_cast<Pipeline>(node));
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ViewportOverlay::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == pipeline() && event.type() == ReferenceEvent::PipelineCacheUpdated) {
        // Send a ReferenceEvent::PipelineInputChanged event to the PropertiesEditor,
        // which should then emit a pipelineInputChanged signal to indicate that
        // new pipeline output data is available.
        notifyDependents(ReferenceEvent::PipelineInputChanged);
    }
    return ActiveObject::referenceEvent(source, event);
}

/******************************************************************************
* Helper method that checks whether the given Qt alignment value contains exactly one horizontal and one vertical alignment flag.
******************************************************************************/
void ViewportOverlay::checkAlignmentParameterValue(int alignment) const
{
    int horizontalAlignment = alignment & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter);
    int verticalAlignment = alignment & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter);

    if(horizontalAlignment == 0)
        throw Exception(tr("No horizontal alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));

    if(horizontalAlignment != Qt::AlignLeft && horizontalAlignment != Qt::AlignRight && horizontalAlignment != Qt::AlignHCenter)
        throw Exception(tr("More than one horizontal alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));

    if(verticalAlignment == 0)
        throw Exception(tr("No vertical alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));

    if(verticalAlignment != Qt::AlignTop && verticalAlignment != Qt::AlignBottom && verticalAlignment != Qt::AlignVCenter)
        throw Exception(tr("More than one vertical alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));
}

}   // End of namespace
