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

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/particles/objects/ParticleType.h>
#include "ParticleSettingsPage.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParticleSettingsPage);

class NameColumnDelegate : public QStyledItemDelegate
{
public:
    NameColumnDelegate(QObject* parent = 0) : QStyledItemDelegate(parent) {}
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override { return nullptr; }
};

class RadiusColumnDelegate : public QStyledItemDelegate
{
public:
    RadiusColumnDelegate(QObject* parent = 0) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        if(!index.model()->data(index, Qt::EditRole).isValid())
            return nullptr;
        QDoubleSpinBox* editor = new QDoubleSpinBox(parent);
        editor->setFrame(false);
        editor->setMinimum(0);
        editor->setSingleStep(0.1);
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        double value = index.model()->data(index, Qt::EditRole).toDouble();
        QDoubleSpinBox* spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->setValue(value);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        QDoubleSpinBox* spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->interpretText();
        double value = spinBox->value();
        model->setData(index, value, Qt::EditRole);
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        editor->setGeometry(option.rect);
    }

    QString displayText(const QVariant& value, const QLocale& locale) const override {
        if(value.isValid())
            return QString::number(value.toDouble());
        else
            return QString();
    }
};

class ColorColumnDelegate : public QStyledItemDelegate
{
public:
    ColorColumnDelegate(QObject* parent = 0) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QColor oldColor = index.model()->data(index, Qt::EditRole).value<QColor>();
        QString ptypeName = index.sibling(index.row(), 0).data().toString();
        QColor newColor = QColorDialog::getColor(oldColor, parent->window(), tr("Select color for '%1'").arg(ptypeName));
        if(newColor.isValid()) {
            const_cast<QAbstractItemModel*>(index.model())->setData(index, QVariant::fromValue(newColor), Qt::EditRole);
        }
        return nullptr;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QBrush brush(index.model()->data(index, Qt::EditRole).value<QColor>());
        painter->fillRect(option.rect, brush);
    }
};

