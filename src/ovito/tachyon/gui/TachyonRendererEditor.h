////////////////////////////////////////////////////////////////////////////////////////
//
//  GUI plugin for the Tachyon renderer.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/rendering/BaseSceneRendererEditor.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

class TachyonRendererEditor : public BaseSceneRendererEditor
{
    OVITO_CLASS(TachyonRendererEditor)

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    /// Copies the settings of one renderer to another.
    virtual void transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final) override;
};

}   // End of namespace
