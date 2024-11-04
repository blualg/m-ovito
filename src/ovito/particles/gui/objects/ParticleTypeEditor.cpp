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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/gui/util/ParticleSettingsPage.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/IntegerParameterUI.h>
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/dialogs/ImportFileDialog.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>
#include "ParticleTypeEditor.h"
#include <ovito/gui/desktop/widgets/general/MenuToolButton.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticleTypeEditor);
SET_OVITO_OBJECT_EDITOR(ParticleType, ParticleTypeEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ParticleTypeEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Particle Type"), rolloutParams);

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);

    QGroupBox* nameBox = new QGroupBox(tr("Particle type"), rollout);
    QGridLayout* gridLayout = new QGridLayout(nameBox);
    gridLayout->setContentsMargins(4,4,4,4);
    gridLayout->setColumnStretch(1, 1);
    layout1->addWidget(nameBox);

    // Numeric ID.
    gridLayout->addWidget(new QLabel(tr("Numeric ID:")), 0, 0);
    QLabel* numericIdLabel = new QLabel();
    gridLayout->addWidget(numericIdLabel, 0, 1);
    connect(this, &PropertiesEditor::contentsReplaced, [numericIdLabel](RefTarget* newEditObject) {
        if(ElementType* ptype = static_object_cast<ElementType>(newEditObject))
            numericIdLabel->setText(QString::number(ptype->numericId()));
        else
            numericIdLabel->setText({});
    });

    // Type name.
    StringParameterUI* namePUI = createParamUI<StringParameterUI>(PROPERTY_FIELD(ParticleType::name));
    gridLayout->addWidget(new QLabel(tr("Name:")), 1, 0);
    gridLayout->addWidget(namePUI->textBox(), 1, 1);

    connect(this, &PropertiesEditor::contentsReplaced, [namePUI](RefTarget* newEditObject) {
        // Update the placeholder text of the name input field to reflect the numeric ID of the current particle type.
        if(QLineEdit* lineEdit = qobject_cast<QLineEdit*>(namePUI->textBox())) {
            if(ElementType* ptype = dynamic_object_cast<ElementType>(newEditObject))
                lineEdit->setPlaceholderText(tr("‹%1›").arg(ElementType::generateDefaultTypeName(ptype->numericId())));
            else
                lineEdit->setPlaceholderText({});
        }
    });

    QGroupBox* appearanceBox = new QGroupBox(tr("Appearance"), rollout);
    gridLayout = new QGridLayout(appearanceBox);
    gridLayout->setContentsMargins(4,4,4,4);
    gridLayout->setColumnStretch(1, 1);
    layout1->addWidget(appearanceBox);

    // Display color parameter.
    ColorParameterUI* colorPUI = createParamUI<ColorParameterUI>(PROPERTY_FIELD(ParticleType::color));
    gridLayout->addWidget(colorPUI->label(), 0, 0);
    gridLayout->addWidget(colorPUI->colorPicker(), 0, 1);

    // Display radius parameter.
    FloatParameterUI* displayRadiusPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ParticleType::radius));
    gridLayout->addWidget(displayRadiusPUI->label(), 1, 0);
    gridLayout->addLayout(displayRadiusPUI->createFieldLayout(), 1, 1);
    displayRadiusPUI->spinner()->setStandardValue(0.0);
    displayRadiusPUI->textBox()->setPlaceholderText(tr("‹unspecified›"));

    // Shape type.
    VariantComboBoxParameterUI* particleShapeUI = createParamUI<VariantComboBoxParameterUI>(PROPERTY_FIELD(ParticleType::shape));
    particleShapeUI->comboBox()->addItem(tr("‹unspecified›"), QVariant::fromValue((int)ParticlesVis::Default));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_sphere.png"), tr("Sphere/Ellipsoid"), QVariant::fromValue((int)ParticlesVis::Sphere));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_circle.png"), tr("Circle"), QVariant::fromValue((int)ParticlesVis::Circle));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_cube.png"), tr("Cube/Box"), QVariant::fromValue((int)ParticlesVis::Box));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_square.png"), tr("Square"), QVariant::fromValue((int)ParticlesVis::Square));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_cylinder.png"), tr("Cylinder"), QVariant::fromValue((int)ParticlesVis::Cylinder));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_spherocylinder.png"), tr("Spherocylinder"), QVariant::fromValue((int)ParticlesVis::Spherocylinder));
    particleShapeUI->comboBox()->addItem(QIcon(":/particles/icons/particle_shape_mesh.png"), tr("Mesh/User-defined"), QVariant::fromValue((int)ParticlesVis::Mesh));
    gridLayout->addWidget(new QLabel(tr("Shape:")), 2, 0);
    gridLayout->addWidget(particleShapeUI->comboBox(), 2, 1, 1, 2);

    // Color presets menu.
    QToolButton* colorPresetsMenuButton = createPresetsMenuButton(tr("color"),
        // Loads the default parameter value.
        [](ParticleType* ptype) { ptype->setColor(ElementType::getDefaultColor(ptype->ownerProperty(), ptype->nameOrNumericId(), ptype->numericId(), true)); },
        // Saves the current parameter value as new default preset.
        [](const ParticleType* ptype) { ElementType::setDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), ptype->nameOrNumericId(), ptype->color()); },
        // Determines if the current parameter value differs from the saved default value or not.
        [](const ParticleType* ptype) { return (ptype->color() == ElementType::getDefaultColor(ptype->ownerProperty(), ptype->nameOrNumericId(), ptype->numericId(), true)); }
    );
    gridLayout->addWidget(colorPresetsMenuButton, 0, 2);

    // Display radius presets menu.
    QToolButton* displayRadiusPresetsMenuButton = createPresetsMenuButton(tr("display radius"),
        // Loads the default parameter value.
        [](ParticleType* ptype) { ptype->setRadius(ParticleType::getDefaultParticleRadius(static_cast<Particles::Type>(ptype->ownerProperty().typeId()), ptype->nameOrNumericId(), ptype->numericId(), true, ParticleType::DisplayRadius)); },
        // Saves the current parameter value as new default preset.
        [](const ParticleType* ptype) { ParticleType::setDefaultParticleRadius(Particles::TypeProperty, ptype->nameOrNumericId(), ptype->radius(), ParticleType::DisplayRadius); },
        // Determines if the current parameter value differs from the saved default value or not.
        [](const ParticleType* ptype) { return (ptype->radius() == ParticleType::getDefaultParticleRadius(static_cast<Particles::Type>(ptype->ownerProperty().typeId()), ptype->nameOrNumericId(), ptype->numericId(), true, ParticleType::DisplayRadius)); }
    );
    gridLayout->addWidget(displayRadiusPresetsMenuButton, 1, 2);

    QGroupBox* shapeGroupBox = new QGroupBox(tr("User-defined shape"), rollout);
    gridLayout = new QGridLayout(shapeGroupBox);
    gridLayout->setContentsMargins(4,4,4,4);
    gridLayout->setSpacing(2);
    layout1->addWidget(shapeGroupBox);
    shapeGroupBox->setVisible(false);

    // User-defined shape.
    QPushButton* loadShapeBtn = new QPushButton(tr("Load geometry file..."));
    loadShapeBtn->setToolTip(tr("Loads a mesh file to be used as shape for this particle type."));
    gridLayout->addWidget(loadShapeBtn, 0, 0, 1, 2);
    BooleanParameterUI* highlightEdgesUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleType::highlightShapeEdges));
    gridLayout->addWidget(highlightEdgesUI->checkBox(), 1, 0, 1, 2);
    BooleanParameterUI* shapeBackfaceCullingUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleType::shapeBackfaceCullingEnabled));
    gridLayout->addWidget(shapeBackfaceCullingUI->checkBox(), 2, 0, 1, 2);
    BooleanParameterUI* shapeUseMeshColorUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ParticleType::shapeUseMeshColor));
    gridLayout->addWidget(shapeUseMeshColorUI->checkBox(), 3, 0, 1, 2);

    // Show/hide controls for user-defined shapes depending on the selected shape type.
    connect(particleShapeUI->comboBox(), qOverload<int>(&QComboBox::currentIndexChanged), this, [this, shapeGroupBox, box = particleShapeUI->comboBox()](int index) {
        bool userDefinedShape = box->itemData(index).toInt() == ParticlesVis::Mesh;
        if(userDefinedShape != shapeGroupBox->isVisible()) {
            shapeGroupBox->setVisible(userDefinedShape);
            container()->updateRolloutsLater();
        }
    });

    // Update the shape buttons whenever the particle type is being modified.
    connect(this, &PropertiesEditor::contentsChanged, this, [=](RefTarget* editObject) {
        if(ParticleType* ptype = static_object_cast<ParticleType>(editObject)) {
            if(ptype->shapeMesh()) {
                loadShapeBtn->setText(tr("%1 faces / %2 vertices").arg(ptype->shapeMesh()->faceCount()).arg(ptype->shapeMesh()->vertexCount()));
                if(loadShapeBtn->icon().isNull())
                    loadShapeBtn->setIcon(QIcon(":/particles/icons/particle_shape_mesh.png"));
            }
            else {
                loadShapeBtn->setText(tr("Load geometry file..."));
                loadShapeBtn->setIcon({});
            }
            displayRadiusPUI->setEnabled(!ptype->radiusIsPrescribed());
        }
    });

    // Shape load button.
    connect(loadShapeBtn, &QPushButton::clicked, this, &ParticleTypeEditor::onLoadParticleShape);

    // Physical properties group.
    QGroupBox* physicalBox = new QGroupBox(tr("Physical properties"), rollout);
    gridLayout = new QGridLayout(physicalBox);
    gridLayout->setContentsMargins(4,4,4,4);
    gridLayout->setColumnStretch(1, 1);
    layout1->addWidget(physicalBox);

    // Mass parameter.
    FloatParameterUI* massPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ParticleType::mass));
    gridLayout->addWidget(massPUI->label(), 0, 0);
    gridLayout->addLayout(massPUI->createFieldLayout(), 0, 1);
    // Reset mass parameter - can't use createPresetsMenuButton because we only
    // offer reset but not the other options
    // Don't use PROPERTY_FIELD_RESETTABLE to give custom (better) tooltip
    MenuToolButton* presetsMenuButton = new MenuToolButton();
    {
        const QString& parameterName = PROPERTY_FIELD(ParticleType::mass)->displayName();
        QAction* loadPresetAction =
            presetsMenuButton->createAction(QIcon::fromTheme("particles_settings_restore"), tr("Reset %1 to default").arg(parameterName));
        loadPresetAction->setStatusTip(
            tr("Reset current %1 back to the hard-coded default value for this particle type.").arg(parameterName));
        connect(loadPresetAction, &QAction::triggered, this, [this, parameterName]() {
            if(ParticleType* ptype = static_object_cast<ParticleType>(editObject())) {
                performTransaction(tr("Reset particle type %1").arg(parameterName), [&]() {
                    ptype->setMass(ParticleType::getDefaultParticleMass(static_cast<Particles::Type>(ptype->ownerProperty().typeId()),
                                                                        ptype->nameOrNumericId(), ptype->numericId(), false));
                    mainWindow().showStatusBarMessage(
                        tr("Reset %1 of particle type '%2' to default value.").arg(parameterName).arg(ptype->nameOrNumericId()), 4000);
                });
            }
        });
    }
    gridLayout->addWidget(presetsMenuButton, 0, 2);

    massPUI->spinner()->setStandardValue(0.0);
    massPUI->textBox()->setPlaceholderText(tr("‹unspecified›"));

    // VDW radius parameter.
    FloatParameterUI* vdwRadiusPUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ParticleType::vdwRadius));
    gridLayout->addWidget(vdwRadiusPUI->label(), 1, 0);
    gridLayout->addLayout(vdwRadiusPUI->createFieldLayout(), 1, 1);
    vdwRadiusPUI->spinner()->setStandardValue(0.0);
    vdwRadiusPUI->textBox()->setPlaceholderText(tr("‹unspecified›"));

    // VDW radius presets menu.
    QToolButton* vdwRadiusPresetsMenuButton = createPresetsMenuButton(tr("VdW radius"),
        // Loads the default parameter value.
        [](ParticleType* ptype) { ptype->setVdwRadius(ParticleType::getDefaultParticleRadius(static_cast<Particles::Type>(ptype->ownerProperty().typeId()), ptype->nameOrNumericId(), ptype->numericId(), true, ParticleType::VanDerWaalsRadius)); },
        // Saves the current parameter value as new default preset.
        [](const ParticleType* ptype) { ParticleType::setDefaultParticleRadius(Particles::TypeProperty, ptype->nameOrNumericId(), ptype->vdwRadius(), ParticleType::VanDerWaalsRadius); },
        // Determines if the current parameter value differs from the saved default value or not.
        [](const ParticleType* ptype) { return (ptype->vdwRadius() == ParticleType::getDefaultParticleRadius(static_cast<Particles::Type>(ptype->ownerProperty().typeId()), ptype->nameOrNumericId(), ptype->numericId(), true, ParticleType::VanDerWaalsRadius)); }
    );
    gridLayout->addWidget(vdwRadiusPresetsMenuButton, 1, 2);
}

