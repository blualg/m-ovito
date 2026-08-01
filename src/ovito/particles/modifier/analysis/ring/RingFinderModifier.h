////////////////////////////////////////////////////////////////////////////////////////
//
//  Ring Finder modifier for m-ovito.
//
//  This native implementation is based on the ovito-org/RingFinder extension
//  (GPL-3.0-only). This file is distributed under the GNU General Public
//  License version 3 only.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/particles/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

namespace Ovito {

/**
 * \brief Finds shortest rings formed by the current bond topology.
 */
class OVITO_PARTICLES_EXPORT RingFinderModifier : public Modifier
{
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:
        using Modifier::OOMetaClass::OOMetaClass;
        [[nodiscard]] virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(RingFinderModifier, OOMetaClass)

public:

    static constexpr QStringView HistogramTableIdentifier = u"ring-size-histogram";
    static constexpr QStringView MeshIdentifier = u"rings";

    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;
    virtual QVariant getPipelineEditorShortInfo(Scene* scene, ModificationNode* node) const override;

private:

    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{3}, minimumRingSize, setMinimumRingSize, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{9}, maximumRingSize, setMaximumRingSize, PROPERTY_FIELD_MEMORIZE);
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, createPolygons, setCreatePolygons, PROPERTY_FIELD_MEMORIZE);
};

}  // namespace Ovito
