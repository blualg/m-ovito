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

#include <ovito/gui/desktop/GUI.h>
#include "ObjectStatusDisplay.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ObjectStatusDisplay);
DEFINE_REFERENCE_FIELD(ObjectStatusDisplay, activeObject);

/******************************************************************************
* Constructor.
******************************************************************************/
void ObjectStatusDisplay::initializeObject(PropertiesEditor* parentEditor)
{
    ParameterUI::initializeObject(parentEditor);
    _widget = new StatusWidget();
}

/******************************************************************************
* Destructor.
******************************************************************************/
ObjectStatusDisplay::~ObjectStatusDisplay()
{
    // Release GUI widget.
    delete statusWidget();
}

/******************************************************************************
* Returns the UI widget managed by this ParameterUI.
******************************************************************************/
StatusWidget* ObjectStatusDisplay::statusWidget() const
{
    return _widget;
}

/******************************************************************************
* This method is called when a new editable object has been assigned to the properties owner this
* parameter UI belongs to.
******************************************************************************/
void ObjectStatusDisplay::resetUI()
{
    ParameterUI::resetUI();

    // Determine the active object. Consider all nested editors.
    ActiveObject* activeObject = dynamic_object_cast<ActiveObject>(editObject());
    if(!activeObject) {
        PropertiesEditor* editor = this->editor()->parentEditor();
        while(editor) {
            activeObject = dynamic_object_cast<ActiveObject>(editor->editObject());
            if(activeObject)
                break;
            editor = editor->parentEditor();
        }
    }
    _activeObject.set(this, PROPERTY_FIELD(activeObject), activeObject);
    _updateTimer.stop();
    _isUpToDate = true;

    if(statusWidget()) {
        if(activeObject) {
            statusWidget()->setEnabled(isEnabled());
            statusWidget()->setStatus(activeObject->status());
        }
        else {
            statusWidget()->clearStatus();
            statusWidget()->setEnabled(false);
        }
    }
}

/******************************************************************************
* Sets the enabled state of the UI.
******************************************************************************/
void ObjectStatusDisplay::setEnabled(bool enabled)
{
    if(enabled == isEnabled())
        return;
    ParameterUI::setEnabled(enabled);
    if(statusWidget())
        statusWidget()->setEnabled(editObject() && isEnabled());
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool ObjectStatusDisplay::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == activeObject() && event.type() == ReferenceEvent::ObjectStatusChanged) {
        if(!_updateTimer.isActive()) {
            if(statusWidget())
                statusWidget()->setStatus(activeObject()->status());
            _isUpToDate = true;
            _updateTimer.start(100, Qt::CoarseTimer, this);
        }
        else {
            _isUpToDate = false;
        }
    }
    return ParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* Handles timer events for this object.
******************************************************************************/
void ObjectStatusDisplay::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == _updateTimer.timerId()) {
        OVITO_ASSERT(_updateTimer.isActive());
        if(_isUpToDate)
            _updateTimer.stop();
        else if(statusWidget())
            statusWidget()->setStatus(activeObject() ? activeObject()->status() : PipelineStatus());
        _isUpToDate = true;
    }
    ParameterUI::timerEvent(event);
}

}   // End of namespace
