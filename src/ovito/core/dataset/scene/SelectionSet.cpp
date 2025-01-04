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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/scene/SelectionSet.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SelectionSet);
DEFINE_VECTOR_REFERENCE_FIELD(SelectionSet, nodes);
SET_PROPERTY_FIELD_LABEL(SelectionSet, nodes, "Nodes");

/******************************************************************************
* Adds a scene node to this selection set.
******************************************************************************/
void SelectionSet::push_back(OORef<SceneNode> node)
{
    OVITO_CHECK_OBJECT_POINTER(node);
    if(nodes().contains(node))
        throw Exception(tr("Node is already in the selection set."));

    // Insert into children array.
    _nodes.push_back(this, PROPERTY_FIELD(nodes), std::move(node));
}

/******************************************************************************
* Inserts a scene node into this selection set.
******************************************************************************/
void SelectionSet::insert(qsizetype index, OORef<SceneNode> node)
{
    OVITO_CHECK_OBJECT_POINTER(node);
    if(nodes().contains(node))
        throw Exception(tr("Node is already in the selection set."));

    // Insert into children array.
    _nodes.insert(this, PROPERTY_FIELD(nodes), index, std::move(node));
}

/******************************************************************************
* Removes a scene node from this selection set.
******************************************************************************/
void SelectionSet::remove(const SceneNode* node)
{
    int index = _nodes.indexOf(node);
    if(index == -1) return;
    removeByIndex(index);
    OVITO_ASSERT(!nodes().contains(node));
}

/******************************************************************************
* Provides a custom function that takes are of the deserialization of a
* serialized property field that has been removed from the class.
* This is needed for file backward compatibility with OVITO 3.11.
******************************************************************************/
RefMakerClass::SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr SelectionSet::OOMetaClass::overrideFieldDeserialization(LoadStream& stream, const SerializedClassInfo::PropertyFieldInfo& field) const
{
    // For backward compatibility with OVITO 3.11:
    // The Pipeline class has been split from the SceneNode base class in OVITO 3.12. This means we have to handle
    // the deserialization of the nodes field here, which used to be a list of SceneNode or Pipeline objects (now only SceneNode instances).
    if(field.definingClass == &SelectionSet::OOClass() && stream.formatVersion() < 30013) {
        if(field.identifier == "nodes") {
            return [](const SerializedClassInfo::PropertyFieldInfo& field, ObjectLoadStream& stream, RefMaker& owner) {
                stream.expectChunk(0x02);
                qint32 numNodes;
                stream >> numNodes;
                for(qint32 i = 0; i < numNodes; i++) {
                    OORef<RefTarget> node = stream.loadObject<RefTarget>();
                    if(OORef<Pipeline> pipeline = dynamic_object_cast<Pipeline>(node))
                        node = pipeline->deserializationSceneNode();
                    static_object_cast<SelectionSet>(&owner)->_nodes.insert(&owner, PROPERTY_FIELD(nodes), i, static_object_cast<SceneNode>(std::move(node)));
                }
                stream.closeChunk();
            };
        }
    }
    return RefTarget::OOMetaClass::overrideFieldDeserialization(stream, field);
}

}   // End of namespace
