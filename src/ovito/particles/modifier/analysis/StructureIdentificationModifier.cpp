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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/app/Application.h>
#include "StructureIdentificationModifier.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(StructureIdentificationModifier);
DEFINE_VECTOR_REFERENCE_FIELD(StructureIdentificationModifier, structureTypes);
DEFINE_PROPERTY_FIELD(StructureIdentificationModifier, onlySelectedParticles);
DEFINE_PROPERTY_FIELD(StructureIdentificationModifier, colorByType);
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, structureTypes, "Structure types");
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, onlySelectedParticles, "Use only selected particles");
SET_PROPERTY_FIELD_LABEL(StructureIdentificationModifier, colorByType, "Color particles by type");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
StructureIdentificationModifier::StructureIdentificationModifier(ObjectInitializationFlags flags) : AsynchronousModifier(flags),
    _onlySelectedParticles(false),
    _colorByType(true)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool StructureIdentificationModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
* Create an instance of the ParticleType class to represent a structure type.
******************************************************************************/
ElementType* StructureIdentificationModifier::createStructureType(int id, ParticleType::PredefinedStructureType predefType)
{
    DataOORef<ElementType> stype = DataOORef<ElementType>::create();
    stype->setNumericId(id);
    stype->setName(ParticleType::getPredefinedStructureTypeName(predefType));
    stype->initializeType(ParticlePropertyReference(Particles::StructureTypeProperty));
    addStructureType(stype);
    return stype;
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void StructureIdentificationModifier::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
    AsynchronousModifier::saveToStream(stream, excludeRecomputableData);
    stream.beginChunk(0x02);
    // For future use.
    stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void StructureIdentificationModifier::loadFromStream(ObjectLoadStream& stream)
{
    AsynchronousModifier::loadFromStream(stream);
    stream.expectChunkRange(0, 2);
    // For future use.
    stream.closeChunk();
}

/******************************************************************************
* Compute engine constructor.
******************************************************************************/
StructureIdentificationModifier::StructureIdentificationEngine::StructureIdentificationEngine(const ModifierEvaluationRequest& request, ParticleOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCell* simCell, const OORefVector<ElementType>& structureTypes, ConstPropertyPtr selection) :
    Engine(request),
    _positions(std::move(positions)),
    _simCell(simCell),
    _selection(std::move(selection)),
    _structures(Particles::OOClass().createStandardProperty(DataBuffer::Uninitialized, fingerprint.particleCount(), Particles::StructureTypeProperty)),
    _inputFingerprint(std::move(fingerprint))
{
    // Create deep copies of the structure elements types, because data objects owned by the modifier should
    // not be passed to the data pipeline.
    for(const ElementType* type : structureTypes) {
        OVITO_ASSERT(type && type->numericId() == _structures->elementTypes().size());

        // Attach structure types to output particle property.
        _structures->addElementType(DataOORef<ElementType>::makeDeepCopy(type));
    }
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void StructureIdentificationModifier::StructureIdentificationEngine::applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state)
{
    StructureIdentificationModifier* modifier = static_object_cast<StructureIdentificationModifier>(request.modifier());
    OVITO_ASSERT(modifier);

    Particles* particles = state.expectMutableObject<Particles>();
    particles->verifyIntegrity();

    if(_inputFingerprint.hasChanged(particles))
        throw Exception(tr("Cached modifier results are obsolete, because the number or the storage order of input particles has changed."));

    // Finalize output property.
    PropertyPtr structureProperty = postProcessStructureTypes(request, structures());
    BufferReadAccess<int32_t> structureData(structureProperty);

    // Add output property to the particles.
    particles->createProperty(structureProperty);

    if(modifier->colorByType()) {

        // Build structure type-to-color map.
        std::vector<ColorG> structureTypeColors(modifier->structureTypes().size());
        for(ElementType* stype : modifier->structureTypes()) {
            OVITO_ASSERT(stype->numericId() >= 0);
            if(stype->numericId() >= (int)structureTypeColors.size()) {
                structureTypeColors.resize(stype->numericId() + 1);
            }
            structureTypeColors[stype->numericId()] = stype->color().toDataType<GraphicsFloatType>();
        }

        // Assign colors to particles based on their structure type.
        BufferWriteAccess<ColorG, access_mode::discard_write> colorProperty = particles->createProperty(Particles::ColorProperty);
        boost::transform(structureData, colorProperty.begin(), [&](int s) {
            if(s >= 0 && s < structureTypeColors.size())
                return structureTypeColors[s];
            else
                return ColorG(1,1,1);
        });
    }

    // Count the number of particles of each identified type.
    int maxTypeId = 0;
    for(ElementType* stype : modifier->structureTypes()) {
        OVITO_ASSERT(stype->numericId() >= 0);
        maxTypeId = std::max(maxTypeId, stype->numericId());
    }
    _typeCounts.resize(maxTypeId + 1);
    boost::fill(_typeCounts, 0);
    for(int t : structureData) {
        if(t >= 0 && t <= maxTypeId)
            _typeCounts[t]++;
    }

    // Create the property arrays for the bar chart.
    PropertyPtr typeCounts = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int64, 1, tr("Count"));
    boost::copy(_typeCounts, BufferWriteAccess<int64_t, access_mode::discard_write>(typeCounts).begin());
    PropertyPtr typeIds = DataTable::OOClass().createUserProperty(DataBuffer::Uninitialized, maxTypeId + 1, Property::Int32, 1, tr("Structure type"));
    boost::algorithm::iota_n(BufferWriteAccess<int32_t, access_mode::discard_write>(typeIds).begin(), 0, typeIds->size());

    // Use the structure types as labels for the output bar chart.
    for(const ElementType* type : structureProperty->elementTypes()) {
        if(type->enabled())
            typeIds->addElementType(type);
    }

    // Output a bar chart with the type counts.
    DataTable* table = state.createObject<DataTable>(QStringLiteral("structures"), request.modificationNode(), DataTable::BarChart, tr("Structure counts"), std::move(typeCounts), std::move(typeIds));
}

}   // End of namespace
