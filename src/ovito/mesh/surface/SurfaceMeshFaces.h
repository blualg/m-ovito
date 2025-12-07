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


#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief Stores all face-related properties of a SurfaceMesh.
 */
class OVITO_MESH_EXPORT SurfaceMeshFaces : public PropertyContainer
{
    /// Define a new property metaclass for this container type.
    class OOMetaClass : public PropertyContainerClass
    {
    public:

        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Create a storage object for standard face properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

        /// Generates a human-readable string representation of the data object reference.
        virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const override;

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(SurfaceMeshFaces, OOMetaClass);

public:

    /// \brief The list of standard face properties.
    enum Type {
        UserProperty = Property::GenericUserProperty, //< This is reserved for user-defined properties.
        SelectionProperty = Property::GenericSelectionProperty,
        ColorProperty = Property::GenericColorProperty,
        FaceTypeProperty = Property::GenericTypeProperty,
        RegionProperty = Property::FirstSpecificProperty,
        BurgersVectorProperty,
        CrystallographicNormalProperty,
    };

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        PropertyContainer::initializeObject(flags);
        // Assign the default data object identifier.
        setIdentifier(OOClass().pythonName());
    }

    /// Returns the data for visualizing a vector property from this container using a VectorVis element.
    virtual VectorVis::VectorData getVectorVisData(const ConstDataObjectPath& path, const PipelineFlowState& state,
                                                   const RendererResourceCache::ResourceFrame& visCache) const override;

    /// Override method to prevent a direct deletion of elements from this container as it would leave the SurfaceMesh in an inconsistent state.
    virtual size_t deleteElements(ConstDataBufferPtr selection, size_t selectionCount = std::numeric_limits<size_t>::max()) override {
        OVITO_ASSERT(false);
        throw Exception(tr("Deleting faces from a SurfaceMesh is not supported via this method. Call SurfaceMesh.delete_faces() on the parent object instead."));
    }

    /// Throws an exception if appending is not supported by this container type.
    /// This is used in the PropertyContainer.append() Python method.
    virtual void checkAppendability() const override { throw Exception(tr("Mesh face containers cannot be appended to. You should use the SurfaceMesh.create_face() method instead.")); }
};

}   // End of namespace
