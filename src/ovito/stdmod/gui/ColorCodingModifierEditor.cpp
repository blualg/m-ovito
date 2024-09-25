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

#include <ovito/stdmod/gui/StdModGui.h>
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
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include "ColorCodingModifierEditor.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ColorCodingModifierEditor);
SET_OVITO_OBJECT_EDITOR(ColorCodingModifier, ColorCodingModifierEditor);

/******************************************************************************
* Sets up the UI widgets of the editor.
******************************************************************************/
void ColorCodingModifierEditor::createUI(const RolloutInsertionParameters& rolloutParams)
{
    // Create a rollout.
    QWidget* rollout = createRollout(tr("Color coding"), rolloutParams, "manual:particles.modifiers.color_coding");

    // Create the rollout contents.
    QVBoxLayout* layout1 = new QVBoxLayout(rollout);
    layout1->setContentsMargins(4,4,4,4);
    layout1->setSpacing(2);

    ModifierDelegateParameterUI* delegateUI = createParamUI<ModifierDelegateParameterUI>(ColorCodingModifierDelegate::OOClass());
    layout1->addWidget(new QLabel(tr("Operate on:")));
    layout1->addWidget(delegateUI->comboBox());

    _sourcePropertyUI = createParamUI<PropertyReferenceParameterUI>(PROPERTY_FIELD(ColorCodingModifier::sourceProperty));
    layout1->addWidget(new QLabel(tr("Input property:")));
    layout1->addWidget(_sourcePropertyUI->comboBox());
    connect(this, &PropertiesEditor::contentsChanged, this, [this](RefTarget* editObject) {
        // When the modifier's delegate changes, update the list of available input properties.
        ColorCodingModifier* modifier = static_object_cast<ColorCodingModifier>(editObject);
        if(modifier && modifier->delegate())
            _sourcePropertyUI->setContainerRef(modifier->delegate()->inputContainerRef());
        else
            _sourcePropertyUI->setContainerRef({});
    });

    _colorGradientList = new QComboBox(rollout);
    layout1->addWidget(new QLabel(tr("Color gradient:")));
    layout1->addWidget(_colorGradientList);
    _colorGradientList->setIconSize(QSize(48,16));
    connect(_colorGradientList, qOverload<int>(&QComboBox::activated), this, &ColorCodingModifierEditor::onColorGradientSelected);
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

    // Update color legend if another modifier has been loaded into the editor.
    connect(this, &ColorCodingModifierEditor::contentsReplaced, this, &ColorCodingModifierEditor::updateColorGradient);
    // Update editor UI if the modifier's settings change.
    connect(this, &ColorCodingModifierEditor::contentsChanged, this, &ColorCodingModifierEditor::onModifierChanged);

    // Update the start/end parameters display whenever the modifier has been evaluated.
    connect(this, &PropertiesEditor::pipelineOutputChanged, this, &ColorCodingModifierEditor::autoRangeChanged);

    layout1->addSpacing(10);

    QGridLayout* layout2 = new QGridLayout();
    layout2->setContentsMargins(0,0,0,0);
    layout2->setColumnStretch(1, 1);
    layout1->addLayout(layout2);

    // End value parameter.
    _endValueUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorCodingModifier::endValue));
    layout2->addWidget(_endValueUI->label(), 0, 0);
    layout2->addLayout(_endValueUI->createFieldLayout(), 0, 1);

    // Insert color map display.
    class ColorMapWidget : public QLabel
    {
    public:
        /// Constructor.
        ColorMapWidget(QWidget* parent, ColorCodingModifierEditor* editor) : QLabel(parent), _editor(editor) {}
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
        ColorCodingModifierEditor* _editor;
    };
    _colorLegendLabel = new ColorMapWidget(rollout, this);
    _colorLegendLabel->setScaledContents(true);
    _colorLegendLabel->setMouseTracking(true);
    layout2->addWidget(_colorLegendLabel, 1, 1);

    // Start value parameter.
    _startValueUI = createParamUI<FloatParameterUI>(PROPERTY_FIELD(ColorCodingModifier::startValue));
    layout2->addWidget(_startValueUI->label(), 2, 0);
    layout2->addLayout(_startValueUI->createFieldLayout(), 2, 1);

    // Export color scale button.
    QToolButton* exportBtn = new QToolButton(rollout);
    exportBtn->setIcon(QIcon(":/particles/icons/export_color_scale.png"));
    exportBtn->setToolTip("Export color map to image file");
    exportBtn->setAutoRaise(true);
    exportBtn->setIconSize(QSize(42,22));
    connect(exportBtn, &QPushButton::clicked, this, &ColorCodingModifierEditor::onExportColorScale);
    layout2->addWidget(exportBtn, 1, 0, Qt::AlignCenter);

    // Auto-adjust range.
    BooleanParameterUI* autoAdjustRangePUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorCodingModifier::autoAdjustRange));
    layout2->addWidget(autoAdjustRangePUI->checkBox(), 3, 1);
    // Symmetric range
    BooleanParameterUI* symmetricRangePUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorCodingModifier::symmetricRange));
    layout2->addWidget(symmetricRangePUI->checkBox(), 4, 1);

    layout1->addSpacing(8);
    _adjustRangeBtn = new QPushButton(tr("Adjust range"), rollout);
    connect(_adjustRangeBtn, &QPushButton::clicked, this, &ColorCodingModifierEditor::onAdjustRange);
    layout1->addWidget(_adjustRangeBtn);
    layout1->addSpacing(4);
    _adjustRangeGlobalBtn = new QPushButton(tr("Adjust range (all frames)"), rollout);
    connect(_adjustRangeGlobalBtn, &QPushButton::clicked, this, &ColorCodingModifierEditor::onAdjustRangeGlobal);
    layout1->addWidget(_adjustRangeGlobalBtn);
    layout1->addSpacing(4);
    _reverseRangeBtn = new QPushButton(tr("Reverse range"), rollout);
    connect(_reverseRangeBtn, &QPushButton::clicked, this, &ColorCodingModifierEditor::onReverseRange);
    layout1->addWidget(_reverseRangeBtn);

    layout1->addSpacing(8);

    // Only selected particles/bonds.
    BooleanParameterUI* onlySelectedPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorCodingModifier::colorOnlySelected));
    layout1->addWidget(onlySelectedPUI->checkBox());

    // Keep selection
    BooleanParameterUI* keepSelectionPUI = createParamUI<BooleanParameterUI>(PROPERTY_FIELD(ColorCodingModifier::keepSelection));
    layout1->addWidget(keepSelectionPUI->checkBox());
    connect(onlySelectedPUI->checkBox(), &QCheckBox::toggled, keepSelectionPUI, &BooleanParameterUI::setEnabled);
    keepSelectionPUI->setEnabled(false);
}

