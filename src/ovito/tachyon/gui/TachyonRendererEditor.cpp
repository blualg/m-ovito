////////////////////////////////////////////////////////////////////////////////////////
//
//  GUI plugin for the Tachyon renderer.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/tachyon/TachyonRenderer.h>
#include "TachyonRendererEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TachyonRendererEditor);
SET_OVITO_OBJECT_EDITOR(TachyonRenderer, TachyonRendererEditor);

/******************************************************************************
* Creates the user interface controls for the editor.
******************************************************************************/
void TachyonRendererEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Tachyon renderer settings"), rolloutParams, "manual:rendering.tachyon_renderer");

    QVBoxLayout* rootLayout = new QVBoxLayout(rollout);
    rootLayout->setContentsMargins(4,4,4,4);

    auto addSeparator = [&]() {
        QFrame* line = new QFrame(rollout);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        rootLayout->addWidget(line);
    };

    BooleanParameterUI* enableAAUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TachyonRenderer::enableAntialiasing));
    rootLayout->addWidget(enableAAUI->checkBox());

    QGridLayout* aaLayout = new QGridLayout();
    aaLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    aaLayout->setSpacing(2);
#endif
    aaLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(aaLayout);

    IntegerParameterUI* antialiasingUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TachyonRenderer::antialiasingSamples));
    aaLayout->addWidget(antialiasingUI->label(), 0, 0);
    aaLayout->addLayout(antialiasingUI->createFieldLayout(), 0, 1);

    connect(enableAAUI->checkBox(), &QCheckBox::toggled, antialiasingUI, &ParameterUI::setEnabled);
    antialiasingUI->setEnabled(enableAAUI->checkBox()->isChecked());

    addSeparator();

    BooleanParameterUI* directLightUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TachyonRenderer::directLight));
    rootLayout->addWidget(directLightUI->checkBox());

    QGridLayout* directLayout = new QGridLayout();
    directLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    directLayout->setSpacing(2);
#endif
    directLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(directLayout);

    FloatParameterUI* directBrightnessUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TachyonRenderer::directLightBrightness));
    directLayout->addWidget(directBrightnessUI->label(), 0, 0);
    directLayout->addLayout(directBrightnessUI->createFieldLayout(), 0, 1);
    connect(directLightUI->checkBox(), &QCheckBox::toggled, directBrightnessUI, &ParameterUI::setEnabled);
    directBrightnessUI->setEnabled(directLightUI->checkBox()->isChecked());

    BooleanParameterUI* shadowsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TachyonRenderer::shadows));
    rootLayout->addWidget(shadowsUI->checkBox());

    addSeparator();

    BooleanParameterUI* aoUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TachyonRenderer::ambientOcclusion));
    rootLayout->addWidget(aoUI->checkBox());

    QGridLayout* aoLayout = new QGridLayout();
    aoLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    aoLayout->setSpacing(2);
#endif
    aoLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(aoLayout);

    FloatParameterUI* aoBrightnessUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TachyonRenderer::ambientOcclusionBrightness));
    aoLayout->addWidget(aoBrightnessUI->label(), 0, 0);
    aoLayout->addLayout(aoBrightnessUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* aoSamplesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TachyonRenderer::ambientOcclusionSamples));
    aoLayout->addWidget(aoSamplesUI->label(), 1, 0);
    aoLayout->addLayout(aoSamplesUI->createFieldLayout(), 1, 1);

    auto updateAOControls = [=]() {
        const bool shadowsEnabled = shadowsUI->checkBox()->isChecked();
        aoUI->checkBox()->setEnabled(shadowsEnabled);
        const bool aoEnabled = shadowsEnabled && aoUI->checkBox()->isChecked();
        aoBrightnessUI->setEnabled(aoEnabled);
        aoSamplesUI->setEnabled(aoEnabled);
    };
    connect(shadowsUI->checkBox(), &QCheckBox::toggled, rollout, updateAOControls);
    connect(aoUI->checkBox(), &QCheckBox::toggled, rollout, updateAOControls);
    updateAOControls();

    addSeparator();

    BooleanParameterUI* dofUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(TachyonRenderer::depthOfField));
    rootLayout->addWidget(dofUI->checkBox());

    QGridLayout* dofLayout = new QGridLayout();
    dofLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    dofLayout->setSpacing(2);
