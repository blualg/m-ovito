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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/DataObjectReference.h>
#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/core/dataset/pipeline/PipelineNode.h>
#include "DataInspectionApplet.h"
#include "DataInspectorPanel.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(DataInspectionApplet);

/******************************************************************************
* Sets the panel hosting this applet.
******************************************************************************/
void DataInspectionApplet::setInspectorPanel(DataInspectorPanel* inspectorPanel)
{
    OVITO_ASSERT(!_inspectorPanel);
    setUserInterface(inspectorPanel->ui());
    _inspectorPanel = inspectorPanel;
}

/******************************************************************************
* Returns the currently selected data pipeline.
******************************************************************************/
Pipeline* DataInspectionApplet::currentPipeline() const
{
    return inspectorPanel()->selectedPipeline();
}

/******************************************************************************
* Returns the currently selected pipeline scene node.
******************************************************************************/
SceneNode* DataInspectionApplet::currentSceneNode() const
{
    return inspectorPanel()->selectedSceneNode();
}

/******************************************************************************
* Returns the current output of the data pipeline displayed in the applet.
******************************************************************************/
const PipelineFlowState& DataInspectionApplet::currentState() const
{
    return inspectorPanel()->pipelineOutput();
}

/******************************************************************************
* Determines whether the given pipeline dataset contains data that can be
* displayed by this applet.
******************************************************************************/
bool DataInspectionApplet::appliesTo(const DataCollection& data)
{
    return data.containsObjectRecursive(*_dataObjectClass);
}

/******************************************************************************
* Creates and returns the list widget displaying the list of data object objects.
******************************************************************************/
QListWidget* DataInspectionApplet::objectSelectionWidget()
{
    if(!_objectSelectionWidget) {
        _objectSelectionWidget = new QListWidget();

        class ItemDelegate : public QStyledItemDelegate
        {
        public:
            ItemDelegate() {
                _font1 = QGuiApplication::font();
                _font1.setBold(true);
                _font2 = QGuiApplication::font();
                _font2.setItalic(true);
            }
        protected:
            QFont _font1, _font2;
            virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
                QStyleOptionViewItem options = option;
                initStyleOption(&options, index);
#ifdef Q_OS_LINUX
                options.state.setFlag(QStyle::State_HasFocus, false);
#endif

                // Draw list item without text content.
                QString text = std::move(options.text);
                options.text.clear();
                options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter, options.widget);

                // Draw first line of text.
                options.rect.adjust(0, 4, 0, -4);
                options.backgroundBrush = QBrush();
#ifndef Q_OS_WIN
                // Override text color for highlighted items.
                if(options.state & QStyle::State_Selected)
                    options.palette.setColor(QPalette::Text, options.palette.color(QPalette::Active, QPalette::HighlightedText));
#endif
                options.state.setFlag(QStyle::State_Selected, false);
                options.state.setFlag(QStyle::State_MouseOver, false);
                options.icon = QIcon();
                options.displayAlignment = Qt::AlignLeft | Qt::AlignTop;
                options.font = _font1;
                options.text = std::move(text);
                options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter, options.widget);

                // Draw second line of text.
                options.text = index.data(Qt::StatusTipRole).toString();
                options.displayAlignment = Qt::AlignLeft | Qt::AlignBottom;
                options.font = _font2;
                options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter, options.widget);
            }

            virtual QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
                QStyleOptionViewItem options = option;
                initStyleOption(&options, index);
                QSize size = options.widget->style()->sizeFromContents(QStyle::CT_ItemViewItem, &options, QSize(), options.widget);
                QFontMetrics fm1(_font1);
                QFontMetrics fm2(_font2);
                size.setHeight(std::max(size.height(), fm1.height() + fm2.height() + 8));
                return size;
            }
        };
        _objectSelectionWidget->setUniformItemSizes(true);
        _objectSelectionWidget->setItemDelegate(new ItemDelegate());

        updateDataObjectList();
        connect(_objectSelectionWidget, &QListWidget::currentRowChanged, this, [this]() {
            QListWidgetItem* item = _objectSelectionWidget->currentItem();
            const DataObject* prevSelectedDataObject = selectedDataObject();
            const QString prevSelectedDataObjectPathString = _selectedDataObjectPathString;
            if(item) {
                _selectedDataObjectPath = item->data(Qt::UserRole).value<ConstDataObjectPath>();
                _selectedDataObjectPathString = _selectedDataObjectPath.toString();
                _selectedDataObject = _selectedDataObjectPath.back();
            }
            else {
                _selectedDataObjectPath.clear();
                _selectedDataObjectPathString.clear();
                _selectedDataObject = nullptr;
            }
            if(prevSelectedDataObject != selectedDataObject()) {
                Q_EMIT currentObjectChanged(selectedDataObject());
            }
            if(prevSelectedDataObjectPathString != _selectedDataObjectPathString) {
                Q_EMIT currentObjectPathChanged(_selectedDataObjectPathString);
            }
        });
    }
    return _objectSelectionWidget;
}