/******************************************************************************
* Updates the display for the color gradient.
******************************************************************************/
void ColorCodingModifierEditor::updateColorGradient()
{
    ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());
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
bool ColorCodingModifierEditor::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(source == editObject() && event.type() == ReferenceEvent::ReferenceChanged) {
        if(static_cast<const ReferenceFieldEvent&>(event).field() == PROPERTY_FIELD(ColorCodingModifier::colorGradient)) {
            updateColorGradient();
        }
    }
    else if(source == editObject() && event.type() == ReferenceEvent::TargetChanged) {
        if((static_cast<const ReferenceFieldEvent&>(event).field() == PROPERTY_FIELD(ColorCodingModifier::autoAdjustRange))) {
            ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());
            if(!mod->autoAdjustRange() && isUndoRecording()) {
                // When the user turns off the auto-adjust option, adopt the current automatic range
                // as the new user-defined range.
                auto newStartValue = _lastAutoRangeMinValue;
                auto newEndValue = _lastAutoRangeMaxValue; // Note: Copying value, because setStartValue() will overwrite _lastAutoRangeMaxValue.
                if(std::isfinite(newStartValue))
                    mod->setStartValue(newStartValue);
                if(std::isfinite(newEndValue))
                    mod->setEndValue(newEndValue);
            }
        }
    }
    return PropertiesEditor::referenceEvent(source, event);
}