/******************************************************************************
* Creates a button that opens a menu for managing the presets for a particle type parameter.
******************************************************************************/
QToolButton* ParticleTypeEditor::createPresetsMenuButton(const QString& parameterName, std::function<void(ParticleType*)> resetFunc, std::function<void(const ParticleType*)> setDefaultFunc, std::function<bool(const ParticleType*)> isUnchangedFunc)
{
    MenuToolButton* presetsMenuButton = new MenuToolButton();
    QAction* loadPresetAction =
        presetsMenuButton->createAction(QIcon::fromTheme("particles_settings_restore"), tr("Reset %1 to default").arg(parameterName));
    loadPresetAction->setStatusTip(tr("Reset current %1 back to user-defined or hard-coded default value for this particle type.").arg(parameterName));
    connect(loadPresetAction, &QAction::triggered, this, [this,parameterName,resetFunc]() {
        if(ParticleType* ptype = static_object_cast<ParticleType>(editObject())) {
            performTransaction(tr("Reset particle type %1").arg(parameterName), [&]() {
                resetFunc(ptype);
                mainWindow().showStatusBarMessage(tr("Reset %1 of particle type '%2' to default value.").arg(parameterName).arg(ptype->nameOrNumericId()), 4000);
            });
        }
    });
    QAction* savePresetAction =
        presetsMenuButton->createAction(QIcon::fromTheme("file_save_as"), tr("Use current %1 as new default").arg(parameterName));
    savePresetAction->setStatusTip(tr("Save current %1 as future default value for this particle type.").arg(parameterName));
    connect(savePresetAction, &QAction::triggered, this, [this,parameterName,setDefaultFunc]() {
        if(ParticleType* ptype = static_object_cast<ParticleType>(editObject())) {
            setDefaultFunc(ptype);
            Q_EMIT contentsChanged(editObject());
            mainWindow().showStatusBarMessage(tr("Stored current %1 as default for particle type '%2'.").arg(parameterName).arg(ptype->nameOrNumericId()), 4000);
        }
    });
    presetsMenuButton->createMenuSeperator();
    QAction* editPresetAction = presetsMenuButton->createAction(QIcon::fromTheme("application_preferences"), tr("Edit presets..."));
    connect(editPresetAction, &QAction::triggered, this, [this]() {
        ApplicationSettingsDialog dlg(mainWindow(), &ParticleSettingsPage::OOClass());
        dlg.exec();
        Q_EMIT contentsChanged(editObject());
    });
    presetsMenuButton->setEnabled(false);
    presetsMenuButton->setToolTip(tr("Presets"));

    connect(this, &PropertiesEditor::contentsChanged, [loadPresetAction,savePresetAction,isUnchangedFunc](RefTarget* editObject) {
        if(ParticleType* ptype = static_object_cast<ParticleType>(editObject)) {
            bool hasDefaultValue = isUnchangedFunc(ptype);
            loadPresetAction->setEnabled(!hasDefaultValue);
            savePresetAction->setEnabled(!hasDefaultValue);
        }
    });

    connect(this, &PropertiesEditor::contentsReplaced, [presetsMenuButton](RefTarget* newEditObject) {
        presetsMenuButton->setEnabled(newEditObject != nullptr);
    });

    return presetsMenuButton;
}

