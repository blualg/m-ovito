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

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>

namespace Ovito {

OVITO_PARTICLES_EXPORT std::vector<int> parseParticleTypeIds(const QString& typeListText,
                                                             const Property* typeProperty,
                                                             const QString& roleDescription,
                                                             const QString& contextDescription);

OVITO_PARTICLES_EXPORT QString canonicalizeTypeList(QString typeListText);

OVITO_PARTICLES_EXPORT QString canonicalizeParticleSelector(const QString& typeListText,
                                                            const QString& expressionText);

OVITO_PARTICLES_EXPORT std::vector<uint8_t> evaluateParticleSelector(const PipelineFlowState& state,
                                                                     const Particles* particles,
                                                                     const Property* typeProperty,
                                                                     const BufferReadAccess<int32_t>& particleTypes,
                                                                     const QString& typeListText,
                                                                     const QString& expressionText,
                                                                     const QString& roleDescription,
                                                                     const QString& contextDescription,
                                                                     size_t* matchCount = nullptr);

OVITO_PARTICLES_EXPORT PropertyPtr createSelectionPropertyFromMask(const std::vector<uint8_t>& mask);

}   // End of namespace Ovito
