////////////////////////////////////////////////////////////////////////////////////////
//
//  GUI plugin for the OSPRay renderer.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/ospray/OSPRayRenderer.h>
#include "OSPRayRendererEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(OSPRayRendererEditor);
SET_OVITO_OBJECT_EDITOR(OSPRayRenderer, OSPRayRendererEditor);

/******************************************************************************
* Creates the user interface controls for the editor.
******************************************************************************/
void OSPRayRendererEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("OSPRay renderer settings"), rolloutParams, "manual:rendering.ospray_renderer");

    QVBoxLayout* rootLayout = new QVBoxLayout(rollout);
    rootLayout->setContentsMargins(4,4,4,4);

    QGridLayout* mainLayout = new QGridLayout();
    mainLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    mainLayout->setSpacing(2);
#endif
    mainLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(mainLayout);

    StringParameterUI* rendererTypeUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(OSPRayRenderer::rendererType));
    mainLayout->addWidget(new QLabel(tr("Renderer type:"), rollout), 0, 0);
    mainLayout->addWidget(rendererTypeUI->textBox(), 0, 1);

    IntegerParameterUI* samplesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(OSPRayRenderer::samplesPerPixel));
    mainLayout->addWidget(samplesUI->label(), 1, 0);
    mainLayout->addLayout(samplesUI->createFieldLayout(), 1, 1);

    StringParameterUI* libraryPathUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(OSPRayRenderer::libraryPath));
    mainLayout->addWidget(new QLabel(tr("OSPRay library path:"), rollout), 2, 0);
    mainLayout->addWidget(libraryPathUI->textBox(), 2, 1);

    auto addSeparator = [&]() {
        QFrame* line = new QFrame(rollout);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        rootLayout->addWidget(line);
    };

    addSeparator();

    BooleanParameterUI* directLightUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(OSPRayRenderer::directLight));
    rootLayout->addWidget(directLightUI->checkBox());

    QGridLayout* directLayout = new QGridLayout();
    directLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    directLayout->setSpacing(2);
#endif
    directLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(directLayout);

    FloatParameterUI* directIntensityUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(OSPRayRenderer::directLightIntensity));
    directLayout->addWidget(directIntensityUI->label(), 0, 0);
    directLayout->addLayout(directIntensityUI->createFieldLayout(), 0, 1);
    connect(directLightUI->checkBox(), &QCheckBox::toggled, directIntensityUI, &ParameterUI::setEnabled);
    directIntensityUI->setEnabled(directLightUI->checkBox()->isChecked());

    BooleanParameterUI* shadowsUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(OSPRayRenderer::shadows));
    rootLayout->addWidget(shadowsUI->checkBox());

    addSeparator();

    BooleanParameterUI* ambientLightUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(OSPRayRenderer::ambientLight));
    rootLayout->addWidget(ambientLightUI->checkBox());

    QGridLayout* ambientLayout = new QGridLayout();
    ambientLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    ambientLayout->setSpacing(2);
#endif
    ambientLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(ambientLayout);

    FloatParameterUI* ambientIntensityUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(OSPRayRenderer::ambientLightIntensity));
    ambientLayout->addWidget(ambientIntensityUI->label(), 0, 0);
    ambientLayout->addLayout(ambientIntensityUI->createFieldLayout(), 0, 1);

    connect(ambientLightUI->checkBox(), &QCheckBox::toggled, ambientIntensityUI, &ParameterUI::setEnabled);
    ambientIntensityUI->setEnabled(ambientLightUI->checkBox()->isChecked());

    BooleanParameterUI* aoUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(OSPRayRenderer::ambientOcclusion));
    rootLayout->addWidget(aoUI->checkBox());

    QGridLayout* aoLayout = new QGridLayout();
    aoLayout->setContentsMargins(0,0,0,0);
#ifndef Q_OS_MACOS
    aoLayout->setSpacing(2);
#endif
    aoLayout->setColumnStretch(1, 1);
    rootLayout->addLayout(aoLayout);

    IntegerParameterUI* aoSamplesUI = createParamUI<IntegerParameterUI>(PROPERTY_FIELD(OSPRayRenderer::ambientOcclusionSamples));
    aoLayout->addWidget(aoSamplesUI->label(), 0, 0);
    aoLayout->addLayout(aoSamplesUI->createFieldLayout(), 0, 1);

    connect(aoUI->checkBox(), &QCheckBox::toggled, aoSamplesUI, &ParameterUI::setEnabled);
    aoSamplesUI->setEnabled(aoUI->checkBox()->isChecked());

    QLabel* runtimeLabel = new QLabel(tr("Leave the OSPRay library path empty to auto-search PATH, the application directory, OSPRay environment roots, and common runtime install folders. You may also set it to the folder containing ospray.dll. First-pass support renders particles, bonds/curves, triangle meshes, and line primitives."), rollout);
    runtimeLabel->setWordWrap(true);
    rootLayout->addWidget(runtimeLabel);
}

/******************************************************************************
* Copies the settings of one renderer to another.
******************************************************************************/
void OSPRayRendererEditor::transferSettingsBetweenRenderers(SceneRenderer* source, SceneRenderer* target, bool isInteractive2final)
{
    OSPRayRenderer* sourceRenderer = dynamic_object_cast<OSPRayRenderer>(source);
    OSPRayRenderer* targetRenderer = dynamic_object_cast<OSPRayRenderer>(target);
    if(sourceRenderer && targetRenderer) {
        targetRenderer->setLibraryPath(sourceRenderer->libraryPath());
        targetRenderer->setRendererType(sourceRenderer->rendererType());
        targetRenderer->setSamplesPerPixel(sourceRenderer->samplesPerPixel());
        targetRenderer->setDirectLight(sourceRenderer->directLight());
        targetRenderer->setDirectLightIntensity(sourceRenderer->directLightIntensity());
        targetRenderer->setAmbientLight(sourceRenderer->ambientLight());
        targetRenderer->setAmbientLightIntensity(sourceRenderer->ambientLightIntensity());
        targetRenderer->setAmbientOcclusion(sourceRenderer->ambientOcclusion());
        targetRenderer->setAmbientOcclusionSamples(sourceRenderer->ambientOcclusionSamples());
        targetRenderer->setShadows(sourceRenderer->shadows());
    }
}

}   // End of namespace
