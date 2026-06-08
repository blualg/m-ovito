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

#pragma once

#include <ovito/particles/gui/ParticlesGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <ovito/stdobj/gui/widgets/DataTablePlotWidget.h>

namespace Ovito {

class GeometryDescriptorModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(GeometryDescriptorModifierEditor)
    Q_OBJECT

protected:
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:
    void updateModeControls();
    void plotValues();

private:
    QWidget* _atom1Label = nullptr;
    QWidget* _atom1Widget = nullptr;
    QWidget* _atom2Label = nullptr;
    QWidget* _atom2Widget = nullptr;
    QWidget* _atom3Label = nullptr;
    QWidget* _atom3Widget = nullptr;
    QWidget* _atom4Label = nullptr;
    QWidget* _atom4Widget = nullptr;
    QWidget* _templateWidget = nullptr;
    QWidget* _templateAtom3Row = nullptr;
    QWidget* _templateAtom4Row = nullptr;
    QWidget* _normalizeWidget = nullptr;
    DataTablePlotWidget* _plotWidget = nullptr;
};

}  // namespace Ovito
