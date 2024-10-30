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
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/core/rendering/SceneRenderer.h>

namespace Ovito {

/**
 * Abstract base for editor components for SceneRenderer classes.
 */
class OVITO_GUI_EXPORT BaseSceneRendererEditor : public PropertiesEditor
{
    OVITO_CLASS(BaseSceneRendererEditor)
    Q_OBJECT

public:

    /// Constructor.
    BaseSceneRendererEditor();

    /// Creates an action widget that lets the user copy the settings of the current interactive renderer to/from the final-frame renderer.
    QWidget* createCopySettingsBetweenRenderersWidget(QWidget* parent = nullptr);

    /// Determines whether the settings of the given scene renderers can be transferred between each other.
    virtual bool canTransferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target) {
        return &source->getOOClass() == &target->getOOClass();
    }

    /// Copies the settings of one renderer to another (which can either be an interactive or a final-frame renderer).
    virtual void transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final) {}

public Q_SLOTS:

    /// Copies the settings of the interactive renderer to the final-frame renderer.
    void copySettingsInteractiveToFinalFrame();

    /// Copies the settings of the final-frame renderer to the interactive renderer.
    void copySettingsFinalFrameToInteractive();

Q_SIGNALS:

    /// This signal is emitted when the editor loads a scene renderer that is being used for interactive viewport rendering.
    void editingInteractiveRenderer();

    /// This signal is emitted when the editor loads a scene renderer that is being used for final frame rendering.
    void editingFinalFrameRenderer();
};

}   // End of namespace