/******************************************************************************
* Updates the contents displayed in the inspector.
******************************************************************************/
void DataInspectionApplet::updateDisplay()
{
    updateDataObjectList();
}

/******************************************************************************
* Updates the list of data objects displayed in the inspector.
******************************************************************************/
void DataInspectionApplet::updateDataObjectList()
{
    // Build list of all data objects of the supported type in the current data collection.
    std::vector<ConstDataObjectPath> objectPaths;
    if(currentState())
        objectPaths = getDataObjectPaths();

    int currentRow = 0;
    if(_objectSelectionWidget) {
        _objectSelectionWidget->setUpdatesEnabled(false);
        QSignalBlocker signalBlocker(_objectSelectionWidget);

        // Update displayed list of data objects.
        // Overwrite existing list items, add new items when needed.
        int numItems = 0;
        for(const ConstDataObjectPath& path : objectPaths) {
            const DataObject* dataObj = path.back();
            QListWidgetItem* item;
            QString itemTitle = dataObj->objectTitle();
            if(_objectSelectionWidget->count() <= numItems) {
                item = new QListWidgetItem(itemTitle, _objectSelectionWidget);
            }
            else {
                item = _objectSelectionWidget->item(numItems);
                item->setText(itemTitle);
            }
            item->setToolTip(tr("Python identifier: \"%1\"").arg(dataObj->identifier()));
            auto createdByNode = dataObj->createdByNode().lock();
            item->setStatusTip(createdByNode ? createdByNode->objectTitle() : QString{});
            item->setData(Qt::UserRole, QVariant::fromValue(path));

            // Select again the previously selected data object.
            if(path.toString() == _selectedDataObjectPathString)
                _objectSelectionWidget->setCurrentItem(item);

            numItems++;
        }
        // Remove excess items from list.
        while(_objectSelectionWidget->count() > numItems)
            delete _objectSelectionWidget->takeItem(_objectSelectionWidget->count() - 1);

        if(!_objectSelectionWidget->currentItem() && _objectSelectionWidget->count() != 0)
            _objectSelectionWidget->setCurrentRow(0);

        // Reactivate updates.
        _objectSelectionWidget->setUpdatesEnabled(true);
        currentRow = _objectSelectionWidget->currentRow();
    }

    // Inform others about the currently selected object.
    const DataObject* prevSelectedDataObject = selectedDataObject();
    const QString prevSelectedDataObjectPathString = _selectedDataObjectPathString;
    if(currentRow >= 0 && currentRow < objectPaths.size()) {
        _selectedDataObjectPath = std::move(objectPaths[currentRow]);
        _selectedDataObjectPathString = _selectedDataObjectPath.toString();
        _selectedDataObject = _selectedDataObjectPath.back();
    }
    else {
        _selectedDataObjectPath.clear();
        _selectedDataObjectPathString.clear();
        _selectedDataObject = nullptr;
    }
    if(prevSelectedDataObject != selectedDataObject()) {
        Q_EMIT currentObjectChanged(selectedDataObject());
    }
    if(prevSelectedDataObjectPathString != _selectedDataObjectPathString) {
        Q_EMIT currentObjectPathChanged(_selectedDataObjectPathString);
    }
}

/******************************************************************************
* Selects a specific data object in this applet.
******************************************************************************/
bool DataInspectionApplet::selectDataObject(const PipelineNode* createdByNode, const QString& objectIdentifierHint, const QVariant& modeHint)
{
    if(!_objectSelectionWidget)
        return false;

    // Check the items in the data object list.
    for(int i = 0; i < _objectSelectionWidget->count(); i++) {
        QListWidgetItem* item = _objectSelectionWidget->item(i);
        const ConstDataObjectPath& objectPath = item->data(Qt::UserRole).value<ConstDataObjectPath>();
        if(!objectPath.empty()) {
            if(objectPath.back()->createdByNode().lock().get() == createdByNode) {
                if(objectIdentifierHint.isEmpty() || objectPath.back()->identifier() == objectIdentifierHint || objectPath.back()->identifier().startsWith(objectIdentifierHint + QChar('.'))) {
                    _objectSelectionWidget->setCurrentRow(i);
                    return true;
                }
            }
        }
    }
    return false;
}

}   // End of namespace
