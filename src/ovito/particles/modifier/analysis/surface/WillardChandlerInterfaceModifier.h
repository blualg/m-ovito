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
#include <ovito/mesh/surface/SurfaceMeshVis.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * Constructs a Willard-Chandler instantaneous interface and classifies particles
 * as liquid, interfacial, or vapor based on their distance from the interface.
 */
class OVITO_PARTICLES_EXPORT WillardChandlerInterfaceModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(WillardChandlerInterfaceModifier, OOMetaClass)

public:

    enum PhaseLabel : int {
        Vapor = -1,
        Interfacial = 0,
        Liquid = 1,
    };
    Q_ENUM(PhaseLabel);

    enum PlateNormalDirection : int {
        PositiveX,
        NegativeX,
        PositiveY,
        NegativeY,
        PositiveZ,
        NegativeZ,
    };
    Q_ENUM(PlateNormalDirection);

    enum PlateGapMode : int {
        GlobalPlateTop,
        LocalPlateTop,
    };
    Q_ENUM(PlateGapMode);

    enum PlateReferenceSource : int {
        PlateAtomsExpression,
        FixedCoordinate,
    };
    Q_ENUM(PlateReferenceSource);

    static constexpr QStringView SurfaceMeshObjectId = u"willard-chandler-interface";
    static constexpr QStringView DistancePropertyName = u"Willard-Chandler Distance";
    static constexpr QStringView PhasePropertyName = u"Willard-Chandler Phase";
    static constexpr QStringView PhaseCountsTableId = u"willard-chandler-phase-counts";
    static constexpr QStringView CavityTableId = u"willard-chandler-cavities";
    static constexpr QStringView FilledRegionTableId = u"willard-chandler-filled-regions";

    void initializeObject(ObjectInitializationFlags flags);
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }
    virtual void replaceVisualElement(DataVis* visElement, const std::function<OORef<DataVis>(const QString&)>& getReplacement) override;

private:

    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<SurfaceMeshVis>, surfaceMeshVis, setSurfaceMeshVis,
                                             PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_MEMORIZE |
                                             PROPERTY_FIELD_OPEN_SUBEDITOR);

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{2.4}, gaussianWidth, setGaussianWidth, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.002}, isoValue, setIsoValue, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{80}, gridResolution, setGridResolution, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.5}, interfacialThickness, setInterfacialThickness, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, onlySelectedParticles, setOnlySelectedParticles);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, selectInterfacialParticles, setSelectInterfacialParticles);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, selectVaporParticles, setSelectVaporParticles);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, extendSelection, setExtendSelection);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, selectionExpression, setSelectionExpression, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, correctDetachedClusters, setCorrectDetachedClusters);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(PlateNormalDirection{PositiveZ}, plateNormalDirection, setPlateNormalDirection, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(PlateReferenceSource{PlateAtomsExpression}, plateReferenceSource, setPlateReferenceSource, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(PlateGapMode{GlobalPlateTop}, plateGapMode, setPlateGapMode, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.0}, detachedClusterGapCutoff, setDetachedClusterGapCutoff, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.0}, detachedClusterBottomPercentile, setDetachedClusterBottomPercentile, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{100.0}, plateTopPercentile, setPlateTopPercentile, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{1}, detachedClusterMinimumSupportAtoms, setDetachedClusterMinimumSupportAtoms, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.0}, plateTopCoordinate, setPlateTopCoordinate, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(QString{}, plateSelectionExpression, setPlateSelectionExpression, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
