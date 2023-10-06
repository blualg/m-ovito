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

#include <ovito/gui/qml/GUI.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/app/PluginManager.h>
#include "DataObjectReferenceParameterUI.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(DataObjectReferenceParameterUI);

/******************************************************************************
* Sets the class of data objects the user can choose from.
******************************************************************************/
void DataObjectReferenceParameterUI::setDataObjectType(const QString& typeName)
{
    _dataObjectType = nullptr;
    auto dataObjectType = PluginManager::instance().findClass(QString(), typeName);
    if(!dataObjectType) {
        qWarning() << "DataObjectReferenceParameterUI: Data object class" << typeName << "does not exist.";
    }
    else if(!dataObjectType->isDerivedFrom(DataObject::OOClass())) {
        qWarning() << "DataObjectReferenceParameterUI: Data object class" << typeName << "is not derived from DataObject base class.";
    }
    else {
        _dataObjectType = static_cast<DataObjectClassPtr>(dataObjectType);
    }
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool DataObjectReferenceParameterUI::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ReferenceEvent::PipelineInputChanged) {
        // The modifier's input from the pipeline has changed -> update list of available input data objects.
        updateDataObjectList();
        // The index of the selected list item may have changed. Update the index as well.
        updateUI();
    }
    return ParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* Rebuilds the list of available input data objects the user can choose from.
******************************************************************************/
void DataObjectReferenceParameterUI::updateDataObjectList()
{
    std::vector<DataObjectReference> acceptedDataObjects;

    if(_dataObjectType) {
        if(Modifier* mod = static_object_cast<Modifier>(editObject())) {
            for(ModifierApplication* modApp : mod->modifierApplications()) {
                const PipelineFlowState& state = modApp->evaluateInputSynchronous(mod->dataset()->animationSettings()->time());
                std::vector<ConstDataObjectPath> dataObjectPaths = state.getObjectsRecursive(*_dataObjectType);
                for(const ConstDataObjectPath& path : dataObjectPaths) {
                    DataObjectReference dataObjRef(path);

                    // Do not add the same dta object to the list more than once.
                    if(std::find(acceptedDataObjects.begin(), acceptedDataObjects.end(), dataObjRef) != acceptedDataObjects.end())
                        continue;

                    acceptedDataObjects.push_back(dataObjRef);
                }
            }

            // Sort list entries alphabetically.
            std::sort(acceptedDataObjects.begin(), acceptedDataObjects.end(), [](const DataObjectReference& a, const DataObjectReference& b) {
                return a.dataTitle().localeAwareCompare(b.dataTitle()) < 0;
            });

            // Get the currently selected data object from the modifier's parameter field.
            QVariant val = editObject()->getPropertyFieldValue(*propertyField());
            OVITO_ASSERT_MSG(val.canConvert<DataObjectReference>(), "DataObjectReferenceParameterUI::updateDataObjectList()", qPrintable(QString("The property field of object class %1 is not of type <DataObjectReference>.").arg(editObject()->metaObject()->className())));
            const DataObjectReference& selectedObject = val.value<DataObjectReference>();

            // Add a placeholder item if the selected data object does not exist anymore in the modifier's input.
            if(selectedObject && boost::find(acceptedDataObjects, selectedObject) == acceptedDataObjects.end()) {
                QString title = selectedObject.dataTitle();
                if(title.isEmpty() && selectedObject.dataClass())
                    title = selectedObject.dataClass()->displayName();
//              if(!currentContainerFilteredOut)
                    title += tr(" (not available)");
                acceptedDataObjects.emplace_back(selectedObject.dataClass(), selectedObject.dataPath(), title);
            }
        }
    }

    _model->resetList(std::move(acceptedDataObjects));
}

/******************************************************************************
* Obtains the current value of the parameter from the C++ object.
******************************************************************************/
QVariant DataObjectReferenceParameterUI::getCurrentValue() const
{
    if(_dataObjectType && editObject()) {

        // Get the currently selected data object from the modifier's parameter field.
        QVariant val = editObject()->getPropertyFieldValue(*propertyField());
        OVITO_ASSERT_MSG(val.canConvert<DataObjectReference>(), "DataObjectReferenceParameterUI::getCurrentValue()", qPrintable(QString("The property field of object class %1 is not of type <DataObjectReference>.").arg(editObject()->metaObject()->className())));
        DataObjectReference selectedDataObjectRef = val.value<DataObjectReference>();

        // Look up its index in the list.
        auto iter = boost::find(_model->dataObjects(), selectedDataObjectRef);
        if(iter != _model->dataObjects().cend())
            return QVariant::fromValue(std::distance(_model->dataObjects().cbegin(), iter));
    }
    return -1;
}

/******************************************************************************
* Changes the current value of the C++ object parameter.
******************************************************************************/
void DataObjectReferenceParameterUI::setCurrentValue(const QVariant& val)
{
    if(_dataObjectType && editObject()) {
        int index = val.toInt();
        if(index >= 0 && index < _model->dataObjects().size()) {
            const DataObjectReference& oldVal = editObject()->getPropertyFieldValue(*propertyField()).value<DataObjectReference>();
            const DataObjectReference& newVal = _model->dataObjects()[index];
            if(newVal != oldVal) {
                UndoableTransaction::handleExceptions(editObject()->dataset()->undoStack(), tr("Select input object"), [&]() {
                    editObject()->setPropertyFieldValue(*propertyField(), QVariant::fromValue(newVal));
                });
            }
        }
    }
}

/******************************************************************************
* Returns the data stored in the model under the given role.
******************************************************************************/
QVariant DataObjectReferenceParameterUI::Model::data(const QModelIndex& index, int role) const
{
    if(index.isValid()) {
        if(index.row() < dataObjects().size()) {
            if(role == Qt::DisplayRole)
                return dataObjects()[index.row()].dataTitle();
            else if(role == Qt::UserRole)
                return QVariant::fromValue(dataObjects()[index.row()]);
        }
        else if(dataObjects().empty()) {
            if(role == Qt::DisplayRole)
                return tr("<No available data objects>");
        }
    }
    return QVariant();
}

}   // End of namespace
