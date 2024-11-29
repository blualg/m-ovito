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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/ParameterUI.h>
#include <ovito/gui/desktop/widgets/display/StatusWidget.h>

namespace Ovito {

/**
 * \brief Displays the PipelineStatus of the edited object.
 */
class OVITO_GUI_EXPORT ObjectStatusDisplay : public ParameterUI
{
    OVITO_CLASS(ObjectStatusDisplay)

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parentEditor);

    /// Destructor.
    ~ObjectStatusDisplay();

    /// Returns the UI widget managed by this ParameterUI.
    StatusWidget* statusWidget() const;

    /// This method is called when a new editable object has been assigned to the properties owner this
    /// parameter UI belongs to.
    virtual void resetUI() override;

    /// Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) override;

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Handles timer events for this object.
    virtual void timerEvent(QTimerEvent* event) override;

private:

    /// The UI widget component.
    QPointer<StatusWidget> _widget;

    /// Timer to throttle the refresh rate of the display.
    QBasicTimer _updateTimer;

    /// Indicates that the displayed status is up-to-date.
    bool _isUpToDate = true;

    /// The object whose status is being displayed.
    DECLARE_REFERENCE_FIELD(OORef<ActiveObject>, activeObject);
};

}   // End of namespace
