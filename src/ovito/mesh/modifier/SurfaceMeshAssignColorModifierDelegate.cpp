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

#include <ovito/mesh/Mesh.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include "SurfaceMeshAssignColorModifierDelegate.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshVerticesAssignColorModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshVerticesAssignColorModifierDelegate, "DisplayName", "Mesh Vertices");
IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshFacesAssignColorModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshFacesAssignColorModifierDelegate, "DisplayName", "Mesh Faces");
IMPLEMENT_CREATABLE_OVITO_CLASS(SurfaceMeshRegionsAssignColorModifierDelegate);
OVITO_CLASSINFO(SurfaceMeshRegionsAssignColorModifierDelegate, "DisplayName", "Mesh Regions");

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshVerticesAssignColorModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh vertices in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshVertices::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshFacesAssignColorModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh faces in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshFaces::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

/******************************************************************************
* Indicates which data objects in the given input data collection the modifier
* delegate is able to operate on.
******************************************************************************/
QVector<DataObjectReference> SurfaceMeshRegionsAssignColorModifierDelegate::OOMetaClass::getApplicableObjects(const DataCollection& input) const
{
    // Gather list of all surface mesh regions in the input data collection.
    QVector<DataObjectReference> objects;
    for(const ConstDataObjectPath& path : input.getObjectsRecursive(SurfaceMeshRegions::OOClass())) {
        objects.push_back(path);
    }
    return objects;
}

}   // End of namespace
