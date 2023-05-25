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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/animation/controller/Controller.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

/**
 * \brief A visualization element for rendering TriMeshObject data objects.
 */
class OVITO_CORE_EXPORT TriMeshVis : public DataVis
{
    OVITO_CLASS(TriMeshVis)
    Q_CLASSINFO("DisplayName", "Triangle mesh");

public:

    /// \brief Constructor.
    Q_INVOKABLE TriMeshVis(ObjectInitializationFlags flags);

    /// \brief Lets the vis element render a data object.
    virtual PipelineStatus render(AnimationTime time, const ConstDataObjectPath& path, const PipelineFlowState& flowState, SceneRenderer* renderer, const PipelineSceneNode* contextNode) override;

    /// \brief Computes the bounding box of the object.
    virtual Box3 boundingBox(AnimationTime time, const ConstDataObjectPath& path, const PipelineSceneNode* contextNode, const PipelineFlowState& flowState, MixedKeyCache& visCache, TimeInterval& validityInterval) override;

    /// Returns the transparency parameter.
    FloatType transparency() const { return transparencyController()->getFloatValue(AnimationTime(0)); }

    /// Sets the transparency parameter.
    void setTransparency(FloatType t) { transparencyController()->setFloatValue(AnimationTime(0), t); }

private:

    /// Controls the display color of the mesh.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(Color, color, setColor, PROPERTY_FIELD_MEMORIZE);

    /// Controls the transparency of the mesh.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<Controller>, transparencyController, setTransparencyController);

    /// Controls whether the polygonal edges of the mesh should be highlighted.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, highlightEdges, setHighlightEdges);

    /// Controls whether triangles facing away from the viewer are not rendered.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, backfaceCulling, setBackfaceCulling);
};

}   // End of namespace
