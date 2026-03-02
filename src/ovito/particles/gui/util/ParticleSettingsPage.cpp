////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileDialog>
#include <ovito/gui/desktop/dialogs/MessageDialog.h>
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

/// Checks if two floating-point values differ by more than a small tolerance.
static bool significantlyDifferent(double a, double b)
{
    return std::abs(a - b) > 1e-9;
}

/// Finds a child tree widget item by name (column 0 text). Returns nullptr if not found.
static QTreeWidgetItem* findChildByName(QTreeWidgetItem* parent, const QString& name)
{
    for(int i = 0; i < parent->childCount(); i++) {
        if(parent->child(i)->text(0) == name)
            return parent->child(i);
    }
    return nullptr;
}

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

    // Compile the list of predefined particle type names and any user-defined type names for which presets exist.
    QStringList typeNames;
    for(int i = 0; i < (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES; i++)
        typeNames << ParticleType::getChemicalElementSymbol(static_cast<ParticleType::ChemicalElement>(i));

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
        FloatType displayRadius =
            ParticleType::getDefaultParticleRadius(Particles::TypeProperty, tname, 0, true, ParticleType::RadiusVariant::DisplayRadius);
        FloatType vdwRadius =
            ParticleType::getDefaultParticleRadius(Particles::TypeProperty, tname, 0, true, ParticleType::RadiusVariant::VanDerWaalsRadius);
        childItem->setData(1, Qt::DisplayRole, QVariant::fromValue((QColor)color));
        childItem->setData(2, Qt::DisplayRole, QVariant::fromValue(displayRadius));
        childItem->setData(3, Qt::DisplayRole, QVariant::fromValue(vdwRadius));
        childItem->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
        _particleTypesItem->addChild(childItem);
    }

    QStringList structureNames;
    for(int i = 0; i < (int)ParticleType::PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES; i++)
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

    layout1->addWidget(new QLabel(tr("Default colors and radii:")));
    _predefTypesTable = new QTreeWidget();
    layout1->addWidget(_predefTypesTable, 1);
    _predefTypesTable->setColumnCount(4);
    _predefTypesTable->setHeaderLabels(QStringList() << tr("Type name") << tr("Color") << tr("Display radius") << tr("Van der Waals radius"));
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
    QPushButton* exportThemeButton = new QPushButton(tr("Export theme..."));
    QPushButton* importThemeButton = new QPushButton(tr("Import theme..."));
    QPushButton* restoreBuiltinDefaultsButton = new QPushButton(tr("Restore built-in defaults"));
    buttonLayout->addWidget(exportThemeButton);
    buttonLayout->addWidget(importThemeButton);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(restoreBuiltinDefaultsButton);
    connect(exportThemeButton, &QPushButton::clicked, this, &ParticleSettingsPage::exportTheme);
    connect(importThemeButton, &QPushButton::clicked, this, &ParticleSettingsPage::importTheme);
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
        ParticleType::setDefaultParticleRadius(
            Particles::TypeProperty, typeName, displayRadius, ParticleType::RadiusVariant::DisplayRadius);
        ParticleType::setDefaultParticleRadius(
            Particles::TypeProperty, typeName, vdwRadius, ParticleType::RadiusVariant::VanDerWaalsRadius);
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
    OVITO_ASSERT(_particleTypesItem->childCount() >= (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES);
    for(int i = 0; i < (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES; i++) {
        QTreeWidgetItem* item = _particleTypesItem->child(i);
        OVITO_ASSERT(item);
        Color color = ElementType::getDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::TypeProperty), item->text(0), 0, false);
        FloatType displayRadius = ParticleType::getDefaultParticleRadius(
            Particles::TypeProperty, item->text(0), 0, false, ParticleType::RadiusVariant::DisplayRadius);
        FloatType vdwRadius = ParticleType::getDefaultParticleRadius(
            Particles::TypeProperty, item->text(0), 0, false, ParticleType::RadiusVariant::VanDerWaalsRadius);
        item->setData(1, Qt::DisplayRole, QVariant::fromValue((QColor)color));
        item->setData(2, Qt::DisplayRole, QVariant::fromValue(displayRadius));
        item->setData(3, Qt::DisplayRole, QVariant::fromValue(vdwRadius));
    }
    for(int i = _particleTypesItem->childCount() - 1; i >= (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES; i--) {
        delete _particleTypesItem->takeChild(i);
    }

    OVITO_ASSERT(_structureTypesItem->childCount() >= (int)ParticleType::PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES);
    for(int i = 0; i < (int)ParticleType::PredefinedStructureType::NUMBER_OF_PREDEFINED_STRUCTURE_TYPES; i++) {
        QTreeWidgetItem* item = _structureTypesItem->child(i);
        OVITO_ASSERT(item);
        Color color = ElementType::getDefaultColor(OwnerPropertyRef(&Particles::OOClass(), Particles::StructureTypeProperty), item->text(0), 0, false);
        item->setData(1, Qt::DisplayRole, QVariant::fromValue((QColor)color));
    }
}

