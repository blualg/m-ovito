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

#include <ovito/stdobj/gui/StdObjGui.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/dialogs/LoadImageFileDialog.h>
#include <ovito/gui/desktop/properties/FloatParameterUI.h>
#include <ovito/gui/desktop/properties/VectorParameterUI.h>
#include <ovito/gui/desktop/properties/ColorParameterUI.h>
#include <ovito/gui/desktop/properties/BooleanParameterUI.h>
#include <ovito/gui/desktop/properties/CustomParameterUI.h>
#include <ovito/gui/desktop/properties/ModifierDelegateParameterUI.h>
#include <ovito/gui/desktop/dialogs/SaveImageFileDialog.h>
#include <ovito/gui/desktop/utilities/concurrent/ProgressDialog.h>
#include <ovito/gui/base/viewport/ViewportInputMode.h>
#include <ovito/core/app/PluginManager.h>
#include <ovito/core/oo/OvitoClass.h>
#include "PropertyColorMappingEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(PropertyColorMappingEditor);
SET_OVITO_OBJECT_EDITOR(PropertyColorMapping, PropertyColorMappingEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void PropertyColorMappingEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Color mapping"), rolloutParams);

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(2);

    _sourcePropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(PropertyColorMapping::sourceProperty));
    layout1->addWidget(new QLabel(tr("Source property:")));
    layout1->addWidget(_sourcePropertyUI->comboBox());

    _colorGradientList = new QComboBox(rollout);
    layout1->addWidget(new QLabel(tr("Color gradient:")));
    layout1->addWidget(_colorGradientList);
    _colorGradientList->setIconSize(QSize(48,16));
    connect(_colorGradientList, qOverload<int>(&QComboBox::activated), this, &PropertyColorMappingEditor::onColorGradientSelected);
    QVector<OvitoClassPtr> sortedColormapClassList = PluginManager::instance().listClasses(ColorCodingGradient::OOClass());
    std::sort(sortedColormapClassList.begin(), sortedColormapClassList.end(),
        [](OvitoClassPtr a, OvitoClassPtr b) { return QString::localeAwareCompare(a->displayName(), b->displayName()) < 0; });
    for(OvitoClassPtr clazz : sortedColormapClassList) {
        if(clazz == &ColorCodingImageGradient::OOClass() || clazz == &ColorCodingTableGradient::OOClass())
            continue;
        _colorGradientList->addItem(iconFromColorMapClass(clazz), clazz->displayName(), QVariant::fromValue(clazz));
        OVITO_ASSERT(_colorGradientList->findData(QVariant::fromValue(clazz)) >= 0);
    }
    _colorGradientList->insertSeparator(_colorGradientList->count());
    _colorGradientList->addItem(tr("Load custom color map..."));
    _gradientListContainCustomItem = false;

    layout1->addSpacing(10);

    QGridLayout* layout2 = new QGridLayout();
    layout2->setContentsMargins(0,0,0,0);
    layout2->setColumnStretch(1, 1);
    layout1->addLayout(layout2);

    // End value parameter.
    _endValueUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(PropertyColorMapping::endValue));
    layout2->addWidget(_endValueUI->label(), 0, 0);
    layout2->addLayout(_endValueUI->createFieldLayout(), 0, 1);

    // Insert color map display.
    class ColorMapWidget : public QLabel
    {
    public:
        /// Constructor.
        ColorMapWidget(QWidget* parent, PropertyColorMappingEditor* editor) : QLabel(parent), _editor(editor) {}
    protected:
        /// Handle mouse move events.
        virtual void mouseMoveEvent(QMouseEvent* event) override {
            // Display a tooltip indicating the property value that corresponds to the color under the mouse cursor.
            QRect cr = contentsRect();
            FloatType t = FloatType(cr.bottom() - ViewportInputMode::getMousePosition(event).y()) / std::max(1, cr.height() - 1);
            FloatType mappedValue = _editor->computeRangeValue(t);
            QString text = std::isfinite(mappedValue) ? tr("Value: %1").arg(mappedValue) : tr("No value range available");
            QToolTip::showText(ViewportInputMode::getGlobalMousePosition(event).toPoint(), text, this, rect());
            QLabel::mouseMoveEvent(event);
        }
    private:
        PropertyColorMappingEditor* _editor;
    };
    _colorLegendLabel = new ColorMapWidget(rollout, this);
    _colorLegendLabel->setScaledContents(true);
    _colorLegendLabel->setMouseTracking(true);
    layout2->addWidget(_colorLegendLabel, 1, 1);

    // Start value parameter.
    _startValueUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(PropertyColorMapping::startValue));
    layout2->addWidget(_startValueUI->label(), 2, 0);
    layout2->addLayout(_startValueUI->createFieldLayout(), 2, 1);

    // Export color scale button.
    QToolButton* exportBtn = new QToolButton(rollout);
    exportBtn->setIcon(QIcon(":/particles/icons/export_color_scale.png"));
    exportBtn->setToolTip("Export color map to image file");
    exportBtn->setAutoRaise(true);
    exportBtn->setIconSize(QSize(42,22));
    connect(exportBtn, &QPushButton::clicked, this, &PropertyColorMappingEditor::onExportColorScale);
    layout2->addWidget(exportBtn, 1, 0, Qt::AlignCenter);

    // Symmetric range
    BooleanParameterUI* symmetricRangePUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(PropertyColorMapping::symmetricRange));
    layout2->addWidget(symmetricRangePUI->checkBox(), 3, 1);
    connect(symmetricRangePUI->checkBox(), &QCheckBox::toggled, _startValueUI, &FloatParameterUI::setDisabled);

    layout1->addSpacing(8);

    _adjustRangeBtn = new QPushButton(tr("Adjust range"), rollout);
    connect(_adjustRangeBtn, &QPushButton::clicked, this, &PropertyColorMappingEditor::onAdjustRange);
    layout1->addWidget(_adjustRangeBtn);
    layout1->addSpacing(4);

    _reverseRangeBtn = new QPushButton(tr("Reverse range"), rollout);
    connect(_reverseRangeBtn, &QPushButton::clicked, this, &PropertyColorMappingEditor::onReverseRange);
    layout1->addWidget(_reverseRangeBtn);

    // Update color legend if another color mapping object has been loaded into the editor.
    connect(this, &PropertiesEditor::contentsReplaced, this, &PropertyColorMappingEditor::updateColorGradient);
}

