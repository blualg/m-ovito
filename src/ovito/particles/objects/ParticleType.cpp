////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/io/FileSource.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include "ParticleType.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticleType);
OVITO_CLASSINFO(ParticleType, "DisplayName", "Particle type");
DEFINE_PROPERTY_FIELD(ParticleType, radius);
DEFINE_PROPERTY_FIELD(ParticleType, radiusIsPrescribed);
DEFINE_PROPERTY_FIELD(ParticleType, vdwRadius);
DEFINE_PROPERTY_FIELD(ParticleType, shape);
DEFINE_REFERENCE_FIELD(ParticleType, shapeMesh);
DEFINE_PROPERTY_FIELD(ParticleType, highlightShapeEdges);
DEFINE_PROPERTY_FIELD(ParticleType, shapeBackfaceCullingEnabled);
DEFINE_PROPERTY_FIELD(ParticleType, shapeUseMeshColor);
DEFINE_PROPERTY_FIELD(ParticleType, mass);
DEFINE_PROPERTY_FIELD(ParticleType, chemicalElement);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, radius);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, vdwRadius);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, shape);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, highlightShapeEdges);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, shapeBackfaceCullingEnabled);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, shapeUseMeshColor);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, mass);
DEFINE_SHADOW_PROPERTY_FIELD(ParticleType, chemicalElement);
SET_PROPERTY_FIELD_LABEL(ParticleType, radius, "Display radius");
SET_PROPERTY_FIELD_LABEL(ParticleType, vdwRadius, "Van der Waals radius");
SET_PROPERTY_FIELD_LABEL(ParticleType, shape, "Shape");
SET_PROPERTY_FIELD_LABEL(ParticleType, shapeMesh, "Shape Mesh");
SET_PROPERTY_FIELD_LABEL(ParticleType, highlightShapeEdges, "Highlight edges");
SET_PROPERTY_FIELD_LABEL(ParticleType, shapeBackfaceCullingEnabled, "Back-face culling");
SET_PROPERTY_FIELD_LABEL(ParticleType, shapeUseMeshColor, "Use mesh color");
SET_PROPERTY_FIELD_LABEL(ParticleType, mass, "Mass");
SET_PROPERTY_FIELD_LABEL(ParticleType, chemicalElement, "Chemical element");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ParticleType, radius, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(ParticleType, vdwRadius, WorldParameterUnit, 0);

/******************************************************************************
* Initializes the type's parameters to default values based on the type's name or numeric ID.
******************************************************************************/
void ParticleType::initializeTypeInternal(const QString& typeName, const OwnerPropertyRef& property, bool loadUserDefaults)
{
    ElementType::initializeTypeInternal(typeName, property, loadUserDefaults);

    // Load standard display radius.
    // First load the hardcoded default radius and freeze it, then load the user-defined default radius.
    setRadius(getDefaultParticleRadius(
        static_cast<Particles::Type>(property.typeId()), typeName, numericId(), false, RadiusVariant::DisplayRadius));
    freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ParticleType::radius)});
    if(loadUserDefaults)
        setRadius(getDefaultParticleRadius(
            static_cast<Particles::Type>(property.typeId()), typeName, numericId(), true, RadiusVariant::DisplayRadius));

    // Load standard van der Waals radius.
    // First load the hardcoded default radius and freeze it, then load the user-defined default radius.
    setVdwRadius(getDefaultParticleRadius(
        static_cast<Particles::Type>(property.typeId()), typeName, numericId(), false, RadiusVariant::VanDerWaalsRadius));
    freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ParticleType::vdwRadius)});
    if(loadUserDefaults)
        setVdwRadius(getDefaultParticleRadius(
            static_cast<Particles::Type>(property.typeId()), typeName, numericId(), true, RadiusVariant::VanDerWaalsRadius));

    // Load standard mass.
    // First load the hardcoded default mass and freeze it, then load the user-defined default mass.
    setMass(getDefaultParticleMass(static_cast<Particles::Type>(property.typeId()), typeName, numericId(), false));
    freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ParticleType::mass)});
    if(loadUserDefaults)
        setMass(getDefaultParticleMass(static_cast<Particles::Type>(property.typeId()), typeName, numericId(), true));

    // Determine chemical element.
    setChemicalElement(getDefaultChemicalElementForType(static_cast<Particles::Type>(property.typeId()), typeName, numericId(), false));
    freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(ParticleType::chemicalElement)});
    if(loadUserDefaults)
        setChemicalElement(getDefaultChemicalElementForType(static_cast<Particles::Type>(property.typeId()), typeName, numericId(), true));
}

