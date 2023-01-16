////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

namespace Ovito {

/**
 * \brief A logical group of ModifierApplication objects, which is used in the GUI to group
 *        modifiers in the pipeline editor.
 */
class OVITO_CORE_EXPORT ModifierGroup : public ActiveObject
{
    OVITO_CLASS(ModifierGroup)
    Q_CLASSINFO("DisplayName", "Modifier group");

public:

    /// \brief Constructs a modifier group object.
    Q_INVOKABLE ModifierGroup(ObjectCreationParams params) : ActiveObject(params), _isCollapsed(false) {}

    /// \brief Returns the list of modifier applications that are part of this group.
    QVector<ModifierApplication*> modifierApplications() const;

    /// \brief Returns the list of pipelines that contain this modifier group.
    /// \param onlyScenePipelines If true, pipelines which are currently not part of the scene are ignored.
    QSet<PipelineSceneNode*> pipelines(bool onlyScenePipelines) const;

Q_SIGNALS:

    /// Signal is emitted every time a modifier is added to the group.
    void modifierAdded(ModifierApplication* modApp);

    /// Signal is emitted every time a modifier is removed from the group.
    void modifierRemoved(ModifierApplication* modApp);

private Q_SLOTS:

    /// \brief Is called when one of the group's modapps has generated an event.
    void modAppEvent(RefTarget* sender, const ReferenceEvent& event);

private:

    /// This is called from a ModifierApplication whenever it becomes a member of this group.
    void registerModApp(ModifierApplication* modApp);

    /// This is called from a ModifierApplication whenever it is removed from this group.
    void unregisterModApp(ModifierApplication* modApp);

    /// This is called whenever one of the group's member modapps changes. 
    /// It computes the combined status of the entire group.
    void updateCombinedStatus();

    friend class ModifierApplication;

private:

    /// Indicates whether this group is currently collapsed in the pipeline editor.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, isCollapsed, setCollapsed, PROPERTY_FIELD_NO_UNDO);
};

}   // End of namespace