/******************************************************************************
* Creates the widget that contains the plugin specific setting controls.
******************************************************************************/
void ParticleSettingsPage::insertSettingsDialogPage(QTabWidget* tabWidget)
{
    QWidget* page = new QWidget();
    tabWidget->addTab(page, tr("Particles"));
    QVBoxLayout* layout1 = new QVBoxLayout(page);
    layout1->setSpacing(2);

    _particleTypesItem = new QTreeWidgetItem(QStringList() << tr("Particle types") << QString() << QString());
    _particleTypesItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    _structureTypesItem = new QTreeWidgetItem(QStringList() << tr("Structure types") << QString() << QString());
    _structureTypesItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

    // Compile the list of predefined atom type names and any user-defined type names for which
    // presets exist.
    QStringList typeNames;
    for(int i = 0; i < ParticleType::PredefinedParticleType::NUMBER_OF_PREDEFINED_PARTICLE_TYPES; i++)
        typeNames << ParticleType::getPredefinedParticleTypeName((ParticleType::PredefinedParticleType)i);

    QSettings settings;
    settings.beginGroup(ElementType::getElementSettingsKey(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), QStringLiteral("color"), {}));
    typeNames.append(settings.childKeys());
    settings.endGroup();
    settings.beginGroup(ElementType::getElementSettingsKey(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), QStringLiteral("radius"), {}));
    typeNames.append(settings.childKeys());
    settings.endGroup();
    settings.beginGroup(ElementType::getElementSettingsKey(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), QStringLiteral("vdw_radius"), {}));
    typeNames.append(settings.childKeys());
    settings.endGroup();

    // The following is for backward compatibility with OVITO 3.3.5, which used to store the
    // default radii in a different branch of the settings registry.
    settings.beginGroup(QStringLiteral("particles/defaults/color/%1").arg((int)Particles::TypeProperty));
    typeNames.append(settings.childKeys());
    settings.endGroup();
    settings.beginGroup(QStringLiteral("particles/defaults/radius/%1").arg((int)Particles::TypeProperty));
    typeNames.append(settings.childKeys());
    settings.endGroup();

    typeNames.removeDuplicates();
    for(const QString& tname : typeNames) {
        QTreeWidgetItem* childItem = new QTreeWidgetItem();
        childItem->setText(0, tname);
        Color color = ElementType::getDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), tname, 0, true);
        FloatType displayRadius = ParticleType::getDefaultParticleRadius(Particles::TypeProperty, tname, 0, true, ParticleType::DisplayRadius);
        FloatType vdwRadius = ParticleType::getDefaultParticleRadius(Particles::TypeProperty, tname, 0, true, ParticleType::VanDerWaalsRadius);
        childItem->setData(1, Qt::DisplayRole, QVariant::fromValue((QColor)color));
        childItem->setData(2, Qt::DisplayRole, QVariant::fromValue(displayRadius));
        childItem->setData(3, Qt::DisplayRole, QVariant::fromValue(vdwRadius));
        childItem->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
        _particleTypesItem->addChild(childItem);
    }

    QStringList structureNames;
    for(int i = 0; i < ParticleType::PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES; i++)
        structureNames << ParticleType::getPredefinedStructureTypeName((ParticleType::PredefinedStructureType)i);
    settings.beginGroup("particles/defaults/color");
    settings.beginGroup(QString::number((int)Particles::StructureTypeProperty));
    structureNames.append(settings.childKeys());
    structureNames.removeDuplicates();

    for(const QString& tname : structureNames) {
        QTreeWidgetItem* childItem = new QTreeWidgetItem();
        childItem->setText(0, tname);
        Color color = ElementType::getDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::StructureTypeProperty), tname, 0, true);
        childItem->setData(1, Qt::DisplayRole, QVariant::fromValue((QColor)color));
        childItem->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
        _structureTypesItem->addChild(childItem);
    }

    layout1->addWidget(new QLabel(tr("Default particle colors and sizes:")));
    _predefTypesTable = new QTreeWidget();
    layout1->addWidget(_predefTypesTable, 1);
    _predefTypesTable->setColumnCount(4);
    _predefTypesTable->setHeaderLabels(QStringList() << tr("Type") << tr("Color") << tr("Display radius") << tr("Van der Waals radius"));
    _predefTypesTable->setRootIsDecorated(true);
    _predefTypesTable->setAllColumnsShowFocus(true);
    _predefTypesTable->addTopLevelItem(_particleTypesItem);
    _predefTypesTable->addTopLevelItem(_structureTypesItem);
    _predefTypesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _predefTypesTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    _predefTypesTable->setColumnWidth(0, 280);
    _particleTypesItem->setFirstColumnSpanned(true);
    _structureTypesItem->setFirstColumnSpanned(true);

    NameColumnDelegate* nameDelegate = new NameColumnDelegate(this);
    _predefTypesTable->setItemDelegateForColumn(0, nameDelegate);
    ColorColumnDelegate* colorDelegate = new ColorColumnDelegate(this);
    _predefTypesTable->setItemDelegateForColumn(1, colorDelegate);
    RadiusColumnDelegate* radiusDelegate = new RadiusColumnDelegate(this);
    _predefTypesTable->setItemDelegateForColumn(2, radiusDelegate);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0,0,0,0);
    QPushButton* restoreBuiltinDefaultsButton = new QPushButton(tr("Restore built-in defaults"));
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(restoreBuiltinDefaultsButton);
    connect(restoreBuiltinDefaultsButton, &QPushButton::clicked, this, &ParticleSettingsPage::restoreBuiltinParticlePresets);
    layout1->addLayout(buttonLayout);
}

