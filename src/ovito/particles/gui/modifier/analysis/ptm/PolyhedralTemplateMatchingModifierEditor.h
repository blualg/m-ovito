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

#pragma once


#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

class QwtPlotZoneItem;

namespace Ovito {

/**
 * \brief A properties editor for the PolyhedralTemplateMatchingModifier class.
 */
class PolyhedralTemplateMatchingModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(PolyhedralTemplateMatchingModifierEditor)
    Q_OBJECT

public:

    /// Constructor.
    using PropertiesEditor::PropertiesEditor;

protected Q_SLOTS:

    /// Replots the histogram computed by the modifier.
    void plotHistogram();

protected:

    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private:

    /// The graph widget to display the RMSD histogram.
    DataTablePlotWidget* _rmsdPlotWidget;

    /// Marks the RMSD cutoff in the histogram plot.
    QwtPlotZoneItem* _rmsdRangeIndicator;
};

}   // End of namespace