/******************************************************************************
* This method is called whenever the parameters of the ColorCodingModifier change.
******************************************************************************/
void ColorCodingModifierEditor::onModifierChanged()
{
    ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());

    bool enableCustomRangeCtrls = (mod && !mod->autoAdjustRange());
    bool symmetricRange = (mod && mod->symmetricRange());

    _startValueUI->setEnabled(enableCustomRangeCtrls && !symmetricRange);
    _endValueUI->setEnabled(enableCustomRangeCtrls);
    _adjustRangeBtn->setEnabled(enableCustomRangeCtrls);
    _adjustRangeGlobalBtn->setEnabled(enableCustomRangeCtrls);
    _reverseRangeBtn->setEnabled(enableCustomRangeCtrls);
    if(enableCustomRangeCtrls) {
        _startValueUI->spinner()->updateTextBox();
        _endValueUI->spinner()->updateTextBox();
        _lastAutoRangeMinValue = std::numeric_limits<FloatType>::quiet_NaN();
        _lastAutoRangeMaxValue = std::numeric_limits<FloatType>::quiet_NaN();
    }
    else {
        autoRangeChanged();
    }
}

/******************************************************************************
* Is called whenever the modifier has been newly evaluated and has auto-adjusted the value range.
******************************************************************************/
void ColorCodingModifierEditor::autoRangeChanged()
{
    ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());
    if(!mod || !mod->autoAdjustRange())
        return;

    ModificationNode* modNode = modificationNode();
    if(!modNode)
        return;

    handleExceptions([&] {
        // Request the modifier's pipeline output.
        const PipelineFlowState& state = getPipelineOutput();

        QVariant minValue = state.getAttributeValue(modNode, QStringLiteral("ColorCoding.RangeMin"));
        QVariant maxValue = state.getAttributeValue(modNode, QStringLiteral("ColorCoding.RangeMax"));
        if(minValue.isValid()) {
            _lastAutoRangeMinValue = minValue.value<FloatType>();
            _startValueUI->textBox()->setText(_startValueUI->spinner()->unit()->formatValue(_lastAutoRangeMinValue));
        }
        else {
            _lastAutoRangeMinValue = std::numeric_limits<FloatType>::quiet_NaN();
            _startValueUI->textBox()->setText(tr("###"));
        }

        if(maxValue.isValid()) {
            _lastAutoRangeMaxValue = maxValue.value<FloatType>();
            _endValueUI->textBox()->setText(_endValueUI->spinner()->unit()->formatValue(_lastAutoRangeMaxValue));
        }
        else {
            _lastAutoRangeMaxValue = std::numeric_limits<FloatType>::quiet_NaN();
            _endValueUI->textBox()->setText(tr("###"));
        }
    });
}

/******************************************************************************
* Determine the property value corresponding to the given relative position in the range interval.
******************************************************************************/
FloatType ColorCodingModifierEditor::computeRangeValue(FloatType t) const
{
    if(ColorCodingModifier* modifier = static_object_cast<ColorCodingModifier>(editObject())) {
        if(!modifier->autoAdjustRange()) {
            return modifier->startValue() + t * (modifier->endValue() - modifier->startValue());
        }
        else {
            const PipelineFlowState& state = getPipelineOutput();
            if(state) {
                QVariant minValue = state.getAttributeValue(modificationNode(), QStringLiteral("ColorCoding.RangeMin"));
                QVariant maxValue = state.getAttributeValue(modificationNode(), QStringLiteral("ColorCoding.RangeMax"));
                if(minValue.isValid() && maxValue.isValid()) {
                    return minValue.value<FloatType>() + t * (maxValue.value<FloatType>() - minValue.value<FloatType>());
                }
            }
        }
    }
    return std::numeric_limits<FloatType>::quiet_NaN();
}

