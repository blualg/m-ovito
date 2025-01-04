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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/rendering/BaseSceneRendererEditor.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

/******************************************************************************
* The editor component for the OpenGLRenderer class.
******************************************************************************/
class OpenGLRendererEditor : public BaseSceneRendererEditor
{
    OVITO_CLASS(OpenGLRendererEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Copies the settings of one renderer to another (which can either be an interactive or a final-frame renderer).
    virtual void transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final) override;
};

}   // End of namespace
