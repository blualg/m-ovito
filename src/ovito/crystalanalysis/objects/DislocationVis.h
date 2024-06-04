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

#pragma once


#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include <ovito/crystalanalysis/objects/DislocationNetworkObject.h>
#include <ovito/crystalanalysis/objects/RenderableDislocationLines.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief This information record is attached to the dislocation segments by the DislocationVis when rendering
 * them in the viewports. It facilitates the picking of dislocations with the mouse.
 */
class OVITO_CRYSTALANALYSIS_EXPORT DislocationPickInfo : public ObjectPickInfo
{
    OVITO_CLASS(DislocationPickInfo)

public:

    /// Constructor.
    void initializeObject(DislocationVis* visElement, const DislocationNetworkObject* dislocationObj, std::vector<int>&& subobjToSegmentMap) {
        ObjectPickInfo::initializeObject();
        _visElement = visElement;
        _dislocationObj = dislocationObj;
        _subobjToSegmentMap = std::move(subobjToSegmentMap);
    }

    /// The data object containing the dislocations.
    const DislocationNetworkObject* dislocationObj() const { return _dislocationObj; }

    /// Returns the vis element that rendered the dislocations.
    DislocationVis* visElement() const { return _visElement; }

    /// \brief Given an sub-object ID returned by the Viewport::pick() method, looks up the
    /// corresponding dislocation segment.
    int segmentIndexFromSubObjectID(quint32 subobjID) const {
        if(subobjID < _subobjToSegmentMap.size())
            return _subobjToSegmentMap[subobjID];
        else
            return -1;
    }

    /// Returns a human-readable string describing the picked object, which will be displayed in the status bar by OVITO.
    virtual QString infoString(Pipeline* pipeline, quint32 subobjectId) override;

private:

    /// The data object containing the dislocations.
    OORef<DislocationNetworkObject> _dislocationObj;

    /// The vis element that rendered the dislocations.
    OORef<DislocationVis> _visElement;

    /// This array is used to map sub-object picking IDs back to dislocation segments.
    std::vector<int> _subobjToSegmentMap;
};

/**
 * \brief A visualization element rendering dislocation lines.
 */
class OVITO_CRYSTALANALYSIS_EXPORT DislocationVis : public DataVis
{
    OVITO_CLASS(DislocationVis)

public:

    enum LineColoringMode {
        ColorByDislocationType,
        ColorByBurgersVector,
        ColorByCharacter
    };
    Q_ENUM(LineColoringMode);

public:

    /// Transforms the DislocationNetwork into a renderable set of lines.
    Future<std::shared_ptr<RenderableDislocationLines>> transformDislocations(const DislocationNetworkObject* dislocations);

    /// Lets the vis element render a data object.
    virtual PipelineStatus render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) override;

    /// Computes the bounding box of the object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Renders an overlay marker for a single dislocation segment.
    void renderOverlayMarker(const DataObject* dataObject, const PipelineFlowState& flowState, int segmentIndex, FrameGraph& frameGraph, const Pipeline* pipeline);

    /// Generates a pretty string representation of a Burgers vector.
    static QString formatBurgersVector(const Vector3& b, const MicrostructurePhase* structure);

public:

    Q_PROPERTY(Ovito::CylinderPrimitive::ShadingMode shadingMode READ shadingMode WRITE setShadingMode)

protected:

    /// Clips a dislocation line at the periodic box boundaries.
    static void clipDislocationLine(const std::deque<Point3>& line, const SimulationCell& simulationCell, const QVector<Plane3>& clippingPlanes, const std::function<void(const Point3&, const Point3&, bool)>& segmentCallback);

protected:

    /// The rendering width for dislocation lines.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1.0}, lineWidth, setLineWidth, PROPERTY_FIELD_MEMORIZE);

    /// The shading mode for dislocation lines.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(CylinderPrimitive::ShadingMode{CylinderPrimitive::NormalShading}, shadingMode, setShadingMode, PROPERTY_FIELD_MEMORIZE);

    /// The rendering width for Burgers vectors.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0.6}, burgersVectorWidth, setBurgersVectorWidth, PROPERTY_FIELD_MEMORIZE);

    /// The scaling factor Burgers vectors.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{3.0}, burgersVectorScaling, setBurgersVectorScaling, PROPERTY_FIELD_MEMORIZE);

    /// Display color for Burgers vectors.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.7, 0.7, 0.7}), burgersVectorColor, setBurgersVectorColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the display of Burgers vectors.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, showBurgersVectors, setShowBurgersVectors);

    /// Controls the display of the line directions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, showLineDirections, setShowLineDirections);

    /// Controls how the display color of dislocation lines is chosen.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(LineColoringMode{ColorByDislocationType}, lineColoringMode, setLineColoringMode);
};

}   // End of namespace
