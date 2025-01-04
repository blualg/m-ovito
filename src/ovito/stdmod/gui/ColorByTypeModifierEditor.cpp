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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/ObjectStatusDisplay.h>
#include <ovito/gui/desktop/properties/DataObjectReferenceParameterUI.h>
#include <ovito/stdmod/modifiers/ColorByTypeModifier.h>
#include "ColorByTypeModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorByTypeModifierEditor);
SET_OVITO_OBJECT_EDITOR(ColorByTypeModifier, ColorByTypeModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ColorByTypeModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    QWidget* rollout = createRollout(tr("Color by type"), rolloutParams, "manual:particles.modifiers.color_by_type");
#ifdef OVITO_BUILD_BASIC
    disableRollout(rollout, tr("This program feature is only available in OVITO Pro &mdash; the complete version of this software. Please visit <a href=\"https://www.ovito.org/#proFeatures\">www.ovito.org</a> for more information."));
#endif

    // Create the rollout contents.
    QVBoxLayout* layout = new QVBoxLayout(rollout);
    layout->setContentsMargins(4,4,4,4);
    layout->setSpacing(2);

    DataObjectReferenceParameterUI* pclassUI = createParamUI<DataObjectReferenceParameterUI>(PROPERTY_FIELD(GenericPropertyModifier::subject), PropertyContainer::OOClass());
    layout->addWidget(new QLabel(tr("Operate on:")));
    layout->addWidget(pclassUI->comboBox());
    pclassUI->setObjectFilter<PropertyContainer>([](const PropertyContainer* container) {
        return container->getOOMetaClass().isValidStandardPropertyId(Property::GenericColorProperty)
            && std::any_of(container->properties().begin(), container->properties().end(), &isValidInputProperty);
    });

    _sourcePropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(ColorByTypeModifier::sourceProperty));
    layout->addSpacing(4);
    layout->addWidget(new QLabel(tr("Property:")));
    layout->addWidget(_sourcePropertyUI->comboBox());

    // Show only typed properties that have some element types attached to them.
    _sourcePropertyUI->setPropertyFilter(std::bind(&isValidInputProperty, std::placeholders::_2));

    layout->addSpacing(4);

    // Only selected elements.
    BooleanParameterUI* onlySelectedPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorByTypeModifier::colorOnlySelected));
    layout->addWidget(onlySelectedPUI->checkBox());

    // Clear selection
    BooleanParameterUI* clearSelectionPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorByTypeModifier::clearSelection));
    layout->addWidget(clearSelectionPUI->checkBox());
    connect(onlySelectedPUI->checkBox(), &QCheckBox::toggled, clearSelectionPUI, &BooleanParameterUI::setEnabled);
    clearSelectionPUI->setEnabled(false);
    layout->addSpacing(4);

    class TableWidget : public QTableView {
    public:
        using QTableView::QTableView;
        virtual QSize sizeHint() const { return QSize(256, 400); }
    };
    _elementTypesBox = new TableWidget();
    ViewModel* model = new ViewModel(this);
    _elementTypesBox->setModel(model);
    _elementTypesBox->setShowGrid(false);
    _elementTypesBox->setSelectionBehavior(QAbstractItemView::SelectRows);
    _elementTypesBox->setCornerButtonEnabled(false);
    _elementTypesBox->verticalHeader()->hide();
    _elementTypesBox->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    _elementTypesBox->setSelectionMode(QAbstractItemView::SingleSelection);
    _elementTypesBox->setWordWrap(false);
    _elementTypesBox->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _elementTypesBox->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    _elementTypesBox->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    _elementTypesBox->verticalHeader()->setDefaultSectionSize(_elementTypesBox->verticalHeader()->minimumSectionSize());
    layout->addWidget(new QLabel(tr("Types:"), rollout));
    layout->addWidget(_elementTypesBox);

    connect(this, &PropertiesEditor::contentsChanged, this, [this,model](RefTarget* editObject) {
        ColorByTypeModifier* modifier = static_object_cast<ColorByTypeModifier>(editObject);
        if(modifier)
            _sourcePropertyUI->setContainerRef(modifier->subject());
        else
            _sourcePropertyUI->setContainerRef({});
        model->refresh();
    });

    // Status label.
    layout->addSpacing(12);
    layout->addWidget(createParamUI<ObjectStatusDisplay>()->statusWidget());
}

/******************************************************************************
* Returns the data stored under the given role for the given RefTarget.
******************************************************************************/
QVariant ColorByTypeModifierEditor::ViewModel::data(const QModelIndex& index, int role) const
{
    if(index.isValid() && index.row() < _elementTypes.size()) {
        if(role == Qt::DisplayRole) {
            if(index.column() == 0)
                return _elementTypes[index.row()]->nameOrNumericId();
            else if(index.column() == 1)
                return _elementTypes[index.row()]->numericId();
        }
        else if(role == Qt::DecorationRole) {
            if(index.column() == 0)
                return (QColor)_elementTypes[index.row()]->color();
        }
    }
    return {};
}

/******************************************************************************
* Returns the header data under the given role for the given RefTarget.
******************************************************************************/
QVariant ColorByTypeModifierEditor::ViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        if(section == 0)
            return tr("Name");
        else if(section == 1)
            return tr("Id");
    }
    return {};
}

/******************************************************************************
* Returns the item flags for the given index.
******************************************************************************/
Qt::ItemFlags ColorByTypeModifierEditor::ViewModel::flags(const QModelIndex& index) const
{
    return Qt::ItemIsEnabled;
}

/******************************************************************************
* Updates the contents of the model.
******************************************************************************/
void ColorByTypeModifierEditor::ViewModel::refresh()
{
    beginResetModel();
    _elementTypes.clear();

    ColorByTypeModifier* mod = static_object_cast<ColorByTypeModifier>(editor()->editObject());
    if(mod && mod->subject() && mod->sourceProperty()) {
        // Populate types list based on the selected input property.
        for(const PipelineFlowState& inputState : editor()->getPipelineInputs()) {
            if(const PropertyContainer* container = inputState.getLeafObject(mod->subject())) {
                if(const Property* inputProperty = mod->sourceProperty().findInContainer(container)) {
                    for(const ElementType* type : inputProperty->elementTypes()) {
                        if(type)
                            _elementTypes.push_back(type);
                    }
                }
            }
        }
    }

    endResetModel();
}

}   // End of namespace
