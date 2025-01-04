////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/utilities/concurrent/Future.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include "ActiveObject.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ActiveObject);
DEFINE_PROPERTY_FIELD(ActiveObject, isEnabled);
DEFINE_PROPERTY_FIELD(ActiveObject, title);
DEFINE_RUNTIME_PROPERTY_FIELD(ActiveObject, status);
DEFINE_SHADOW_PROPERTY_FIELD(ActiveObject, isEnabled);
DEFINE_SHADOW_PROPERTY_FIELD(ActiveObject, title);
SET_PROPERTY_FIELD_LABEL(ActiveObject, isEnabled, "Enabled");
SET_PROPERTY_FIELD_LABEL(ActiveObject, title, "Name");
SET_PROPERTY_FIELD_LABEL(ActiveObject, status, "Status");
SET_PROPERTY_FIELD_CHANGE_EVENT(ActiveObject, isEnabled, ReferenceEvent::TargetEnabledOrDisabled);
SET_PROPERTY_FIELD_CHANGE_EVENT(ActiveObject, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Is called when the value of a non-animatable property field of this RefMaker has changed.
******************************************************************************/
void ActiveObject::propertyChanged(const PropertyFieldDescriptor* field)
{
    // If the object is disabled, clear its status.
    if(field == PROPERTY_FIELD(isEnabled) && !isEnabled()) {
        setStatus(PipelineStatus::Success);
    }

    // Whenever the object's status changes, update UI.
    if(field == PROPERTY_FIELD(status)) {
        notifyDependents(ReferenceEvent::ObjectStatusChanged);
    }

    RefTarget::propertyChanged(field);
}

/******************************************************************************
* Increments the internal task counter and notifies the UI that this
* object is currently active.
******************************************************************************/
void ActiveObject::incrementNumberOfActiveTasks()
{
    if(_numberOfActiveTasks++ == 0) {
        notifyDependents(ActiveObject::ActivityChanged);
    }
}

/******************************************************************************
* Decrements the internal task counter and, if the counter has reached zero,
* notifies the UI that this object is no longer active.
******************************************************************************/
void ActiveObject::decrementNumberOfActiveTasks()
{
    OVITO_ASSERT(_numberOfActiveTasks > 0);
    if(--_numberOfActiveTasks == 0) {
        notifyDependents(ActiveObject::ActivityChanged);
    }
}

/******************************************************************************
* Registers the given future as an active task associated with this object.
******************************************************************************/
void ActiveObject::registerActiveFuture(const FutureBase& future)
{
    OVITO_ASSERT(future);
    if(!future.task()->isFinished() && Application::guiEnabled()) {
        incrementNumberOfActiveTasks();
        // Reset the pending status after the Future is fulfilled.
        future.finally(ObjectExecutor(this), [this]() noexcept { decrementNumberOfActiveTasks(); });
    }
}

/******************************************************************************
* Displays the given status information in the GUI for this object.
* The status is only displayed if the current frame of the pipeline matches the frame
* for which the status was generated.
******************************************************************************/
void ActiveObject::setStatusIfCurrentFrame(const PipelineStatus& status, const PipelineEvaluationRequest& request)
{
    // Don't show outcome of preliminary (interactive) pipeline evaluations or results for animation times other than the current one.
    if(request.interactiveMode())
        return;

    // No need to display status when there is no GUI.
    if(!Application::guiEnabled())
        return;

    if(request.time() != this_task::ui()->datasetContainer().currentAnimationTime())
        return;

    setStatus(status);
}

/******************************************************************************
* Returns a short piece of information (typically a string or color) to be
* displayed next to the object's title in the pipeline editor.
******************************************************************************/
QVariant ActiveObject::getPipelineEditorShortInfo(Scene* scene) const
{
    return status().shortInfo();
}

}   // End of namespace
