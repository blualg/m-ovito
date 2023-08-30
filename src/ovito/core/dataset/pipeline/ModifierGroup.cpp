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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModifierApplication.h>

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ModifierGroup);
DEFINE_PROPERTY_FIELD(ModifierGroup, isCollapsed);
SET_PROPERTY_FIELD_LABEL(ModifierGroup, isCollapsed, "Collapsed");

/******************************************************************************
* This is called from a ModifierApplication whenever it becomes a member of this group.
******************************************************************************/
void ModifierGroup::registerModApp(ModifierApplication* modApp)
{
    connect(modApp, &ModifierApplication::objectEvent, this, &ModifierGroup::modAppEvent, Qt::UniqueConnection);
    updateCombinedStatus();
    Q_EMIT modifierAdded(modApp);
}

/******************************************************************************
* This is called from a ModifierApplication whenever it is removed from this group.
******************************************************************************/
void ModifierGroup::unregisterModApp(ModifierApplication* modApp)
{
    disconnect(modApp, &ModifierApplication::objectEvent, this, &ModifierGroup::modAppEvent);
    updateCombinedStatus();
    Q_EMIT modifierRemoved(modApp);
}

/******************************************************************************
* Is called when one of the group's modapps has generated an event.
******************************************************************************/
void ModifierGroup::modAppEvent(RefTarget* sender, const ReferenceEvent& event)
{
    if(event.type() == ReferenceEvent::ObjectStatusChanged) {
        // Update the group's status whenever the status of one of its members changes.
        updateCombinedStatus();
    }
}

/******************************************************************************
* This is called whenever one of the group's member modapps changes.
* It computes the combined status of the entire group.
******************************************************************************/
void ModifierGroup::updateCombinedStatus()
{
    bool isActive = false;
    PipelineStatus combinedStatus(PipelineStatus::Success);
    if(isEnabled()) {
        visitDependents([&](RefMaker* dependent) {
            if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dependent)) {
                OVITO_ASSERT(modApp->modifierGroup() == this);
                if(modApp->isObjectActive())
                    isActive = true;

                if(modApp->modifier() && modApp->modifier()->isEnabled()) {
                    const PipelineStatus& modAppStatus = modApp->status();
                    if(combinedStatus.type() == PipelineStatus::Success || modAppStatus.type() == PipelineStatus::Error)
                        combinedStatus.setType(modAppStatus.type());
                    if(!modAppStatus.text().isEmpty()) {
                        if(!combinedStatus.text().isEmpty())
                            combinedStatus.setText(combinedStatus.text() + QStringLiteral("\n") + modAppStatus.text());
                        else
                            combinedStatus.setText(modAppStatus.text());
                    }
                }
            }
        });
    }

    if(!isObjectActive() && isActive)
        incrementNumberOfActiveTasks();
    else if(isObjectActive() && !isActive)
        decrementNumberOfActiveTasks();
    setStatus(std::move(combinedStatus));
}

/******************************************************************************
* Returns the list of modifier applications that are part of this group.
******************************************************************************/
QVector<ModifierApplication*> ModifierGroup::modifierApplications() const
{
    // Gather the list of modapps that are part of the group.
    QVector<ModifierApplication*> modApps;
    visitDependents([&](RefMaker* dependent) {
        if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dependent))
            modApps.push_back(modApp);
    });
    if(!modApps.empty()) {
        // Order the modapps according to their sequence in the data pipeline.
        boost::sort(modApps, [](ModifierApplication* a, ModifierApplication* b) {
            return b->isReferencedBy(a);
        });
#ifdef OVITO_DEBUG
        // The input (successor) of the last modapp (the group's tail) should not be part of the modifier group.
        ModifierApplication* successor = !modApps.empty() ? dynamic_object_cast<ModifierApplication>(modApps.back()->input()) : nullptr;
        OVITO_ASSERT(!successor || successor->modifierGroup() != this);
        // All others should be referenced by the group's head modapp. This ensures that the modapps are all from the same pipeline branch.
        for(ModifierApplication* modApp : modApps) {
            OVITO_ASSERT(modApp->modifierGroup() == this);
            OVITO_ASSERT(modApp->isReferencedBy(modApps.front()));
        }
#endif
    }

    return modApps;
}

/******************************************************************************
* Returns the list of pipelines that contain this modifier group.
******************************************************************************/
QSet<PipelineSceneNode*> ModifierGroup::pipelines(bool onlyScenePipelines) const
{
    QSet<PipelineSceneNode*> pipelineList;
    visitDependents([&](RefMaker* dependent) {
        if(ModifierApplication* modApp = dynamic_object_cast<ModifierApplication>(dependent))
            pipelineList.unite(modApp->pipelines(onlyScenePipelines));
    });
    return pipelineList;
}

}   // End of namespace