/******************************************************************************
* Is called when the user selects a color gradient in the list box.
******************************************************************************/
void ColorCodingModifierEditor::onColorGradientSelected(int index)
{
    if(index < 0) return;
    ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());
    OVITO_CHECK_OBJECT_POINTER(mod);

    OvitoClassPtr descriptor = _colorGradientList->itemData(index).value<OvitoClassPtr>();
    if(descriptor) {
        performTransaction(tr("Change color gradient"), [descriptor, mod]() {
            OORef<ColorCodingGradient> gradient = static_object_cast<ColorCodingGradient>(descriptor->createInstance());
            if(gradient) {
                mod->setColorGradient(gradient);

                QSettings settings;
                settings.beginGroup(ColorCodingModifier::OOClass().plugin()->pluginId());
                settings.beginGroup(ColorCodingModifier::OOClass().name());
                settings.setValue(PROPERTY_FIELD(ColorCodingModifier::colorGradient)->identifier(),
                        QVariant::fromValue(OvitoClass::encodeAsString(descriptor)));
            }
        });
    }
    else if(index == _colorGradientList->count() - 1) {
        performTransaction(tr("Change color gradient"), [this, mod]() {
            LoadImageFileDialog fileDialog(container(), tr("Pick color map image"));
            if(fileDialog.exec()) {
                OORef<ColorCodingImageGradient> gradient = OORef<ColorCodingImageGradient>::create();
                gradient->loadImage(fileDialog.imageInfo().filename());
                mod->setColorGradient(gradient);
            }
        });
    }
}

/******************************************************************************
* Is called when the user presses the "Adjust Range" button.
******************************************************************************/
void ColorCodingModifierEditor::onAdjustRange()
{
    ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());
    OVITO_CHECK_OBJECT_POINTER(mod);

    performTransaction(tr("Adjust range"), [&]() {
        ProgressDialog progressDialog(mainWindow(), container(), tr("Determining property value range"));
        mod->adjustRange(currentAnimationTime());
    });
}

/******************************************************************************
* Is called when the user presses the "Adjust range over all frames" button.
******************************************************************************/
void ColorCodingModifierEditor::onAdjustRangeGlobal()
{
    ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());
    OVITO_CHECK_OBJECT_POINTER(mod);

    if(AnimationSettings* anim = mainWindow().datasetContainer().activeAnimationSettings()) {
        performTransaction(tr("Adjust range"), [this, mod, firstFrame=anim->firstFrame(), lastFrame=anim->lastFrame()]() {
            ProgressDialog progressDialog(mainWindow(), container(), tr("Determining property value range"));
            mod->adjustRangeGlobal(firstFrame, lastFrame);
        });
    }
}

/******************************************************************************
* Is called when the user presses the "Reverse Range" button.
******************************************************************************/
void ColorCodingModifierEditor::onReverseRange()
{
    if(ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject())) {
        performTransaction(tr("Reverse range"), [mod]() {
            // Swap controllers for start and end value.
            mod->reverseRange();
        });
    }
}

/******************************************************************************
* Is called when the user presses the "Export color scale" button.
******************************************************************************/
void ColorCodingModifierEditor::onExportColorScale()
{
    ColorCodingModifier* mod = static_object_cast<ColorCodingModifier>(editObject());
    if(!mod || !mod->colorGradient()) return;

    SaveImageFileDialog fileDialog(_colorLegendLabel, tr("Save color map"));
    if(fileDialog.exec()) {

        // Create the color legend image.
        int legendWidth = 32;
        int legendHeight = 256;
        QImage image(1, legendHeight, QImage::Format_RGB32);
        for(int y = 0; y < legendHeight; y++) {
            FloatType t = (FloatType)y / (FloatType)(legendHeight - 1);
            Color color = mod->colorGradient()->valueToColor(1.0 - t);
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
QIcon ColorCodingModifierEditor::iconFromColorMapClass(OvitoClassPtr clazz)
{
    /// Cache icons for color map types.
    static std::map<OvitoClassPtr, QIcon> iconCache;
    if(auto entry = iconCache.find(clazz); entry != iconCache.end())
        return entry->second;

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
QIcon ColorCodingModifierEditor::iconFromColorMap(ColorCodingGradient* map)
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
