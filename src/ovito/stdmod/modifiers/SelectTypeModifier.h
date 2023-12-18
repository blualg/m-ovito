////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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


#include <ovito/stdmod/StdMod.h>
#include <ovito/stdobj/properties/GenericPropertyModifier.h>
#include <ovito/stdobj/properties/PropertyReference.h>

namespace Ovito {

/**
 * \brief Selects data elements of one or more types.
 */
class OVITO_STDMOD_EXPORT SelectTypeModifier : public GenericPropertyModifier
{
    OVITO_CLASS(SelectTypeModifier)

    OVITO_CLASSINFO("DisplayName", "Select type");
    OVITO_CLASSINFO("Description", "Select particles based on chemical species, or bonds based on bond type.");
    OVITO_CLASSINFO("ModifierCategory", "Selection");

public:

    /// Constructor.
    explicit SelectTypeModifier(ObjectInitializationFlags flags);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;

    /// Returns a short piece information (typically a string or color) to be displayed next to the modifier's title in the pipeline editor list.
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// The input type property that is used as data source for the selection.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference, sourceProperty, setSourceProperty);

    /// The numeric IDs of the types to select.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QSet<int32_t>, selectedTypeIDs, setSelectedTypeIDs);

    /// The names of the types to select.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QSet<QString>, selectedTypeNames, setSelectedTypeNames);
};

}   // End of namespace