/******************************************************************************
* Called when the user wants to pick and load a mesh-based particle shape from disk.
******************************************************************************/
void ParticleTypeEditor::onLoadParticleShape()
{
    OORef<ParticleType> ptype = static_object_cast<ParticleType>(editObject());
    if(!ptype)
        return;

    handleExceptions([&]() {
        // Build list of file importers that can import triangle meshes.
        QVector<const FileImporterClass*> meshImporters;
        for(const FileImporterClass* importerClass : PluginManager::instance().metaclassMembers<FileSourceImporter>()) {
            if(importerClass->importsDataType(TriangleMesh::OOClass()))
                meshImporters.push_back(importerClass);
        }

        // Let the user select a geometry file to import.
        ImportFileDialog fileDialog(meshImporters, &mainWindow(), tr("Load geometry file"), false, QStringLiteral("particle_shape_mesh"));
        if(fileDialog.exec() != QDialog::Accepted)
            return;

        auto [fileImporterClass, fileImporterFormat] = fileDialog.selectedFileImporter();

        // Asynchronously load the geometry from the selected file.
        auto future = ptype->loadShapeMesh(fileDialog.urlToImport(), fileImporterClass, fileImporterFormat).then(ObjectExecutor(ptype), [this, ptype](DataOORef<TriangleMesh> shapeMesh) {
            performTransaction(tr("Load particle shape"), [&]() {
                // Assign the mesh-based shape to the particle type.
                ptype->setShapeMesh(std::move(shapeMesh));

                // Also switch the particle type's visualization mode to mesh-based.
                ptype->setShape(ParticlesVis::Mesh);

                // Determine whether the mesh is a closed manifold.
                // If not, we should turn off back-face culling.
                if(ptype->shapeMesh() && !ptype->shapeMesh()->isClosed())
                    ptype->setShapeBackfaceCullingEnabled(false);
            });
        });

        // Show a progress dialog while performing the whole operation. The dialog will self-destruct afterwards.
        ProgressDialog::showForFuture(std::move(future), mainWindow(), parentWindow(), tr("Loading geometry file"));
    });
}

}   // End of namespace