/******************************************************************************
* Loads a mesh-based shape from a geometry file (but doesn't yet assign it to the ParticleType).
******************************************************************************/
Future<DataOORef<TriangleMesh>> ParticleType::loadShapeMesh(QUrl sourceUrl,
                                                            const FileImporterClass* importerClass,
                                                            const QString& importerFormat) const
{
    OVITO_ASSERT(this_task::get());
    OVITO_ASSERT(this_task::isMainThread());

    // Create the right importer for the file. May need to inspect input file to detect its format.
    Future<OORef<FileImporter>> importerFuture;
    if(!importerClass) {
        importerFuture = FileImporter::autodetectFileFormat(sourceUrl);
    }
    else {
        OORef<FileSourceImporter> importer = dynamic_object_cast<FileSourceImporter>(importerClass->createInstance());
        if(importer)
            importer->setSelectedFileFormat(importerFormat);
        importerFuture = std::move(importer);
    }

    // Wait for format detection to complete.
    OORef<FileSourceImporter> importer = dynamic_object_cast<FileSourceImporter>(co_await FutureAwaiter(ObjectExecutor(this), std::move(importerFuture)));
    if(!importer)
        throw Exception(tr("Could not detect the format of the geometry file. The format might not be supported."));

    OVITO_ASSERT(!CompoundOperation::isUndoRecording()); // Be sure that our actions are not recorded on the undo stack.

    // Create a temporary FileSource for loading the geometry data from the file.
    OORef<FileSource> fileSource = OORef<FileSource>::create();
    fileSource->setSource({std::move(sourceUrl)}, std::move(importer), false);

    // Evaluate the FileSource to load the data and obtain a data collection.
    PipelineFlowState state = co_await FutureAwaiter(ObjectExecutor(this), fileSource->evaluate(PipelineEvaluationRequest(AnimationTime(0), true)).asFuture());

    // Some error checking.
    if(state.status().type() == PipelineStatus::Error)
        throw Exception(state.status().text());
    if(!state)
        throw Exception(tr("The loaded geometry file does not contain any valid mesh."));

    // Extract the TriangleMesh object from the data collection.
    // Then discard the rest of the data collection and make the mesh mutable.
    DataOORef<const TriangleMesh> meshObjConst = state.expectObject<TriangleMesh>();
    state.reset();
    DataOORef<TriangleMesh> meshObj = std::move(meshObjConst).makeMutable();

    // Throw away any visual elements attached to the mesh object.
    meshObj->setVisElement(nullptr);

    // Show sharp edges of the mesh.
    meshObj->determineEdgeVisibility();

    co_return meshObj;
}

/******************************************************************************
* Is called once for this object after it has been completely loaded from a stream.
******************************************************************************/
void ParticleType::loadFromStreamComplete(ObjectLoadStream& stream)
{
    ElementType::loadFromStreamComplete(stream);

    // For backward compatibility with OVITO 3.3.5:
    // The 'shape' parameter field of the ParticleType class does not exist yet in state files written by older program versions.
    // Automatically switch the type's shape to 'Mesh' if a mesh geometry has been assigned to the type.
    if(stream.formatVersion() < 30007) {
        if(shape() == ParticlesVis::ParticleShape::Default && shapeMesh())
            setShape(ParticlesVis::ParticleShape::Mesh);
    }
}

/******************************************************************************
* Is called when the value of a property of this object has changed.
******************************************************************************/
void ParticleType::propertyChanged(const PropertyFieldDescriptor* field)
{
    ElementType::propertyChanged(field);

    if(field == PROPERTY_FIELD(chemicalElement) && !isUndoingOrRedoing() && !shouldIgnoreChanges()) {
        if(chemicalElement() != ParticleType::ChemicalElement::X) {
            // Update the particle type's color, mass and radii to match the selected chemical element.
            setColor(getChemicalElementColor(chemicalElement()));
            const QString& symbol = getChemicalElementSymbol(chemicalElement());
            if(name().isEmpty())
                setName(symbol);
            setRadius(getDefaultParticleRadius(
                static_cast<Particles::Type>(ownerProperty().typeId()), symbol, numericId(), true, RadiusVariant::DisplayRadius));
            setVdwRadius(getDefaultParticleRadius(
                static_cast<Particles::Type>(ownerProperty().typeId()), symbol, numericId(), true, RadiusVariant::VanDerWaalsRadius));
            setMass(getDefaultParticleMass(static_cast<Particles::Type>(ownerProperty().typeId()), symbol, numericId(), true));
        }
    }
    else if(field == PROPERTY_FIELD(ElementType::name) && !isUndoingOrRedoing() && !shouldIgnoreChanges() && !name().isEmpty()) {
        ChemicalElement el = getChemicalElementFromSymbol(name());
        if(el != ParticleType::ChemicalElement::X) {
            // Update the particle type's chemical element info. This in turn may update other parameters such as the color, mass, and radii.
            setChemicalElement(el);
        }
    }
}

