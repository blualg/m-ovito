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
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/gui/desktop/widgets/rendering/FrameBufferWidget.h>
#include "OpacityFunctionParameterUI.h"

#include <qwt/qwt_plot.h>
#include <qwt/qwt_plot_curve.h>
#include <qwt/qwt_plot_layout.h>
#include <qwt/qwt_scale_widget.h>
#include <qwt/qwt_plot_picker.h>
#include <qwt/qwt_painter.h>
#include <qwt/qwt_picker_machine.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(OpacityFunctionParameterUI);
DEFINE_REFERENCE_FIELD(OpacityFunctionParameterUI, colorMapping);

/******************************************************************************
* Constructor.
******************************************************************************/
void OpacityFunctionParameterUI::initializeObject(PropertiesEditor* parent, const PropertyFieldDescriptor* propField, const PropertyFieldDescriptor* colorMapPropField)
{
    PropertyParameterUI::initializeObject(parent, propField);
    OVITO_ASSERT(isReferenceFieldUI());
    _colorMapPropField = colorMapPropField;

    QwtPainter::setRoundingAlignment(false);
    _plotWidget = new QwtPlot();
    _plotWidget->setCanvasBackground(Qt::white);
    _plotWidget->plotLayout()->setCanvasMargin(4);
    _plotWidget->setAxisTitle(QwtPlot::yLeft, tr("Opacity function"));

#if 0
    // Choose a smaller font size for the axis labels.
    QFont fscl(_plotWidget->fontInfo().family(), 8);
    QFont fttl(_plotWidget->fontInfo().family(), 8, QFont::Bold);
    for(int axisId = 0; axisId < QwtPlot::axisCnt; axisId++) {
        _plotWidget->axisWidget(axisId)->setFont(fscl);
        QwtText text = _plotWidget->axisWidget(axisId)->title();
        text.setFont(fttl);
        _plotWidget->axisWidget(axisId)->setTitle(text);
    }
#endif

    QwtPlotPicker* picker = new QwtPlotPicker(_plotWidget->canvas());
    picker->setTrackerMode(QwtPlotPicker::AlwaysOff);
    picker->setStateMachine(new QwtPickerDragPointMachine());
    connect(picker, qOverload<const QPointF&>(&QwtPlotPicker::appended), this, &OpacityFunctionParameterUI::onPickerPoint);
    connect(picker, qOverload<const QPointF&>(&QwtPlotPicker::moved), this, &OpacityFunctionParameterUI::onPickerPoint);
    connect(picker, &QwtPicker::activated, this, &OpacityFunctionParameterUI::onPickerActivated);
}

/******************************************************************************
* Destructor.
******************************************************************************/
OpacityFunctionParameterUI::~OpacityFunctionParameterUI()
{
    // Release widgets managed by this class.
    delete plotWidget();
}

/******************************************************************************
* This method is called when a reference target changes.
******************************************************************************/
bool OpacityFunctionParameterUI::referenceEvent(RefTarget* source, const ReferenceEvent& event)
{
    if(_colorMapPropField) {
        if(source == editObject() && event.type() == ReferenceEvent::ReferenceChanged) {
            if(_colorMapPropField == static_cast<const ReferenceFieldEvent&>(event).field()) {
                if(editObject()->getReferenceFieldTarget(_colorMapPropField) != colorMapping())
                    resetUI();
            }
        }
        else if(source == colorMapping() && event.type() == ReferenceEvent::TargetChanged) {
            updateUI();
        }
    }
    return PropertyParameterUI::referenceEvent(source, event);
}

/******************************************************************************
* This method is called whenever the child parameter object or the parent object are replaced.
******************************************************************************/
void OpacityFunctionParameterUI::resetUI()
{
    setColorMapping((_colorMapPropField && editObject())
        ? static_object_cast<PropertyColorMapping>(editObject()->getReferenceFieldTarget(_colorMapPropField))
        : nullptr);
    PropertyParameterUI::resetUI();
}

