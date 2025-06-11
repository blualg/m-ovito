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


#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataBuffer.h>
#include "PseudoColorMapping.h"
#include "OpacityFunction.h"
#include "RenderingPrimitive.h"

namespace Ovito {

/**
 * \brief A volumetric field to be rendered by a scene renderer.
 */
class OVITO_CORE_EXPORT VolumePrimitive final : public RenderingPrimitive
{
    Q_GADGET

#ifndef OVITO_BUILD_MONOLITHIC
    // Give this exported c++ class a "key function" to work around dynamic_cast problems (observed on macOS platform).
    // This function is not actually used but ensures that the class' vtable ends up in the core module.
    // See also http://itanium-cxx-abi.github.io/cxx-abi/abi.html#vague-vtable
    virtual void __key_function() override;
#endif

public:

    /// Returns the mapping from pseudo-color values at the mesh vertices to RGB colors.
    const PseudoColorMapping& pseudoColorMapping() const { return _pseudoColorMapping; }

    /// Sets the mapping from pseudo-color values at the mesh vertices to RGB colors.
    void setPseudoColorMapping(const PseudoColorMapping& mapping) {
        _pseudoColorMapping = mapping;
    }

 	/// Computes the 3d bounding box of the primitive in local coordinate space.
	virtual Box3 computeBoundingBox(const RendererResourceCache::ResourceFrame& visCache) const override {
        OVITO_ASSERT(this);
        return Box3(Point3(0), Point3(1)).transformed(domain());
    }

    /// Returns the dimensions of the regular grid.
    const std::array<size_t, 3>& dimensions() const { return _dimensions; }

    /// Sets the dimensions of the regular grid.
    void setDimensions(const std::array<size_t, 3>& dimensions) { _dimensions = dimensions; }

    /// Returns the affine transformation matrix that defines the grid's outer shape.
    const AffineTransformation& domain() const { return _domain; }

    /// Sets the affine transformation matrix that defines the grid's outer shape.
    void setDomain(const AffineTransformation& domain) { _domain = domain; }

    /// Returns the field data.
    const ConstDataBufferPtr& fieldData() const { return _fieldData; }

    /// Returns the field data vector component.
    int fieldDataComponent() const { return _fieldDataComponent; }

    /// Sets the field data and the optional field data vector component.
    void setFieldData(ConstDataBufferPtr fieldData, int fieldDataComponent = 0) { _fieldData = std::move(fieldData); _fieldDataComponent = fieldDataComponent; }

    /// Returns the opacity function.
    const DataOORef<const OpacityFunction>& opacityFunction() const { return _opacityFunction; }

    /// Sets the opacity function.
    void setOpacityFunction(DataOORef<const OpacityFunction> opacityFunction) { _opacityFunction = std::move(opacityFunction); }

    /// Returns the distance after which a 'opacity' fraction of light traveling through the volume is absorbed.
    FloatType absorptionUnitDistance() const { return _absorptionUnitDistance; }

    /// Sets the distance after which a 'opacity' fraction of light traveling through the volume is absorbed.
    void setAbsorptionUnitDistance(FloatType distance) { _absorptionUnitDistance = distance; }

private:

    /// The dimensions of the regular grid, i.e, the number of vertices (data points) in each direction.
    std::array<size_t, 3> _dimensions{{0,0,0}};

    /// An affine transformation matrix that defines the grid's outer shape.
    AffineTransformation _domain = AffineTransformation::Identity();

    /// Field data.
    ConstDataBufferPtr _fieldData;

    /// Field data vector component.
    int _fieldDataComponent = 0;

    /// The mapping from field values to RGB colors.
    PseudoColorMapping _pseudoColorMapping;

    /// Opacity gradient.
    DataOORef<const OpacityFunction> _opacityFunction;

    /// The distance after which a 'opacity' fraction of light traveling through the volume is absorbed.
    FloatType _absorptionUnitDistance = 1;
};

}   // End of namespace
