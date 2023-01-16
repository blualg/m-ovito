////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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


#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/util/ParticleOrderingFingerprint.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include <ovito/core/dataset/pipeline/AsynchronousModifier.h>

namespace Ovito::Particles {

/**
 * \brief Base class for modifiers that assign a structure type to each particle.
 */
class OVITO_PARTICLES_EXPORT StructureIdentificationModifier : public AsynchronousModifier
{
    /// Give this modifier class its own metaclass.
    class OVITO_PARTICLES_EXPORT StructureIdentificationModifierClass : public AsynchronousModifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using AsynchronousModifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(StructureIdentificationModifier, StructureIdentificationModifierClass)

public:

    /// Computes the modifier's results.
    class OVITO_PARTICLES_EXPORT StructureIdentificationEngine : public Engine
    {
    public:

        /// Constructor.
        StructureIdentificationEngine(const ModifierEvaluationRequest& request, ParticleOrderingFingerprint fingerprint, ConstPropertyPtr positions, const SimulationCellObject* simCell, const OORefVector<ElementType>& structureTypes, ConstPropertyPtr selection = {});

        /// Injects the computed results into the data pipeline.
        virtual void applyResults(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

        /// This method is called by the system whenever a parameter of the modifier changes.
        /// The method can be overridden by subclasses to indicate to the caller whether the engine object should be 
        /// discarded (false) or may be kept in the cache, because the computation results are not affected by the changing parameter (true). 
        virtual bool modifierChanged(const PropertyFieldEvent& event) override {
            // Avoid a recomputation if the user toggles just the color-by-type option.
            if(event.field() == PROPERTY_FIELD(colorByType))
                return true;
            return Engine::modifierChanged(event);
        }
        
        /// Returns the property storage that contains the computed per-particle structure types.
        const PropertyPtr& structures() const { return _structures; }

        /// Returns the property storage that contains the input particle positions.
        const ConstPropertyPtr& positions() const { return _positions; }

        /// Returns the property storage that contains the particle selection (optional).
        const ConstPropertyPtr& selection() const { return _selection; }

        /// Returns the simulation cell data.
        const DataOORef<const SimulationCellObject>& cell() const { return _simCell; }

        /// Returns whether a given structural type is enabled for identification.
        bool typeIdentificationEnabled(int typeId) const {
            OVITO_ASSERT(typeId >= 0);
            if(typeId >= structures()->elementTypes().size()) return false;
            OVITO_ASSERT(structures()->elementTypes()[typeId]->numericId() == typeId);
            return structures()->elementTypes()[typeId]->enabled();
        }

        /// Returns the number of identified particles of the given structure type.
        qlonglong getTypeCount(int typeIndex) const {
            if(_typeCounts.size() > typeIndex) return _typeCounts[typeIndex];
            return 0;
        }

    protected:

        /// Releases data that is no longer needed.
        void releaseWorkingData() {
            _positions.reset();
            _selection.reset();
            _simCell.reset();
        }

        /// Gives subclasses the possibility to post-process per-particle structure types
        /// before they are output to the data pipeline.
        virtual PropertyPtr postProcessStructureTypes(const ModifierEvaluationRequest& request, const PropertyPtr& structures) {
            return structures;
        }

    private:

        ConstPropertyPtr _positions;
        ConstPropertyPtr _selection;
        DataOORef<const SimulationCellObject> _simCell;
        const PropertyPtr _structures;
        ParticleOrderingFingerprint _inputFingerprint;
        std::vector<qlonglong> _typeCounts;
    };

public:

    /// Constructor.
    StructureIdentificationModifier(ObjectCreationParams params);

    /// Returns an existing structure type managed by the modifier.
    ElementType* structureTypeById(int id) const {
        for(ElementType* type : structureTypes())
            if(type->numericId() == id) return type;
        return nullptr;
    }

#ifdef OVITO_QML_GUI
    /// This helper method is called by the QML GUI (StructureListParameter.qml) to extract the identification counts 
    /// from the cached pipeline output state after the modifier has been evaluated. 
    Q_INVOKABLE QVector<qlonglong> getStructureCountsFromModifierResults(ModifierApplication* modApp) const;
#endif

protected:

    /// Saves the class' contents to the given stream.
    virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

    /// Loads the class' contents from the given stream.
    virtual void loadFromStream(ObjectLoadStream& stream) override;

    /// Inserts a structure type into the list.
    void addStructureType(ElementType* type) { 
        // Make sure the numeric type ID is unique.
        OVITO_ASSERT(std::none_of(structureTypes().begin(), structureTypes().end(), [&](const ElementType* t) { return t->numericId() == type->numericId(); }));
        _structureTypes.push_back(this, PROPERTY_FIELD(structureTypes), type); 
    }

    /// Create an instance of the ParticleType class to represent a structure type.
    ElementType* createStructureType(int id, ParticleType::PredefinedStructureType predefType, ObjectCreationParams params);

private:

    /// Contains the list of structure types recognized by this analysis modifier.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD(OORef<ElementType>, structureTypes, setStructureTypes);

    /// Controls whether analysis should take into account only selected particles.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, onlySelectedParticles, setOnlySelectedParticles);

    /// Controls whether the modifier colors particles based on their type.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, colorByType, setColorByType);
};

}   // End of namespace
