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
#include <ovito/core/dataset/pipeline/ActiveObject.h>
#include <ovito/core/dataset/animation/TimeInterval.h>

namespace Ovito {

/**
 * \brief Abstract base class for display objects that are responsible
 *        for rendering DataObject-derived classes.
 */
class OVITO_CORE_EXPORT DataVis : public ActiveObject
{
    OVITO_CLASS(DataVis)

public:

    /// Constructor.
    using ActiveObject::ActiveObject;

    /// \brief Lets the vis element produce a visual representation of a data object.
    ///
    /// \param path The data object to be rendered and its parent objects.
    /// \param flowState The pipeline evaluation results.
    /// \param frameGraph The output frame graph being generated.
    /// \param pipeline The pipeline scene node that produced the data object.
    /// \return A status code indicating the success or failure of the rendering operation.
    virtual PipelineStatus render(const ConstDataObjectPath& path, const PipelineFlowState& flowState, FrameGraph& frameGraph, const Pipeline* pipeline) = 0;

    /// \brief Computes the view-dependent bounding box of the given data object.
    virtual Box3 boundingBoxImmediate(AnimationTime time, const ConstDataObjectPath& path, const Pipeline* pipeline, const PipelineFlowState& flowState, TimeInterval& validityInterval) = 0;

    /// \brief Indicates whether this visual element should be surrounded by a selection marker in the viewports when it is selected.
    /// \return \c true to let the system render a selection marker around the object when it is selected.
    ///
    /// The default implementation returns \c true.
    virtual bool showSelectionMarker() { return true; }

    /// \brief Returns all pipelines that produced this visualization element.
    /// \param onlyScenePipelines If true, pipelines which are currently not part of the scene are ignored.
    QSet<Pipeline*> pipelines(bool onlyScenePipelines) const;

    /// Returns whether the DataVis class currently manages its error state and not the scene renderer.
    bool manualErrorStateControl() const { return _manualErrorStateControl; }

    /// Sets whether the DataVis class currently manages its error state and not the scene renderer.
    void setManualErrorStateControl(bool enable) { _manualErrorStateControl = enable; }

private:

    /// Indicates that the DataVis class manages its error state and not the scene renderer.
    /// This flag is used by TransformingDataVis class to preserve an error state generated during
    /// the transformation process. Otherwise, the scene renderer would automatically reset the
    /// error state during rendering.
    bool _manualErrorStateControl = false;
};

}   // End of namespace
