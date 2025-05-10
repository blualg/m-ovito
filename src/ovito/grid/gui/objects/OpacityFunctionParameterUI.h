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

#pragma once

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/properties/PropertyParameterUI.h>
#include <ovito/core/rendering/OpacityFunction.h>
#include <ovito/stdobj/properties/PropertyColorMapping.h>

#include <qwt/qwt_plot.h>
#include <qwt/qwt_scale_draw.h>
#include <qwt/qwt_text.h>

class QwtPlotCurve;

namespace Ovito {

/**
 * \brief A editor widget for the OpacityFunction class.
 */
class OpacityFunctionParameterUI : public PropertyParameterUI
{
    OVITO_CLASS(OpacityFunctionParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parent, const PropertyFieldDescriptor* propField, const PropertyFieldDescriptor* colorMapPropField = nullptr);

    /// Destructor.
    ~OpacityFunctionParameterUI();

    /// Returns the opacity function being edited (const/read-only).
    const OpacityFunction* opacityFunction() const { return static_object_cast<OpacityFunction>(parameterObject()); }

    /// Returns the opacity function being edited (mutable).
    OpacityFunction* mutableOpacityFunction();

    /// The plot widget.
    QwtPlot* plotWidget() const { return _plotWidget; }

    /// This method is called whenever the child parameter object or the parent object are replaced.
    virtual void resetUI() override;

    /// Updates the displayed value of the parameter UI.
    virtual void updateUI() override;

private Q_SLOTS:

    /// Is called when the user starts or stops picking a location in the plot widget.
    void onPickerActivated(bool on);

    /// Is called when the user picks a location in the plot widget.
    void onPickerPoint(const QPointF& pt);

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The pseudo-color mapping.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<PropertyColorMapping>, colorMapping, setColorMapping);

    /// The plot widget.
    QPointer<QwtPlot> _plotWidget;

    /// The plotted function.
    QwtPlotCurve* _curve = nullptr;

    /// Used to make changes to the opacity function reversible.
    UndoableTransaction _undoTransaction;

    /// The sequence of points drawn by the user using the mouse.
    std::vector<Point2> _pickerPoints;

    /// The property field that contains the color map.
    const PropertyFieldDescriptor* _colorMapPropField = nullptr;
};

}   // End of namespace
