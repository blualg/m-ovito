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
#include <ovito/core/oo/RefTarget.h>
#include "SceneNode.h"

namespace Ovito {

/**
 * \brief Stores a selection of scene nodes.
 *
 * This selection set class holds a reference list to all SceneNode objects
 * that are selected.
 *
 * The current selection set can be accessed via the DataSetManager::currentSelection() method.
 */
class OVITO_CORE_EXPORT SelectionSet : public RefTarget
{
    OVITO_CLASS(SelectionSet)

public:

    /// \brief Creates an empty selection set.
    using RefTarget::RefTarget;

    /// \brief Adds a scene node to this selection set.
    /// \param node The node to be added.
    /// \undoable
    void push_back(SceneNode* node);

    /// \brief Inserts a scene node into this selection set.
    /// \param index The index at which to insert the node into the list.
    /// \param node The node to be inserted.
    /// \undoable
    void insert(qsizetype index, SceneNode* node);

    /// \brief Removes a scene node from this selection set.
    /// \param node The node to be unselected.
    /// \undoable
    void remove(SceneNode* node);

    /// \brief Removes a scene node from this selection set.
    /// \param index The index of the node to be unselected.
    /// \undoable
    void removeByIndex(qsizetype index) { _nodes.remove(this, PROPERTY_FIELD(nodes), index); }

    /// \brief Clears the selection.
    ///
    /// All nodes are removed from the selection set.
    /// \undoable
    void clear() { _nodes.clear(this, PROPERTY_FIELD(nodes)); }

    /// \brief Resets the selection set to contain only the given node.
    /// \param node The node to be selected.
    void setNode(SceneNode* node) {
        if(node)
            setNodes({node});
        else
            clear();
    }

    /// \brief Returns the first scene node from the selection, or NULL if the set is empty.
    SceneNode* firstNode() const {
        return nodes().empty() ? nullptr : nodes().front();
    }

private:

    /// References to the selected scene nodes.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(SceneNode*, nodes, setNodes, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_WEAK_REF);
};

}   // End of namespace