/******************************************************************************
* Lets the page save all changed settings.
******************************************************************************/
void ParticleSettingsPage::saveValues(QTabWidget* tabWidget)
{
    // Remove outdated settings branch from old OVITO versions.
    QSettings settings;
    settings.beginGroup(ElementType::getElementSettingsKey(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), QStringLiteral("color"), {}));
    settings.remove({});
    OVITO_ASSERT(settings.childKeys().empty());
    settings.endGroup();
    settings.beginGroup(ElementType::getElementSettingsKey(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), QStringLiteral("radius"), {}));
    settings.remove({});
    OVITO_ASSERT(settings.childKeys().empty());
    settings.endGroup();
    settings.beginGroup(ElementType::getElementSettingsKey(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), QStringLiteral("vdw_radius"), {}));
    settings.remove({});
    OVITO_ASSERT(settings.childKeys().empty());
    settings.endGroup();

    // This is for backward compatibility with OVITO 3.3.5.
    // Newer OVITO versions store the default colors/radii in a different location.
    settings.beginGroup(QStringLiteral("particles/defaults/color/%1").arg((int)Particles::TypeProperty));
    settings.remove({});
    settings.endGroup();
    settings.beginGroup(QStringLiteral("particles/defaults/radius/%1").arg((int)Particles::TypeProperty));
    settings.remove({});
    settings.endGroup();
    settings.beginGroup(QStringLiteral("particles/defaults/color/%1").arg((int)Particles::StructureTypeProperty));
    settings.remove({});
    settings.endGroup();

    for(int i = 0; i < _particleTypesItem->childCount(); i++) {
        QTreeWidgetItem* item = _particleTypesItem->child(i);
        const QString& typeName = item->text(0);
        QColor color = item->data(1, Qt::DisplayRole).value<QColor>();
        FloatType displayRadius = item->data(2, Qt::DisplayRole).value<FloatType>();
        FloatType vdwRadius = item->data(3, Qt::DisplayRole).value<FloatType>();
        ElementType::setDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), typeName, color);
        ParticleType::setDefaultParticleRadius(Particles::TypeProperty, typeName, displayRadius, ParticleType::DisplayRadius);
        ParticleType::setDefaultParticleRadius(Particles::TypeProperty, typeName, vdwRadius, ParticleType::VanDerWaalsRadius);
    }

    for(int i = 0; i < _structureTypesItem->childCount(); i++) {
        QTreeWidgetItem* item = _structureTypesItem->child(i);
        const QString& typeName = item->text(0);
        QColor color = item->data(1, Qt::DisplayRole).value<QColor>();
        ElementType::setDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::StructureTypeProperty), typeName, color);
    }
}

/******************************************************************************
* Restores the built-in default particle colors and sizes.
******************************************************************************/
void ParticleSettingsPage::restoreBuiltinParticlePresets()
{
    for(int i = 0; i < ParticleType::PredefinedParticleType::NUMBER_OF_PREDEFINED_PARTICLE_TYPES; i++) {
        QTreeWidgetItem* item = _particleTypesItem->child(i);
        Color color = ElementType::getDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), item->text(0), 0, false);
        FloatType displayRadius = ParticleType::getDefaultParticleRadius(Particles::TypeProperty, item->text(0), 0, false, ParticleType::DisplayRadius);
        FloatType vdwRadius = ParticleType::getDefaultParticleRadius(Particles::TypeProperty, item->text(0), 0, false, ParticleType::VanDerWaalsRadius);
        item->setData(1, Qt::DisplayRole, QVariant::fromValue((QColor)color));
        item->setData(2, Qt::DisplayRole, QVariant::fromValue(displayRadius));
        item->setData(3, Qt::DisplayRole, QVariant::fromValue(vdwRadius));
    }
    for(int i = _particleTypesItem->childCount() - 1; i >= ParticleType::PredefinedParticleType::NUMBER_OF_PREDEFINED_PARTICLE_TYPES; i--) {
        delete _particleTypesItem->takeChild(i);
    }

    for(int i = 0; i < ParticleType::PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES; i++) {
        QTreeWidgetItem* item = _structureTypesItem->child(i);
        Color color = ElementType::getDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::StructureTypeProperty), item->text(0), 0, false);
        item->setData(1, Qt::DisplayRole, QVariant::fromValue((QColor)color));
    }
}

}	// End of namespace