// Define table of elements with default colors, radii, masses, etc.
//
// Van der Waals radii have been adopted from the VMD software, which adopted them from A. Bondi, J. Phys. Chem., 68, 441 - 452, 1964,
// except the value for H, which was taken from R.S. Rowland & R. Taylor, J. Phys. Chem., 100, 7384 - 7391, 1996.
// For radii that are not available in either of these publications use r = 2.0.
// The radii for ions (Na, K, Cl, Ca, Mg, and Cs) are based on the CHARMM27 Rmin/2 parameters for (SOD, POT, CLA, CAL, MG, CES).
//
// Some colors and covalent radii have been adopted from OpenBabel.
// clang-format off
const std::array<ParticleType::PredefinedChemicalType, (size_t)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES> ParticleType::_PredefinedChemicalTypes{{
    ParticleType::PredefinedChemicalType{ QStringLiteral("X"),  QStringLiteral("Unspecified"),  Color(255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f), 0.00f, 0.00f, 0.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("H"),  QStringLiteral("Hydrogen"),  Color(255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f), 0.46f, 1.20f, 1.00794 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("He"), QStringLiteral("Helium"), Color(217.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f), 1.22f, 1.40f, 4.00260 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Li"), QStringLiteral("Lithium"), Color(204.0f/255.0f, 128.0f/255.0f, 255.0f/255.0f), 1.57f, 1.82f, 6.941 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Be"), QStringLiteral("Beryllium"), Color(         0.76,          1.00,          0.00), 1.47f, 2.00f, 9.012182 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("B"),  QStringLiteral("Boron"), Color(         1.00,          0.71,          0.71), 2.01f, 2.00f, 10.811 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("C"),  QStringLiteral("Carbon"), Color(144.0f/255.0f, 144.0f/255.0f, 144.0f/255.0f), 0.77f, 1.70f, 12.0107 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("N"),  QStringLiteral("Nitrogen"), Color( 48.0f/255.0f,  80.0f/255.0f, 248.0f/255.0f), 0.74f, 1.55f, 14.0067 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("O"),  QStringLiteral("Oxygen"), Color(255.0f/255.0f,  13.0f/255.0f,  13.0f/255.0f), 0.74f, 1.52f, 15.9994 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("F"),  QStringLiteral("Fluorine"), Color(         0.50,          0.70,          1.00), 0.74f, 1.47f, 18.9984032 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ne"), QStringLiteral("Neon"), Color(         0.70,          0.89,          0.96), 0.74f, 1.54f, 20.1797 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Na"), QStringLiteral("Sodium"), Color(171.0f/255.0f,  92.0f/255.0f, 242.0f/255.0f), 1.91f, 1.36f, 22.989770 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Mg"), QStringLiteral("Magnesium"), Color(138.0f/255.0f, 255.0f/255.0f,   0.0f/255.0f), 1.60f, 1.18f, 24.3050 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Al"), QStringLiteral("Aluminum"), Color(191.0f/255.0f, 166.0f/255.0f, 166.0f/255.0f), 1.43f, 2.00f, 26.981538 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Si"), QStringLiteral("Silicon"), Color(240.0f/255.0f, 200.0f/255.0f, 160.0f/255.0f), 1.18f, 2.10f, 28.0855 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("P"),  QStringLiteral("Phosphorus"), Color(         1.00,          0.50,          0.00), 1.07f, 1.80f, 30.973761 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("S"),  QStringLiteral("Sulfur"), Color(         0.70,          0.70,          0.00), 1.05f, 1.80f, 32.065 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Cl"), QStringLiteral("Chlorine"), Color(         0.12,          0.94,          0.12), 1.02f, 2.27f, 35.453 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ar"), QStringLiteral("Argon"), Color(         0.50,          0.82,          0.89), 1.06f, 1.88f, 39.948 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("K"),  QStringLiteral("Potassium"), Color(         0.56,          0.25,          0.83), 2.03f, 1.76f, 39.0983 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ca"), QStringLiteral("Calcium"), Color(         0.24,          1.00,          0.00), 1.97f, 1.37f, 40.078 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Sc"), QStringLiteral("Scandium"), Color(         0.90,          0.90,          0.90), 1.70f, 2.00f, 44.955910 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Ti"), QStringLiteral("Titanium"), Color(191.0f/255.0f, 194.0f/255.0f, 199.0f/255.0f), 1.47f, 2.00f, 47.867 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("V"),  QStringLiteral("Vanadium"), Color(         0.65,          0.65,          0.67), 1.53f, 2.00f, 50.9415 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Cr"), QStringLiteral("Chromium"), Color(138.0f/255.0f, 153.0f/255.0f, 199.0f/255.0f), 1.29f, 2.00f, 51.9961 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Mn"), QStringLiteral("Manganese"), Color(         0.61,          0.48,          0.78), 1.39f, 2.00f, 54.938049 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Fe"), QStringLiteral("Iron"), Color(224.0f/255.0f, 102.0f/255.0f,  51.0f/255.0f), 1.26f, 2.00f, 55.845 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Co"), QStringLiteral("Cobalt"), Color(240.0f/255.0f, 144.0f/255.0f, 160.0f/255.0f), 1.25f, 2.00f, 58.9332 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ni"), QStringLiteral("Nickel"), Color( 80.0f/255.0f, 208.0f/255.0f,  80.0f/255.0f), 1.25f, 1.63f, 58.6934 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Cu"), QStringLiteral("Copper"), Color(200.0f/255.0f, 128.0f/255.0f,  51.0f/255.0f), 1.28f, 1.40f, 63.546 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Zn"), QStringLiteral("Zinc"), Color(125.0f/255.0f, 128.0f/255.0f, 176.0f/255.0f), 1.37f, 1.39f, 65.38, 65.409 }, // Zn has two atomic weights: the old mass (pre 2001) 65.409 and the updated mass 65.38 (https://www.ciaaw.org/zinc.htm)

    ParticleType::PredefinedChemicalType{ QStringLiteral("Ga"), QStringLiteral("Gallium"), Color(194.0f/255.0f, 143.0f/255.0f, 143.0f/255.0f), 1.53f, 1.07f, 69.723 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ge"), QStringLiteral("Germanium"), Color(102.0f/255.0f, 143.0f/255.0f, 143.0f/255.0f), 1.22f, 2.00f, 72.64 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("As"), QStringLiteral("Arsenic"), Color(         0.74,          0.50,          0.89), 1.19f, 1.85f, 74.92160 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Se"), QStringLiteral("Selenium"), Color(         1.00,          0.63,          0.00), 1.20f, 1.90f, 78.96 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Br"), QStringLiteral("Bromine"), Color(         0.65,          0.16,          0.16), 1.20f, 1.85f, 79.904 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Kr"), QStringLiteral("Krypton"), Color( 92.0f/255.0f, 184.0f/255.0f, 209.0f/255.0f), 1.98f, 2.02f, 83.798 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Rb"), QStringLiteral("Rubidium"), Color(         0.44,          0.18,          0.69), 2.20f, 2.00f, 85.4678 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Sr"), QStringLiteral("Strontium"), Color(         0.0f,          1.0f,      0.15259f), 2.15f, 2.00f, 87.62 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Y"),  QStringLiteral("Yttrium"), Color(     0.40259f,      0.59739f,      0.55813f), 1.82f, 2.00f, 88.90585 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Zr"), QStringLiteral("Zirconium"), Color(         0.0f,          1.0f,          0.0f), 1.60f, 2.00f, 91.224 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Nb"), QStringLiteral("Niobium"), Color(     0.29992f,          0.7f,      0.46459f), 1.47f, 2.00f, 92.90638 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Mo"), QStringLiteral("Molybdenum"), Color(         0.33,          0.71,          0.71), 1.54f, 2.00f, 95.94 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Tc"), QStringLiteral("Technetium"), Color(         0.23,          0.62,          0.62), 1.47f, 2.00f, 98.0 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Ru"), QStringLiteral("Ruthenium"), Color(         0.14,          0.56,          0.56), 1.46f, 2.00f, 101.07 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Rh"), QStringLiteral("Rhodium"), Color(         0.04,          0.49,          0.55), 1.42f, 2.00f, 102.90550 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Pd"), QStringLiteral("Palladium"), Color(  0.0f/255.0f, 105.0f/255.0f, 133.0f/255.0f), 1.37f, 1.63f, 106.42 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ag"), QStringLiteral("Silver"), Color(         0.88,          0.88,          1.00), 1.45f, 1.72f, 107.8682 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Cd"), QStringLiteral("Cadmium"), Color(         1.00,          0.85,          0.56), 1.44f, 1.58f, 112.411 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("In"), QStringLiteral("Indium"), Color(         0.65,          0.46,          0.45), 1.42f, 1.93f, 114.818 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Sn"), QStringLiteral("Tin"), Color(         0.40,          0.50,          0.50), 1.39f, 2.17f, 118.710 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Sb"), QStringLiteral("Antimony"), Color(         0.62,          0.39,          0.71), 1.39f, 2.00f, 121.760 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Te"), QStringLiteral("Tellurium"), Color(         0.83,          0.48,          0.00), 1.38f, 2.06f, 127.60 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("I"),  QStringLiteral("Iodine"), Color(         0.58,          0.00,          0.58), 1.39f, 1.98f, 126.90447 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Xe"), QStringLiteral("Xenon"), Color(         0.26,          0.62,          0.69), 1.40f, 2.16f, 131.293 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Cs"), QStringLiteral("Cesium"), Color(         0.34,          0.09,          0.56), 2.44f, 2.10f, 132.90545 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ba"), QStringLiteral("Barium"), Color(         0.00,          0.79,          0.00), 2.15f, 2.00f, 137.327 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("La"), QStringLiteral("Lanthanum"), Color(         0.44,          0.83,          1.00), 2.07f, 2.00f, 138.9055 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ce"), QStringLiteral("Cerium"), Color(         1.00,          1.00,          0.78), 2.04f, 2.00f, 140.116 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Pr"), QStringLiteral("Praseodymium"), Color(         0.85,          1.00,          0.78), 2.03f, 2.00f, 140.90765 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Nd"), QStringLiteral("Neodymium"), Color(         0.78,          1.00,          0.78), 2.01f, 2.00f, 144.24 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Pm"), QStringLiteral("Promethium"), Color(         0.64,          1.00,          0.78), 1.99f, 2.00f, 145.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Sm"), QStringLiteral("Samarium"), Color(         0.56,          1.00,          0.78), 1.98f, 2.00f, 150.36 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Eu"), QStringLiteral("Europium"), Color(         0.38,          1.00,          0.78), 1.98f, 2.00f, 151.964 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Gd"), QStringLiteral("Gadolinium"), Color(         0.27,          1.00,          0.78), 1.96f, 2.00f, 157.25 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Tb"), QStringLiteral("Terbium"), Color(         0.19,          1.00,          0.78), 1.94f, 2.00f, 158.92534 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Dy"), QStringLiteral("Dysprosium"), Color(         0.12,          1.00,          0.78), 1.92f, 2.00f, 162.500 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ho"), QStringLiteral("Holmium"), Color(         0.00,          1.00,          0.61), 1.92f, 2.00f, 164.93032 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Er"), QStringLiteral("Erbium"), Color(         0.00,          0.90,          0.46), 1.89f, 2.00f, 167.259 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Tm"), QStringLiteral("Thulium"), Color(         0.00,          0.83,          0.32), 1.90f, 2.00f, 168.93421 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Yb"), QStringLiteral("Ytterbium"), Color(         0.00,          0.75,          0.22), 1.87f, 2.00f, 173.04 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Lu"), QStringLiteral("Lutetium"), Color(         0.00,          0.67,          0.14), 1.87f, 2.00f, 174.967 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Hf"), QStringLiteral("Hafnium"), Color(         0.30,          0.76,          1.00), 1.75f, 2.00f, 178.49 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ta"), QStringLiteral("Tantalum"), Color(         0.30,          0.65,          1.00), 1.70f, 2.00f, 180.9479 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("W"),  QStringLiteral("Tungsten"), Color(         0.13,          0.58,          0.84), 1.62f, 2.00f, 183.84 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Re"), QStringLiteral("Rhenium"), Color(         0.15,          0.49,          0.67), 1.51f, 2.00f, 186.207 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Os"), QStringLiteral("Osmium"), Color(         0.15,          0.40,          0.59), 1.44f, 2.00f, 190.23 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Ir"), QStringLiteral("Iridium"), Color(         0.09,          0.33,          0.53), 1.41f, 2.00f, 192.217 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Pt"), QStringLiteral("Platinum"), Color(         0.90,          0.85,          0.68), 1.39f, 1.72f, 195.078 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Au"), QStringLiteral("Gold"), Color(255.0f/255.0f, 209.0f/255.0f,  35.0f/255.0f), 1.44f, 1.66f, 196.96655 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Hg"), QStringLiteral("Mercury"), Color(         0.71,          0.71,          0.76), 1.32f, 1.55f, 200.59 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Tl"), QStringLiteral("Thallium"), Color(         0.65,          0.33,          0.30), 1.45f, 1.96f, 204.3833 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Pb"), QStringLiteral("Lead"), Color( 87.0f/255.0f,  89.0f/255.0f,  97.0f/255.0f), 1.47f, 2.02f, 207.2 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Bi"), QStringLiteral("Bismuth"), Color(158.0f/255.0f,  79.0f/255.0f, 181.0f/255.0f), 1.46f, 2.00f, 208.98038 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Po"), QStringLiteral("Polonium"), Color(         0.67,          0.36,          0.00), 1.40f, 2.00f, 209.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("At"), QStringLiteral("Astatine"), Color(         0.46,          0.31,          0.27), 1.50f, 2.00f, 210.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Rn"), QStringLiteral("Radon"), Color(         0.26,          0.51,          0.59), 1.50f, 2.00f, 222.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Fr"), QStringLiteral("Francium"), Color(         0.26,          0.00,          0.40), 2.60f, 2.00f, 223.0 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Ra"), QStringLiteral("Radium"), Color(         0.00,          0.49,          0.67), 2.43f, 2.00f, 226.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ac"), QStringLiteral("Actinium"), Color(         0.44,          0.67,          0.98), 2.15f, 2.00f, 227.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Th"), QStringLiteral("Thorium"), Color(         0.73,          0.78,          0.87), 2.06f, 2.00f, 232.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Pa"), QStringLiteral("Protactinium"), Color(         0.80,          0.88,          0.98), 2.00f, 2.00f, 231.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("U"),  QStringLiteral("Uranium"), Color(         0.12,          0.58,          0.83), 1.96f, 2.00f, 238.02891 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Np"), QStringLiteral("Neptunium"), Color(         0.83,          0.62,          0.92), 1.90f, 2.00f, 237.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Pu"), QStringLiteral("Plutonium"), Color(         0.82,          0.68,          0.78), 1.87f, 2.00f, 244.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Am"), QStringLiteral("Americium"), Color(         0.33,          0.36,          0.95), 1.80f, 2.00f, 243.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Cm"), QStringLiteral("Curium"), Color(         0.95,          0.30,          0.30), 1.69f, 2.00f, 247.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Bk"), QStringLiteral("Berkelium"), Color(         0.89,          0.67,          0.21), 1.54f, 2.00f, 247.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Cf"), QStringLiteral("Californium"), Color(         0.92,          0.20,          0.20), 1.86f, 2.00f, 251.0 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Es"), QStringLiteral("Einsteinium"), Color(         0.92,          0.31,          0.35), 1.86f, 2.00f, 252.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Fm"), QStringLiteral("Fermium"), Color(         0.90,          0.30,          0.30), 1.86f, 2.00f, 257.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Md"), QStringLiteral("Mendelevium"), Color(         0.82,          0.49,          0.20), 1.86f, 2.00f, 258.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("No"), QStringLiteral("Nobelium"), Color(         0.78,          0.50,          0.20), 1.86f, 2.00f, 259.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Lr"), QStringLiteral("Lawrencium"), Color(         0.76,          0.49,          0.41), 1.86f, 2.00f, 262.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Rf"), QStringLiteral("Rutherfordium"), Color(         0.80,          0.60,          0.20), 1.75f, 2.00f, 267.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Db"), QStringLiteral("Dubnium"), Color(         0.82,          0.51,          0.31), 1.69f, 2.00f, 268.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Sg"), QStringLiteral("Seaborgium"), Color(         0.85,          0.47,          0.31), 1.78f, 2.00f, 271.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Bh"), QStringLiteral("Bohrium"), Color(         0.88,          0.48,          0.20), 1.61f, 2.00f, 272.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Hs"), QStringLiteral("Hassium"), Color(         0.90,          0.30,          0.30), 1.52f, 2.00f, 277.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Mt"), QStringLiteral("Meitnerium"), Color(         0.92,          0.29,          0.20), 1.30f, 2.00f, 278.0 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("Ds"), QStringLiteral("Darmstadtium"), Color(         0.88,          0.27,          0.22), 1.30f, 2.00f, 281.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Rg"), QStringLiteral("Roentgenium"), Color(         0.82,          0.30,          0.32), 1.30f, 2.00f, 282.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Cn"), QStringLiteral("Copernicium"), Color(         0.75,          0.47,          0.47), 1.30f, 2.00f, 285.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Nh"), QStringLiteral("Nihonium"), Color(         0.70,          0.49,          0.51), 1.30f, 2.00f, 286.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Fl"), QStringLiteral("Flerovium"), Color(         0.64,          0.52,          0.58), 1.30f, 2.00f, 289.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Mc"), QStringLiteral("Moscovium"), Color(         0.60,          0.55,          0.67), 1.30f, 2.00f, 290.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Lv"), QStringLiteral("Livermorium"), Color(         0.53,          0.55,          0.67), 1.30f, 2.00f, 293.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Ts"), QStringLiteral("Tennessine"), Color(         0.46,          0.56,          0.67), 1.30f, 2.00f, 294.0 },
    ParticleType::PredefinedChemicalType{ QStringLiteral("Og"), QStringLiteral("Oganesson"), Color(         0.40,          0.40,          0.40), 1.30f, 2.00f, 294.0 },

    ParticleType::PredefinedChemicalType{ QStringLiteral("D"),  QStringLiteral("Deuterium"), Color(255.0f/255.0f, 255.0f/255.0f, 255.0f/255.0f), 0.46f, 1.20f, 2.01410177812 },
}};