/******************************************************************************
* Refreshes the display of the opacity function in the editor.
******************************************************************************/
void OpacityFunctionParameterUI::updateUI()
{
    if(!plotWidget())
        return;

    if(!_curve) {
        _curve = new QwtPlotCurve();
        _curve->setRenderHint(QwtPlotItem::RenderAntialiased);
        _curve->setStyle(QwtPlotCurve::Lines);
        _curve->setPen(Qt::black, 1);
        _curve->setZ(0);
        _curve->attach(plotWidget());
        plotWidget()->setAxisScale(QwtPlot::yLeft, 0.0, 1.0);
    }

    // Get the opacity function and its tabulated values.
    if(opacityFunction()) {
        float xmin = 0.0f;
        float xmax = 1.0f;
        if(colorMapping()) {
            xmin = colorMapping()->startValue();
            xmax = colorMapping()->endValue();
            if(xmax == xmin)
                xmax = xmin + 1.0f;
            plotWidget()->setAxisScale(QwtPlot::xBottom, xmin, xmax);
        }
        else {
            plotWidget()->setAxisScale(QwtPlot::xBottom, 0.0, 1.0);
        }
        const size_t numSamples = opacityFunction()->optimalTabulationSize();
        std::vector<float> ycoords(numSamples);
        opacityFunction()->tabulateOpacityValues(ycoords);
        std::vector<float> xcoords(numSamples);
        for(size_t i = 0; i < numSamples; ++i)
            xcoords[i] = xmin + (xmax - xmin) * (float)i / (numSamples - 1);
        _curve->setSamples(xcoords.data(), ycoords.data(), numSamples);
        if(colorMapping() && colorMapping()->colorGradient()) {
            QLinearGradient gradient;
            gradient.setCoordinateMode(QGradient::ObjectMode);
            gradient.setStart(0, 0);
            gradient.setFinalStop(1, 0);
            for(size_t i = 0; i < numSamples; ++i) {
                qreal t = (qreal)i / (numSamples - 1);
                const auto color = colorMapping()->colorGradient()->valueToColor(t);
                gradient.setColorAt(t, ColorAT<qreal>(color, ycoords[i]));
            }
            _curve->setBrush(QBrush(gradient));
            plotWidget()->setCanvasBackground(FrameBufferWidget::backgroundBrush());
        }
        else {
            _curve->setBrush(Qt::NoBrush);
            plotWidget()->setCanvasBackground(Qt::white);
        }
        _curve->show();
    }
    else {
        _curve->hide();
    }

    // Workaround for layout bug in QwtPlot:
    plotWidget()->axisWidget(QwtPlot::yLeft)->setBorderDist(1, 1);
    plotWidget()->axisWidget(QwtPlot::yLeft)->setBorderDist(0, 0);

    plotWidget()->replot();
}

/******************************************************************************
* Is called when the user starts or stops picking a location in the plot widget.
******************************************************************************/
void OpacityFunctionParameterUI::onPickerActivated(bool on)
{
    _pickerPoints.clear();
    if(on) {
        if(opacityFunction()) {
            _undoTransaction.begin(mainWindow(), tr("Change opacity function"));
        }
    }
    else {
        if(_undoTransaction.operation()) {
            if(opacityFunction())
                _undoTransaction.commit();
            else
                _undoTransaction.cancel();
        }
    }
}

/******************************************************************************
* Is called when the user picks a location in the plot widget.
******************************************************************************/
void OpacityFunctionParameterUI::onPickerPoint(const QPointF& pt)
{
    QwtInterval interval = plotWidget()->axisInterval(QwtPlot::xBottom);
    FloatType x = pt.x();
    if(FloatType dx = interval.maxValue() - interval.minValue())
        x = (x - interval.minValue()) / dx;
    _pickerPoints.push_back(Point2(x, pt.y()));
    _undoTransaction.revert();
    if(opacityFunction()) {
        performActions(_undoTransaction, [&] {
            mutableOpacityFunction()->freeDraw(_pickerPoints);
        });
    }
}

/******************************************************************************
* Returns the opacity function being edited (mutable).
******************************************************************************/
OpacityFunction* OpacityFunctionParameterUI::mutableOpacityFunction()
{
    const OpacityFunction* func = opacityFunction();
    if(!func)
        return nullptr;
    if(func->isSafeToModify())
        return const_cast<OpacityFunction*>(func);

    DataOORef<OpacityFunction> newFunc = DataOORef<OpacityFunction>::makeCopy(func);
    editObject()->setReferenceFieldTarget(propertyField(), newFunc.get());
    OVITO_ASSERT(opacityFunction() == newFunc);
    newFunc.reset();
    OVITO_ASSERT(opacityFunction()->isSafeToModify());

    return const_cast<OpacityFunction*>(opacityFunction());
}

}   // End of namespace