/******************************************************************************
* Exports the current particle type defaults to a JSON theme file.
******************************************************************************/
void ParticleSettingsPage::exportTheme()
{
    handleExceptions([&] {
        TaskManager::setNativeDialogActive(true);
        QString filename = QFileDialog::getSaveFileName(settingsDialog(),
            tr("Export Particle Theme"), QString(),
            tr("OVITO Particle Theme (*.ovito-theme);;JSON files (*.json)"));
        TaskManager::setNativeDialogActive(false);
        if(filename.isEmpty())
            return;

        QJsonArray particleTypesArray;
        for(int i = 0; i < _particleTypesItem->childCount(); i++) {
            QTreeWidgetItem* item = _particleTypesItem->child(i);
            QColor color = item->data(1, Qt::DisplayRole).value<QColor>();
            double displayRadius = item->data(2, Qt::DisplayRole).toDouble();
            double vdwRadius = item->data(3, Qt::DisplayRole).toDouble();
            QJsonObject typeObj;
            typeObj[QStringLiteral("name")] = item->text(0);
            typeObj[QStringLiteral("color")] = QJsonArray{color.redF(), color.greenF(), color.blueF()};
            typeObj[QStringLiteral("display_radius")] = displayRadius;
            typeObj[QStringLiteral("vdw_radius")] = vdwRadius;
            particleTypesArray.append(typeObj);
        }

        QJsonArray structureTypesArray;
        for(int i = 0; i < _structureTypesItem->childCount(); i++) {
            QTreeWidgetItem* item = _structureTypesItem->child(i);
            QColor color = item->data(1, Qt::DisplayRole).value<QColor>();
            QJsonObject typeObj;
            typeObj[QStringLiteral("name")] = item->text(0);
            typeObj[QStringLiteral("color")] = QJsonArray{color.redF(), color.greenF(), color.blueF()};
            structureTypesArray.append(typeObj);
        }

        QJsonObject root;
        root[QStringLiteral("format")] = QStringLiteral("OvitoParticleTheme");
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("particle_types")] = particleTypesArray;
        root[QStringLiteral("structure_types")] = structureTypesArray;

        QFile file(filename);
        if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
            throw Exception(tr("Failed to open file '%1' for writing: %2").arg(filename).arg(file.errorString()));
        if(file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) == -1)
            throw Exception(tr("Failed to write to file '%1': %2").arg(filename).arg(file.errorString()));
    });
}

