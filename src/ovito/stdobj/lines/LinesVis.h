////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyColorMapping.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/rendering/FrameGraph.h>
#include "Lines.h"

namespace Ovito {

/**
 * \brief This information record is attached to a lines segment by theLiensVis when rendering
 * them in the viewports. It facilitates the picking of dislocations with the mouse.
 */
class OVITO_STDOBJ_EXPORT LinesPickInfo : public ObjectPickInfo
{
    OVITO_CLASS(LinesPickInfo)

public:

    /// Constructor.
    void initializeObject(const Lines* linesObj, std::vector<int>&& subobjToSegmentMap) {
        ObjectPickInfo::initializeObject();
        _linesObj = linesObj;
        _subobjToSegmentMap = std::move(subobjToSegmentMap);
    }

    /// The data object containing the lines.
    const Lines* linesObj() const { return _linesObj; }

    /// Given an sub-object ID returned by the Viewport::pick() method, looks up the corresponding lines segment.
    int segmentIndexFromSubObjectID(uint32_t subobjID) const {
        if(subobjID < _subobjToSegmentMap.size())
            return _subobjToSegmentMap[subobjID];
        else
            return -1;
    }

    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(const Pipeline* pipeline, uint32_t subobjectId) override;

private:
    /// The data object containing the line segments.
    DataOORef<const Lines> _linesObj;

    /// This array is used to map sub-object picking IDs back to line segments.
    std::vector<int> _subobjToSegmentMap;
};

/**
 * \brief A visualization element for rendering lines.
 */
class OVITO_STDOBJ_EXPORT LinesVis : public DataVis
{
    OVITO_CLASS(LinesVis)

public:
    /// The shading modes supported by the lines vis element.
    enum ShadingMode
    {
        NormalShading = CylinderPrimitive::ShadingMode::NormalShading,
        FlatShading = CylinderPrimitive::ShadingMode::FlatShading
    };
    Q_ENUM(ShadingMode);

    /// The coloring modes supported by the lines vis element.
    enum ColoringMode
    {
        UniformColoring,
        PseudoColoring,
    };
    Q_ENUM(ColoringMode);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Renders the associated data object.
    virtual std::variant<PipelineStatus, Future<PipelineStatus>> render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const SceneNode* sceneNode) override;

    /// Computes the bounding box of the data object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Replaces this visual element with a shared visual element by telling all dependents to update their references.
    virtual void replaceWithSharedElement(DataVis* sharedVis) const override;

public:
    Q_PROPERTY(Ovito::LinesVis::ShadingMode shadingMode READ shadingMode WRITE setShadingMode)

protected:
    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:
    /// Controls the display width of the lines.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.2}, lineWidth, setLineWidth, PROPERTY_FIELD_MEMORIZE);

    /// Controls the color of the lines.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.6, 0.6, 0.6}), lineColor, setLineColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the lines are rendered only up to the current animation time.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, showUpToCurrentTime, setShowUpToCurrentTime);

    /// Controls the whether the displayed lines are wrapped at periodic boundaries of the simulation cell.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, wrappedLines, setWrappedLines, PROPERTY_FIELD_MEMORIZE);

    /// Controls the whether the displayed lines are wrapped at periodic boundaries of the simulation cell.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, roundedCaps, setRoundedCaps, PROPERTY_FIELD_MEMORIZE);

    /// Controls the shading mode for lines.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(LinesVis::ShadingMode{FlatShading}, shadingMode, setShadingMode, PROPERTY_FIELD_MEMORIZE);

    /// Controls how the lines are being colored.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(LinesVis::ColoringMode{UniformColoring}, coloringMode, setColoringMode);

    /// Transfer function for pseudo-color visualization of a trajectory line property.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<PropertyColorMapping>, colorMapping, setColorMapping);
};

}  // namespace Ovito
