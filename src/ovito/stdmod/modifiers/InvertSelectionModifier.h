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

namespace Ovito {

/**
 * \brief This modifier inverts the selection status of each element.
 */
class OVITO_STDMOD_EXPORT InvertSelectionModifier : public GenericPropertyModifier
{
    OVITO_CLASS(InvertSelectionModifier)
    OVITO_CLASSINFO("DisplayName", "Invert selection");
    OVITO_CLASSINFO("Description", "Invert the current selection state of each element.");
    OVITO_CLASSINFO("ModifierCategory", "Selection");

public:

    /// Constructor.
    explicit InvertSelectionModifier(ObjectInitializationFlags flags);

    /// Modifies the input data synchronously.
    virtual void evaluateSynchronous(const ModifierEvaluationRequest& request, PipelineFlowState& state) override;
};

}   // End of namespace