/******************************************************************************
* Imports particle type defaults from a JSON theme file.
******************************************************************************/
void ParticleSettingsPage::importTheme()
{
    handleExceptions([&] {
        TaskManager::setNativeDialogActive(true);
        QString filename = QFileDialog::getOpenFileName(settingsDialog(),
            tr("Import Particle Theme"), QString(),
            tr("OVITO Particle Theme (*.ovito-theme);;JSON files (*.json);;All files (*)"));
        TaskManager::setNativeDialogActive(false);
        if(filename.isEmpty())
            return;

        QFile file(filename);
        if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
            throw Exception(tr("Failed to open file '%1' for reading: %2").arg(filename).arg(file.errorString()));

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if(doc.isNull())
            throw Exception(tr("Failed to parse JSON file '%1': %2").arg(filename).arg(parseError.errorString()));

        QJsonObject root = doc.object();
        if(root[QStringLiteral("format")].toString() != QStringLiteral("OvitoParticleTheme"))
            throw Exception(tr("The selected file is not a valid OVITO particle theme file."));
        if(root[QStringLiteral("version")].toInt() > 1)
            throw Exception(tr("The theme file version is not supported by this version of OVITO. Please update OVITO to import this file."));

        // Apply imported particle types (merge mode: only update types present in the file).
        int importedParticleTypeCount = 0;
        QJsonArray particleTypes = root[QStringLiteral("particle_types")].toArray();
        for(const QJsonValue& val : particleTypes) {
            QJsonObject typeObj = val.toObject();
            QString name = typeObj[QStringLiteral("name")].toString();
            if(name.isEmpty())
                continue;

            QTreeWidgetItem* item = findChildByName(_particleTypesItem, name);
            if(!item) {
                item = new QTreeWidgetItem();
                item->setText(0, name);
                item->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
                _particleTypesItem->addChild(item);
            }

            QJsonArray colorArr = typeObj[QStringLiteral("color")].toArray();
            if(colorArr.size() == 3) {
                QColor newColor = QColor::fromRgbF(colorArr[0].toDouble(), colorArr[1].toDouble(), colorArr[2].toDouble());
                QColor oldColor = item->data(1, Qt::DisplayRole).value<QColor>();
                if(significantlyDifferent(newColor.redF(), oldColor.redF()) || significantlyDifferent(newColor.greenF(), oldColor.greenF()) || significantlyDifferent(newColor.blueF(), oldColor.blueF()))
                    item->setData(1, Qt::DisplayRole, QVariant::fromValue(newColor));
            }
            if(typeObj.contains(QStringLiteral("display_radius"))) {
                double newRadius = typeObj[QStringLiteral("display_radius")].toDouble();
                if(significantlyDifferent(newRadius, item->data(2, Qt::DisplayRole).toDouble()))
                    item->setData(2, Qt::DisplayRole, QVariant::fromValue(newRadius));
            }
            if(typeObj.contains(QStringLiteral("vdw_radius"))) {
                double newRadius = typeObj[QStringLiteral("vdw_radius")].toDouble();
                if(significantlyDifferent(newRadius, item->data(3, Qt::DisplayRole).toDouble()))
                    item->setData(3, Qt::DisplayRole, QVariant::fromValue(newRadius));
            }
            importedParticleTypeCount++;
        }

        // Apply imported structure types.
        int importedStructureTypeCount = 0;
        QJsonArray structureTypes = root[QStringLiteral("structure_types")].toArray();
        for(const QJsonValue& val : structureTypes) {
            QJsonObject typeObj = val.toObject();
            QString name = typeObj[QStringLiteral("name")].toString();
            if(name.isEmpty())
                continue;

            QTreeWidgetItem* item = findChildByName(_structureTypesItem, name);
            if(!item) {
                item = new QTreeWidgetItem();
                item->setText(0, name);
                item->setFlags(Qt::ItemFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemNeverHasChildren));
                _structureTypesItem->addChild(item);
            }

            QJsonArray colorArr = typeObj[QStringLiteral("color")].toArray();
            if(colorArr.size() == 3) {
                QColor newColor = QColor::fromRgbF(colorArr[0].toDouble(), colorArr[1].toDouble(), colorArr[2].toDouble());
                QColor oldColor = item->data(1, Qt::DisplayRole).value<QColor>();
                if(significantlyDifferent(newColor.redF(), oldColor.redF()) || significantlyDifferent(newColor.greenF(), oldColor.greenF()) || significantlyDifferent(newColor.blueF(), oldColor.blueF()))
                    item->setData(1, Qt::DisplayRole, QVariant::fromValue(newColor));
            }
            importedStructureTypeCount++;
        }

        int totalCount = importedParticleTypeCount + importedStructureTypeCount;
        MessageDialog(QMessageBox::Information, tr("Theme Imported"),
            tr("Successfully imported settings for %n type(s).", nullptr, totalCount),
            QMessageBox::Ok, settingsDialog()).exec();
    });
}

}	// End of namespace