/******************************************************************************
* Updates the display for the color gradient.
******************************************************************************/
void PropertyColorMappingEditor::updateColorGradient()
{
    PropertyColorMapping* mod = static_object_cast<PropertyColorMapping>(editObject());
    if(!mod) return;

    // Create the color legend image.
    int legendHeight = 128;
    QImage image(1, legendHeight, QImage::Format_RGB32);
    for(int y = 0; y < legendHeight; y++) {
        FloatType t = (FloatType)y / (legendHeight - 1);
        Color color = mod->colorGradient()->valueToColor(1.0 - t);
        image.setPixel(0, y, QColor(color).rgb());
    }
    _colorLegendLabel->setPixmap(QPixmap::fromImage(image));

    // Select the right entry in the color gradient selector.
    bool isCustomMap = false;
    if(mod->colorGradient()) {
        int index = _colorGradientList->findData(QVariant::fromValue(&mod->colorGradient()->getOOClass()));
        if(index >= 0)
            _colorGradientList->setCurrentIndex(index);
        else
            isCustomMap = true;
    }
    else _colorGradientList->setCurrentIndex(-1);

    if(isCustomMap) {
        if(!_gradientListContainCustomItem) {
            _gradientListContainCustomItem = true;
            _colorGradientList->insertItem(_colorGradientList->count() - 2, iconFromColorMap(mod->colorGradient()), tr("Custom color map"));
            _colorGradientList->insertSeparator(_colorGradientList->count() - 3);
        }
        else {
            _colorGradientList->setItemIcon(_colorGradientList->count() - 3, iconFromColorMap(mod->colorGradient()));
        }
        _colorGradientList->setCurrentIndex(_colorGradientList->count() - 3);
    }
    else if(_gradientListContainCustomItem) {
        _gradientListContainCustomItem = false;
        _colorGradientList->removeItem(_colorGradientList->count() - 3);
        _colorGradientList->removeItem(_colorGradientList->count() - 3);
    }
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool PropertyColorMappingEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ReferenceEvent::ReferenceChanged) {
        if(static_cast<const ReferenceFieldEvent&>(event).field() == PROPERTY_FIELD(PropertyColorMapping::colorGradient)) {
            updateColorGradient();
        }
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* Determines the min/max range of values in the selected input property.
******************************************************************************/
std::optional<std::pair<FloatType, FloatType>> PropertyColorMappingEditor::determineValueRange() const
{
    // Get the color mapping object.
    PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(editObject());
    if(!mapping)
        return {};

    // Get the property container.
    const PropertyContainer* container = _sourcePropertyUI->container();
    if(!container)
        return {};

    // Look up the selected property.
    QString errorDescr;
    auto [pseudoColorProperty, pseudoColorPropertyComponent] = mapping->sourceProperty().findInContainerWithComponent(container, errorDescr);
    if(!pseudoColorProperty)
        return {};

    // Determine min/max value range.
    return mapping->determineValueRange(pseudoColorProperty, pseudoColorPropertyComponent);
}

/******************************************************************************
* Determine the property value corresponding to the given relative position in the range interval.
******************************************************************************/
FloatType PropertyColorMappingEditor::computeRangeValue(FloatType t) const
{
    if(PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(editObject())) {
        return mapping->startValue() + t * (mapping->endValue() - mapping->startValue());
    }
    return std::numeric_limits<FloatType>::quiet_NaN();
}

/******************************************************************************
* Is called when the user selects a color gradient in the list box.
******************************************************************************/
void PropertyColorMappingEditor::onColorGradientSelected(int index)
{
    if(index < 0) return;
    PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(editObject());
    OVITO_CHECK_OBJECT_POINTER(mapping);

    OvitoClassPtr descriptor = _colorGradientList->itemData(index).value<OvitoClassPtr>();
    if(descriptor) {
        performTransaction(tr("Change color gradient"), [&]() {
            OORef<ColorCodingGradient> gradient = static_object_cast<ColorCodingGradient>(descriptor->createInstance());
            if(gradient) {
                mapping->setColorGradient(std::move(gradient));

                QSettings settings;
                settings.beginGroup(PropertyColorMapping::OOClass().plugin()->pluginId());
                settings.beginGroup(PropertyColorMapping::OOClass().name());
                settings.setValue(PROPERTY_FIELD(PropertyColorMapping::colorGradient)->identifier(),
                        QVariant::fromValue(OvitoClass::encodeAsString(descriptor)));
            }
        });
    }
    else if(index == _colorGradientList->count() - 1) {
        performTransaction(tr("Change color gradient"), [&]() {
            LoadImageFileDialog fileDialog(container(), tr("Pick color map image"));
            if(fileDialog.exec()) {
                OORef<ColorCodingImageGradient> gradient = OORef<ColorCodingImageGradient>::create();
                gradient->loadImage(fileDialog.imageInfo().filename());
                mapping->setColorGradient(std::move(gradient));
            }
        });
    }
}

/******************************************************************************
* Is called when the user presses the "Adjust Range" button.
******************************************************************************/
void PropertyColorMappingEditor::onAdjustRange()
{
    performTransaction(tr("Adjust range"), [&]() {
        if(PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(editObject())) {
            if(std::optional<std::pair<FloatType, FloatType>> range = determineValueRange()) {
                if(!mapping->symmetricRange()) {
                    mapping->setStartValue(range->first);
                }
                mapping->setEndValue(range->second);
            }
        }
    });
}

/******************************************************************************
* Is called when the user presses the "Reverse Range" button.
******************************************************************************/
void PropertyColorMappingEditor::onReverseRange()
{
    if(PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(editObject())) {
        performTransaction(tr("Reverse range"), [&]() {
            // Swap start and end value.
            mapping->reverseRange();
        });
    }
}

/******************************************************************************
* Is called when the user presses the "Export color scale" button.
******************************************************************************/
void PropertyColorMappingEditor::onExportColorScale()
{
    PropertyColorMapping* mapping = static_object_cast<PropertyColorMapping>(editObject());
    if(!mapping || !mapping->colorGradient()) return;

    SaveImageFileDialog fileDialog(_colorLegendLabel, tr("Save color map"));
    if(fileDialog.exec()) {

        // Create the color legend image.
        int legendWidth = 32;
        int legendHeight = 256;
        QImage image(1, legendHeight, QImage::Format_RGB32);
        for(int y = 0; y < legendHeight; y++) {
            FloatType t = (FloatType)y / (FloatType)(legendHeight - 1);
            Color color = mapping->colorGradient()->valueToColor(1.0 - t);
            image.setPixel(0, y, QColor(color).rgb());
        }

        QString imageFilename = fileDialog.imageInfo().filename();
        if(!image.scaled(legendWidth, legendHeight, Qt::IgnoreAspectRatio, Qt::FastTransformation).save(imageFilename, fileDialog.imageInfo().format())) {
            mainWindow().reportError(tr("Failed to save image to file '%1'.").arg(imageFilename));
        }
    }
}

/******************************************************************************
* Returns an icon representing the given color map class.
******************************************************************************/
QIcon PropertyColorMappingEditor::iconFromColorMapClass(OvitoClassPtr clazz)
{
    /// Cache icons for color map types.
    static std::map<OvitoClassPtr, QIcon> iconCache;
    if(auto item = iconCache.find(clazz); item != iconCache.end())
        return item->second;

    try {
        // Create a temporary instance of the color map class.
        OORef<ColorCodingGradient> map = static_object_cast<ColorCodingGradient>(clazz->createInstance());
        if(map) {
            QIcon icon = iconFromColorMap(map);
            iconCache.insert(std::make_pair(clazz, icon));
            return icon;
        }
    }
    catch(...) {}

    return QIcon();
}

/******************************************************************************
* Returns an icon representing the given color map.
******************************************************************************/
QIcon PropertyColorMappingEditor::iconFromColorMap(ColorCodingGradient* map)
{
    const int sizex = 48;
    const int sizey = 16;
    QImage image(sizex, sizey, QImage::Format_RGB32);
    for(int x = 0; x < sizex; x++) {
        FloatType t = (FloatType)x / (sizex - 1);
        uint c = QColor(map->valueToColor(t)).rgb();
        for(int y = 0; y < sizey; y++)
            image.setPixel(x, y, c);
    }
    return QIcon(QPixmap::fromImage(image));
}

}   // End of namespace
