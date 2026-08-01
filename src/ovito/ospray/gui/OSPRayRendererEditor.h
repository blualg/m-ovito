////////////////////////////////////////////////////////////////////////////////////////
//
//  GUI plugin for the OSPRay renderer.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/rendering/BaseSceneRendererEditor.h>

namespace Ovito {

class OSPRayRendererEditor : public BaseSceneRendererEditor
{
    OVITO_CLASS(OSPRayRendererEditor)

protected:

    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

    virtual bool canTransferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target) override { return source && target && source->getOOClass() == target->getOOClass(); }

    virtual void transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final) override;
};

}   // End of namespace
