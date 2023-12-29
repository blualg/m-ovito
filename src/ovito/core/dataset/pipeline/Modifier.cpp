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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/Modifier.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(Modifier);
DEFINE_PROPERTY_FIELD(Modifier, isEnabled);
DEFINE_PROPERTY_FIELD(Modifier, title);
SET_PROPERTY_FIELD_LABEL(Modifier, isEnabled, "Enabled");
SET_PROPERTY_FIELD_CHANGE_EVENT(Modifier, isEnabled, ReferenceEvent::TargetEnabledOrDisabled);
SET_PROPERTY_FIELD_LABEL(Modifier, title, "Name");
SET_PROPERTY_FIELD_CHANGE_EVENT(Modifier, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
Modifier::Modifier(ObjectInitializationFlags flags) : RefTarget(flags),
    _isEnabled(true)
{
}

/******************************************************************************
* Creates a new modification node for inserting this modifier into a pipeline.
******************************************************************************/
OORef<ModificationNode> Modifier::createModificationNode()
{
    // Look which ModificationNode class has been registered for this Modifier class.
    for(OvitoClassPtr clazz = &getOOClass(); clazz != nullptr; clazz = clazz->superClass()) {
        if(OvitoClassPtr nodeClass = ModificationNode::registry().getModificationNodeType(clazz)) {
            if(!nodeClass->isDerivedFrom(ModificationNode::OOClass()))
                throw Exception(tr("The modification node class %1 assigned to the Modifier-derived class %2 is not derived from ModificationNode.").arg(nodeClass->name(), clazz->name()));
#ifdef OVITO_DEBUG
            for(OvitoClassPtr superClazz = clazz->superClass(); superClazz != nullptr; superClazz = superClazz->superClass()) {
                if(OvitoClassPtr nodeSuperClass = ModificationNode::registry().getModificationNodeType(superClazz)) {
                    if(!nodeClass->isDerivedFrom(*nodeSuperClass))
                        throw Exception(tr("The modification node class %1 assigned to the Modifier-derived class %2 is not derived from the ModificationNode specialization %3.").arg(nodeClass->name(), clazz->name(), nodeSuperClass->name()));
                }
            }
#endif
            return static_object_cast<ModificationNode>(nodeClass->createInstance());
        }
    }

    // Fall back to generic node type.
    return OORef<ModificationNode>::create();
}

/******************************************************************************
* Returns the number of animation frames this modifier provides.
******************************************************************************/
int Modifier::numberOfOutputFrames(ModificationNode* node) const
{
    OVITO_ASSERT(node);
    if(PipelineNode* input = node->input())
        return input->numberOfSourceFrames();
    return 1;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> Modifier::evaluate(const ModifierEvaluationRequest& request, const PipelineFlowState& input)
{
    PipelineFlowState output = input;
    if(output)
        evaluateSynchronous(request, output);
    return Future<PipelineFlowState>::createImmediate(std::move(output));
}

/******************************************************************************
* Returns the list of pipeline nodes that reference this modifier.
******************************************************************************/
QVector<ModificationNode*> Modifier::nodes() const
{
    QVector<ModificationNode*> list;
    visitDependents([&](RefMaker* dependent) {
        ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent);
        if(node != nullptr && node->modifier() == this)
            list.push_back(node);
    });
    return list;
}

/******************************************************************************
* Returns one of the pipelines nodes referencing this modifier in a pipeline.
******************************************************************************/
ModificationNode* Modifier::someNode() const
{
    ModificationNode* result = nullptr;
    visitDependents([&](RefMaker* dependent) {
        ModificationNode* node = dynamic_object_cast<ModificationNode>(dependent);
        if(node != nullptr && node->modifier() == this)
            result = node;
    });
    return result;
}

/******************************************************************************
* Returns the current status of the modifier's pipeline node(s).
******************************************************************************/
PipelineStatus Modifier::globalStatus() const
{
    // Combine the status values of all ModificationNodes into a single status.
    PipelineStatus result;
    for(ModificationNode* node : nodes()) {
        PipelineStatus s = node->status();

        if(result.text().isEmpty())
            result.setText(s.text());
        else if(s.text() != result.text())
            result.setText(result.text() + QStringLiteral("\n") + s.text());

        if(s.type() == PipelineStatus::Error)
            result.setType(PipelineStatus::Error);
        else if(result.type() != PipelineStatus::Error && s.type() == PipelineStatus::Warning)
            result.setType(PipelineStatus::Warning);
    }
    return result;
}

}   // End of namespace
