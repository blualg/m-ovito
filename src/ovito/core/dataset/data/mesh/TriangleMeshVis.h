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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering TriangleMesh data objects.
 */
class OVITO_CORE_EXPORT TriangleMeshVis : public DataVis
{
    OVITO_CLASS(TriangleMeshVis)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Lets the vis element produce a visual representation of a data object.
    virtual PipelineStatus render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) override;

    /// Computes the bounding box of the object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) override;

    /// Returns the transparency parameter.
    FloatType transparency() const { return transparencyController()->getFloatValue(AnimationTime(0)); }

    /// Sets the transparency parameter.
    void setTransparency(FloatType t) { transparencyController()->setFloatValue(AnimationTime(0), t); }

private:

    /// Controls the display color of the mesh.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS((Color{0.85, 0.85, 1}), color, setColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the transparency of the mesh.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, transparencyController, setTransparencyController);

    /// Controls whether the polygonal edges of the mesh should be highlighted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, highlightEdges, setHighlightEdges);

    /// Controls whether triangles facing away from the viewer are not rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, backfaceCulling, setBackfaceCulling);
};

}   // End of namespace
