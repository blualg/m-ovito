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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/core/utilities/linalg/LinAlg.h>

namespace Ovito {

/**
 * \brief Stores a set of (poly)lines.
 */
class OVITO_STDOBJ_EXPORT Lines : public PropertyContainer
{
public:
    /// Define a new property metaclass for this property container type.
    class OVITO_STDOBJ_EXPORT OOMetaClass : public PropertyContainerClass
    {
    public:
        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Creates a storage object for standard properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type,
                                                           const ConstDataObjectPath& containerPath) const override;

    protected:
        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(Lines, OOMetaClass);

public:
    /// \brief The list of standard properties.
    enum Type
    {
        ColorProperty = Property::GenericColorProperty,
        SelectionProperty = Property::GenericSelectionProperty,
        PositionProperty = Property::FirstSpecificProperty,
        SampleTimeProperty,  // Is used by the GenerateTrajectoryLinesModifier
        SectionProperty,
        Position1Property,
        Position2Property,
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags);

    /// Returns the data for visualizing a vector property from this container using a VectorVis element.
    VectorVis::VectorData getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                           const RendererResourceCache::ResourceFrame& visCache) const override;

private:

    /// Tests whether the given spatial point is culled by the cutting planes set for this object.
    bool isPointCulled(const Point3& p) const {
        return std::any_of(cuttingPlanes().begin(), cuttingPlanes().end(), [&](const Plane3& plane) { return plane.classifyPoint(p) > 0; });
    }

    /// The planar cuts to be applied to geometry after its has been transformed into a non-periodic representation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QVector<Plane3>{}, cuttingPlanes, setCuttingPlanes);

    /// The cached bounding box of the vertex coordinates.
    Box3 _boundingBox;
};

}  // namespace Ovito