#endif
    dofLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(dofLayout);

    FloatParameterUI* focalLengthUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TachyonRenderer::focalLength));
    dofLayout->addWidget(focalLengthUI->label(), 0, 0);
    dofLayout->addLayout(focalLengthUI->createFieldLayout(), 0, 1);

    FloatParameterUI* apertureUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TachyonRenderer::aperture));
    dofLayout->addWidget(apertureUI->label(), 1, 0);
    dofLayout->addLayout(apertureUI->createFieldLayout(), 1, 1);

    connect(dofUI->checkBox(), &QCheckBox::toggled, focalLengthUI, &ParameterUI::setEnabled);
    connect(dofUI->checkBox(), &QCheckBox::toggled, apertureUI, &ParameterUI::setEnabled);
    focalLengthUI->setEnabled(dofUI->checkBox()->isChecked());
    apertureUI->setEnabled(dofUI->checkBox()->isChecked());

    QGroupBox* advancedBox = new QGroupBox(tr("Advanced"), rollout);
    rootLayout->addWidget(advancedBox);
    QGridLayout* advancedLayout = new QGridLayout(advancedBox);
    advancedLayout->setContentsMargins(4,4,4,4);
#ifndef Q_OS_MACOS
    advancedLayout->setSpacing(2);
#endif
    advancedLayout->setColumnStretch(1, 1);

    IntegerParameterUI* maxThreadsUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TachyonRenderer::maxThreads));
    advancedLayout->addWidget(maxThreadsUI->label(), 0, 0);
    advancedLayout->addLayout(maxThreadsUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* rayDepthUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TachyonRenderer::maxRayRecursion));
    advancedLayout->addWidget(rayDepthUI->label(), 1, 0);
    advancedLayout->addLayout(rayDepthUI->createFieldLayout(), 1, 1);

    IntegerParameterUI* transparentSurfacesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(TachyonRenderer::maxTransparentSurfaces));
    advancedLayout->addWidget(transparentSurfacesUI->label(), 2, 0);
    advancedLayout->addLayout(transparentSurfacesUI->createFieldLayout(), 2, 1);

    FloatParameterUI* materialAmbientUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TachyonRenderer::materialAmbient));
    advancedLayout->addWidget(materialAmbientUI->label(), 3, 0);
    advancedLayout->addLayout(materialAmbientUI->createFieldLayout(), 3, 1);

    FloatParameterUI* materialDiffuseUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TachyonRenderer::materialDiffuse));
    advancedLayout->addWidget(materialDiffuseUI->label(), 4, 0);
    advancedLayout->addLayout(materialDiffuseUI->createFieldLayout(), 4, 1);

    FloatParameterUI* materialSpecularUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(TachyonRenderer::materialSpecular));
    advancedLayout->addWidget(materialSpecularUI->label(), 5, 0);
    advancedLayout->addLayout(materialSpecularUI->createFieldLayout(), 5, 1);

    QLabel* limitationLabel = new QLabel(tr("First-pass Tachyon support renders particles, cylinders/bonds, triangle meshes, and basic line primitives. Volume rendering and special particle glyph shapes are not yet implemented."), rollout);
    limitationLabel->setWordWrap(true);
    rootLayout->addWidget(limitationLabel);
}

/******************************************************************************
* Copies the settings of one renderer to another.
******************************************************************************/
void TachyonRendererEditor::transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final)
{
    TachyonRenderer* sourceRenderer = dynamic_object_cast<TachyonRenderer>(source);
    TachyonRenderer* targetRenderer = dynamic_object_cast<TachyonRenderer>(target);
    if(sourceRenderer && targetRenderer) {
        targetRenderer->setEnableAntialiasing(sourceRenderer->enableAntialiasing());
        targetRenderer->setAntialiasingSamples(sourceRenderer->antialiasingSamples());
        targetRenderer->setDirectLight(sourceRenderer->directLight());
        targetRenderer->setDirectLightBrightness(sourceRenderer->directLightBrightness());
        targetRenderer->setShadows(sourceRenderer->shadows());
        targetRenderer->setAmbientOcclusion(sourceRenderer->ambientOcclusion());
        targetRenderer->setAmbientOcclusionBrightness(sourceRenderer->ambientOcclusionBrightness());
        targetRenderer->setAmbientOcclusionSamples(sourceRenderer->ambientOcclusionSamples());
        targetRenderer->setDepthOfField(sourceRenderer->depthOfField());
        targetRenderer->setFocalLength(sourceRenderer->focalLength());
        targetRenderer->setAperture(sourceRenderer->aperture());
        targetRenderer->setMaxRayRecursion(sourceRenderer->maxRayRecursion());
        targetRenderer->setMaxTransparentSurfaces(sourceRenderer->maxTransparentSurfaces());
        targetRenderer->setMaterialAmbient(sourceRenderer->materialAmbient());
        targetRenderer->setMaterialDiffuse(sourceRenderer->materialDiffuse());
        targetRenderer->setMaterialSpecular(sourceRenderer->materialSpecular());
        targetRenderer->setMaxThreads(sourceRenderer->maxThreads());
    }
}

}   // End of namespace