// Define default names, colors, and radii for predefined structure types.
const std::array<ParticleType::PredefinedStructuralType, (size_t)ParticleType::PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES> ParticleType::_predefinedStructureTypes{{
    ParticleType::PredefinedStructuralType{ QStringLiteral("Other"), Color(0.95f, 0.95f, 0.95f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("FCC"), Color(0.4f, 1.0f, 0.4f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("HCP"), Color(1.0f, 0.4f, 0.4f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("BCC"), Color(0.4f, 0.4f, 1.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("ICO"), Color(0.95f, 0.8f, 0.2f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Cubic diamond"), Color(19.0f/255.0f, 160.0f/255.0f, 254.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Cubic diamond (1st neighbor)"), Color(0.0f/255.0f, 254.0f/255.0f, 245.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Cubic diamond (2nd neighbor)"), Color(126.0f/255.0f, 254.0f/255.0f, 181.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Hexagonal diamond"), Color(254.0f/255.0f, 137.0f/255.0f, 0.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Hexagonal diamond (1st neighbor)"), Color(254.0f/255.0f, 220.0f/255.0f, 0.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Hexagonal diamond (2nd neighbor)"), Color(204.0f/255.0f, 229.0f/255.0f, 81.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Simple cubic"), Color(160.0f/255.0f, 20.0f/255.0f, 254.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Graphene"), Color(160.0f/255.0f, 120.0f/255.0f, 254.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Hexagonal ice"), Color(0.0f, 0.9f, 0.9f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Cubic ice"), Color(1.0f, 193.0f/255.0f, 5.0f/255.0f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Interfacial ice"), Color(0.5f, 0.12f, 0.4f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Hydrate"), Color(1.0f, 0.3f, 0.1f) },
    ParticleType::PredefinedStructuralType{ QStringLiteral("Interfacial hydrate"), Color(0.1f, 1.0f, 0.1f) },
}};
// clang-format on

/******************************************************************************
* Returns the default radius for a particle type.
******************************************************************************/
FloatType ParticleType::getDefaultParticleRadius(Particles::Type typeClass, const QString& particleTypeName, int numericTypeId, bool loadUserDefaults, RadiusVariant radiusVariant)
{
    // Interactive execution context means that we are supposed to load the user-defined
    // settings from the settings store.
    if(loadUserDefaults && typeClass != Particles::UserProperty) {

#ifndef OVITO_DISABLE_QSETTINGS
        // Use the type's name, property type and container class to look up the
        // default radius saved by the user.
        const QString& settingsKey = ElementType::getElementSettingsKey(
            OwnerPropertyRef(&Particles::OOClass(), typeClass),
            (radiusVariant == RadiusVariant::DisplayRadius) ? QStringLiteral("radius") : QStringLiteral("vdw_radius"),
            particleTypeName);
        QVariant v = QSettings().value(settingsKey);
        if(v.isValid() && v.canConvert<FloatType>())
            return v.value<FloatType>();

        // The following is for backward compatibility with OVITO 3.3.5, which used to store the
        // default radii in a different branch of the settings registry.
        if(radiusVariant == RadiusVariant::DisplayRadius) {
            v = QSettings().value(QStringLiteral("particles/defaults/radius/%1/%2").arg(typeClass).arg(particleTypeName));
            if(v.isValid() && v.canConvert<FloatType>())
                return v.value<FloatType>();
        }
#endif
    }

    if(typeClass == Particles::TypeProperty) {
        for(const PredefinedChemicalType& predefType : _PredefinedChemicalTypes) {
            if(predefType.symbol == particleTypeName) {
                if(radiusVariant == RadiusVariant::DisplayRadius)
                    return predefType.displayRadius;
                else
                    return predefType.vdwRadius;
            }
        }

        // Sometimes atom type names have additional letters/numbers appended.
        if(particleTypeName.length() > 1 && particleTypeName.length() <= 5) {
            return getDefaultParticleRadius(typeClass, particleTypeName.left(particleTypeName.length() - 1), numericTypeId, loadUserDefaults, radiusVariant);
        }
    }

    return 0;
}

/******************************************************************************
* Changes the default radius for a particle type.
******************************************************************************/
void ParticleType::setDefaultParticleRadius(Particles::Type typeClass, const QString& particleTypeName, FloatType radius, RadiusVariant radiusVariant)
{
    if(typeClass == Particles::UserProperty)
        return;

#ifndef OVITO_DISABLE_QSETTINGS
    QSettings settings;
    const QString& settingsKey = ElementType::getElementSettingsKey(
        OwnerPropertyRef(&Particles::OOClass(), typeClass),
        (radiusVariant == RadiusVariant::DisplayRadius) ? QStringLiteral("radius") : QStringLiteral("vdw_radius"),
        particleTypeName);

    if(std::abs(getDefaultParticleRadius(typeClass, particleTypeName, 0, false, radiusVariant) - radius) > 1e-6)
        settings.setValue(settingsKey, QVariant::fromValue(radius));
    else
        settings.remove(settingsKey);
#endif
}

/******************************************************************************
* Returns the default mass for a particle type.
******************************************************************************/
FloatType ParticleType::getDefaultParticleMass(Particles::Type typeClass, const QString& particleTypeName, int numericTypeId, bool loadUserDefaults)
{
    if(typeClass == Particles::TypeProperty) {
        for(const PredefinedChemicalType& predefType : _PredefinedChemicalTypes) {
            if(predefType.symbol == particleTypeName) {
                return predefType.mass;
            }
        }

        // Sometimes atom type names have additional letters/numbers appended.
        if(particleTypeName.length() > 1 && particleTypeName.length() <= 5) {
            return getDefaultParticleMass(typeClass, particleTypeName.left(particleTypeName.length() - 1), numericTypeId, loadUserDefaults);
        }
    }

    return 0;
}

/******************************************************************************
* Determines the chemical element represented by a particle type (if any).
******************************************************************************/
ParticleType::ChemicalElement ParticleType::getDefaultChemicalElementForType(Particles::Type typeClass, const QString& particleTypeName, int numericTypeId, bool loadUserDefaults)
{
    if(typeClass == Particles::TypeProperty) {
        int index = 0;
        for(const PredefinedChemicalType& predefType : _PredefinedChemicalTypes) {
            if(predefType.symbol == particleTypeName) {
                return static_cast<ChemicalElement>(index);
            }
            index++;
        }

        // Sometimes atom type names have additional letters/numbers appended.
        if(particleTypeName.length() > 1 && particleTypeName.length() <= 5) {
            return getDefaultChemicalElementForType(typeClass, particleTypeName.left(particleTypeName.length() - 1), numericTypeId, loadUserDefaults);
        }
    }

    return ChemicalElement::X;
}

/******************************************************************************
* Performs a reverse lookup. Given a mass value, find the corresponding
* standard particle type name. Currently, this method only considers chemical
* elements from the hard-coded table, because mass presets cannot be configured by the user.
******************************************************************************/
QString ParticleType::guessTypeNameFromMass(FloatType mass)
{
    // Maximum allowed deviation from reference mass value:
    constexpr FloatType tolerance = 5e-3;

    for(const PredefinedChemicalType& predefType : _PredefinedChemicalTypes) {
        if(std::abs(predefType.mass - mass) <= tolerance) {
            return predefType.symbol;
        }
        if(predefType.alternativeMass != 0 && std::abs(predefType.alternativeMass - mass) <= tolerance) {
            return predefType.symbol;
        }
    }

    return {};
}

/******************************************************************************
* Returns a list of column names to be displayed in the data inspector for
* element types of this class.
******************************************************************************/
QStringList ParticleType::OOMetaClass::dataInspectorColumns() const
{
    QStringList columns = ElementTypeClass::dataInspectorColumns();
    columns << QStringLiteral("Radius") << QStringLiteral("Mass") << QStringLiteral("VdW Radius") << QStringLiteral("Element") << QStringLiteral("Shape");
    return columns;
}

/******************************************************************************
* Returns the Qt table model data for the given element type to be displayed in the data inspector.
******************************************************************************/
QVariant ParticleType::OOMetaClass::dataInspectorModelData(int columnIndex, const QString& columnName, const ElementType* elementType, int role) const
{
    if(role == Qt::DisplayRole) {
        if(const ParticleType* ptype = dynamic_object_cast<ParticleType>(elementType)) {
            if(columnName == QStringLiteral("Radius")) {
                if(ptype->radius() != 0)
                    return ptype->radius();
            }
            else if(columnName == QStringLiteral("Mass")) {
                if(ptype->mass() != 0)
                    return ptype->mass();
            }
            else if(columnName == QStringLiteral("VdW Radius")) {
                if(ptype->vdwRadius() != 0)
                    return ptype->vdwRadius();
            }
            else if(columnName == QStringLiteral("Element")) {
                if(ptype->chemicalElement() != ParticleType::ChemicalElement::X)
                    return QStringLiteral("%1 (%2)").arg(ParticleType::getChemicalElementFullName(ptype->chemicalElement())).arg(ParticleType::getChemicalElementSymbol(ptype->chemicalElement()));
            }
            else if(columnName == QStringLiteral("Shape")) {
                switch(ptype->shape()) {
                    case ParticlesVis::ParticleShape::Default: return {};
                    case ParticlesVis::ParticleShape::Sphere: return QStringLiteral("Sphere/Ellipsoid");
                    case ParticlesVis::ParticleShape::Circle: return QStringLiteral("Circle");
                    case ParticlesVis::ParticleShape::Box: return QStringLiteral("Cube/Box");
                    case ParticlesVis::ParticleShape::Square: return QStringLiteral("Square");
                    case ParticlesVis::ParticleShape::Cylinder: return QStringLiteral("Cylinder");
                    case ParticlesVis::ParticleShape::Spherocylinder: return QStringLiteral("Spherocylinder");
                    case ParticlesVis::ParticleShape::Mesh: return QStringLiteral("Mesh");
                    default:;  // Ignore
                }
            }
        }
    }
    return ElementTypeClass::dataInspectorModelData(columnIndex, columnName, elementType, role);
}

}   // End of namespace
