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
#include <ovito/gui/desktop/mainwin/data_inspector/DataInspectionApplet.h>
#include <ovito/stdobj/simcell/SimulationCell.h>

namespace Ovito {

/**
 * \brief Data inspector page for global attribute values.
 */
class SimulationCellInspectionApplet : public DataInspectionApplet
{
    OVITO_CLASS(SimulationCellInspectionApplet)
    Q_OBJECT

public:
    /// Constructor.
    void initializeObject() { DataInspectionApplet::initializeObject(SimulationCell::OOClass()); }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 99; }

    /// Determines whether the given pipeline flow state contains data that can be displayed by this applet.
    virtual bool appliesTo(const DataCollection& data) override;

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Updates the contents displayed in the inspector.
    virtual void updateDisplay() override;

    /// Returns the help topic ID for the documentation page of this applet.
    virtual QString helpTopicId() const override { return QStringLiteral("manual:data_inspector.simulation_cell"); }

private:

    /// Displays the dimensionality of the simulation cell.
    QLabel* _dimensionalityDisplay = nullptr;

    /// Simulation cell pbc flags
    std::array<QLabel*, 3> _checkboxPBC;

    /// Simulation cell cell vectors and origin.
    std::array<std::array<QLineEdit*, 3>, 4> _cellVectorFields;

    /// Simulation cell vectors length and angles (cell parameters).
    std::array<std::array<QLineEdit*, 3>, 2> _cellParamsFields;

    /// Simulation cell bounding box size.
    std::array<QLineEdit*, 3> _bboxFields;

    /// Two widget palettes for indicating PBC flags.
    QPalette _pbcEnabledColor, _pbcDisabledColor;
};

}  // namespace Ovito
