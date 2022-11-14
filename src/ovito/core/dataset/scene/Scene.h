////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/core/dataset/animation/TimeInterval.h>
#include "SceneNode.h"

namespace Ovito {

/**
 * \brief This represents an entire scene node tree.
 */
class OVITO_CORE_EXPORT Scene : public SceneNode
{
	OVITO_CLASS(Scene)
	Q_CLASSINFO("ClassNameAlias", "RootSceneNode");	// For backward compatibility with OVITO 3.7.11.

public:

	/// \brief Creates a scene object.
	Q_INVOKABLE Scene(ObjectCreationParams params);

	/// \brief Searches the scene for a node with the given name.
	/// \param nodeName The name to look for.
	/// \return The scene node or \c NULL, if there is no node with the given name.
	SceneNode* getNodeByName(const QString& nodeName) const;

	/// \brief Generates a name for a node that is unique throughout the scene.
	/// \param baseName A base name that will be made unique by appending a number.
	/// \return The generated unique name.
	QString makeNameUnique(QString baseName) const;

	/// \brief Returns the bounding box of the scene.
	/// \param time The time at which the bounding box should be computed.
	/// \return An world axis-aligned box that contains the bounding boxes of all child nodes.
	virtual Box3 localBoundingBox(TimePoint time, TimeInterval& validity) const override { return Box3(); }

	/// \brief Returns whether this is the root scene node.
	virtual bool isRootNode() const override { return true; }

	/// \brief Deletes all child nodes of the scene.
	void clear() {
		while(!children().empty())
			children().back()->deleteNode();
	}
};

}	// End of namespace
