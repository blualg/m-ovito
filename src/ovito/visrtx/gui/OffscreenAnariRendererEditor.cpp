////////////////////////////////////////////////////////////////////////////////////////
//
//  GUI plugin for the VisRTX renderer.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/visrtx/OffscreenAnariRenderer.h>
#include "OffscreenAnariRendererEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OffscreenAnariRendererEditor);
SET_OVITO_OBJECT_EDITOR(OffscreenAnariRenderer, OffscreenAnariRendererEditor);

/******************************************************************************
* Creates the user interface controls for the editor.
******************************************************************************/
void OffscreenAnariRendererEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("VisRTX renderer settings"), rolloutParams, "manual:rendering.visrtx_renderer");

    QVBoxLayout* rootLayout = new QVBoxLayout(rollout);
    rootLayout->setContentsMargins(4,4,4,4);

    auto addSeparator = [&]() {
        QFrame* line = new QFrame(rollout);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        rootLayout->addWidget(line);
    };

    QGridLayout* runtimeLayout = new QGridLayout();
    runtimeLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    runtimeLayout->setSpacing(2);
#endif
    runtimeLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(runtimeLayout);

    StringParameterUI* runtimePathUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::anariRuntimePath));
    runtimeLayout->addWidget(new QLabel(tr("ANARI runtime path:"), rollout), 0, 0);
    runtimeLayout->addWidget(runtimePathUI->textBox(), 0, 1);

    StringParameterUI* libraryNameUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::anariLibraryName));
    runtimeLayout->addWidget(new QLabel(tr("ANARI library name:"), rollout), 1, 0);
    runtimeLayout->addWidget(libraryNameUI->textBox(), 1, 1);

    StringParameterUI* deviceSubtypeUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::deviceSubtype));
    runtimeLayout->addWidget(new QLabel(tr("Device subtype:"), rollout), 2, 0);
    runtimeLayout->addWidget(deviceSubtypeUI->textBox(), 2, 1);

    StringParameterUI* rendererSubtypeUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::rendererSubtype));
    runtimeLayout->addWidget(new QLabel(tr("Renderer subtype:"), rollout), 3, 0);
    runtimeLayout->addWidget(rendererSubtypeUI->textBox(), 3, 1);

    addSeparator();

    QGridLayout* qualityLayout = new QGridLayout();
    qualityLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    qualityLayout->setSpacing(2);
#endif
    qualityLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(qualityLayout);

    IntegerParameterUI* samplesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::samplesPerPixel));
    qualityLayout->addWidget(samplesUI->label(), 0, 0);
    qualityLayout->addLayout(samplesUI->createFieldLayout(), 0, 1);

    IntegerParameterUI* rayDepthUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::maxRayDepth));
    qualityLayout->addWidget(rayDepthUI->label(), 1, 0);
    qualityLayout->addLayout(rayDepthUI->createFieldLayout(), 1, 1);

    BooleanParameterUI* denoiseUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::denoise));
    rootLayout->addWidget(denoiseUI->checkBox());

    addSeparator();

    BooleanParameterUI* directLightUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::directLight));
    rootLayout->addWidget(directLightUI->checkBox());

    QGridLayout* directLayout = new QGridLayout();
    directLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    directLayout->setSpacing(2);
#endif
    directLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(directLayout);

    FloatParameterUI* directIrradianceUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::directLightIrradiance));
    directLayout->addWidget(directIrradianceUI->label(), 0, 0);
    directLayout->addLayout(directIrradianceUI->createFieldLayout(), 0, 1);
    connect(directLightUI->checkBox(), &QCheckBox::toggled, directIrradianceUI, &ParameterUI::setEnabled);
    directIrradianceUI->setEnabled(directLightUI->checkBox()->isChecked());

    addSeparator();

    QGridLayout* ambientLayout = new QGridLayout();
    ambientLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    ambientLayout->setSpacing(2);
#endif
    ambientLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(ambientLayout);

    FloatParameterUI* ambientRadianceUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::ambientRadiance));
    ambientLayout->addWidget(ambientRadianceUI->label(), 0, 0);
    ambientLayout->addLayout(ambientRadianceUI->createFieldLayout(), 0, 1);

    BooleanParameterUI* aoUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::ambientOcclusion));
    rootLayout->addWidget(aoUI->checkBox());

    QGridLayout* aoLayout = new QGridLayout();
    aoLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    aoLayout->setSpacing(2);
#endif
    aoLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(aoLayout);

    IntegerParameterUI* ambientSamplesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(OffscreenAnariRenderer::ambientSamples));
    aoLayout->addWidget(ambientSamplesUI->label(), 0, 0);
    aoLayout->addLayout(ambientSamplesUI->createFieldLayout(), 0, 1);
    connect(aoUI->checkBox(), &QCheckBox::toggled, ambientSamplesUI, &ParameterUI::setEnabled);
    ambientSamplesUI->setEnabled(aoUI->checkBox()->isChecked());

    QLabel* runtimeLabel = new QLabel(tr("Leave the runtime path empty to auto-search PATH, the application directory, ANARI/VisRTX environment roots, and common runtime install folders. You may also set it to the folder containing anari.dll. Use library name 'visrtx' for NVIDIA VisRTX. VisRTX requires a compatible NVIDIA GPU and driver."), rollout);
    runtimeLabel->setWordWrap(true);
    rootLayout->addWidget(runtimeLabel);
}

/******************************************************************************
* Copies the settings of one renderer to another.
******************************************************************************/
void OffscreenAnariRendererEditor::transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final)
{
    OffscreenAnariRenderer* sourceRenderer = dynamic_object_cast<OffscreenAnariRenderer>(source);
    OffscreenAnariRenderer* targetRenderer = dynamic_object_cast<OffscreenAnariRenderer>(target);
    if(sourceRenderer && targetRenderer) {
        targetRenderer->setAnariRuntimePath(sourceRenderer->anariRuntimePath());
        targetRenderer->setAnariLibraryName(sourceRenderer->anariLibraryName());
        targetRenderer->setDeviceSubtype(sourceRenderer->deviceSubtype());
        targetRenderer->setRendererSubtype(sourceRenderer->rendererSubtype());
        targetRenderer->setSamplesPerPixel(sourceRenderer->samplesPerPixel());
        targetRenderer->setDirectLight(sourceRenderer->directLight());
        targetRenderer->setDirectLightIrradiance(sourceRenderer->directLightIrradiance());
        targetRenderer->setAmbientRadiance(sourceRenderer->ambientRadiance());
        targetRenderer->setAmbientOcclusion(sourceRenderer->ambientOcclusion());
        targetRenderer->setAmbientSamples(sourceRenderer->ambientSamples());
        targetRenderer->setDenoise(sourceRenderer->denoise());
        targetRenderer->setMaxRayDepth(sourceRenderer->maxRayDepth());
    }
}

}   // End of namespace
