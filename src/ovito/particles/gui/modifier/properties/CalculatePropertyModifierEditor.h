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
#include <ovito/gui/desktop/properties/StringParameterUI.h>
#include <ovito/gui/desktop/properties/VariantComboBoxParameterUI.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>
#include <QLabel>
#include <QWidget>

namespace Ovito {

/**
 * A properties editor for the CalculatePropertyModifier class.
 */
class CalculatePropertyModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(CalculatePropertyModifierEditor)
    Q_OBJECT

protected:
    /// Creates the user interface controls for the editor.
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:
    void updateVisibleControls();

private:
    QWidget* _selectorWidget = nullptr;
    QLabel* _selectorDescriptionLabel = nullptr;
    QWidget* _outputWidget = nullptr;
    QWidget* _expressionWidget = nullptr;
    QWidget* _vectorExpressionWidget = nullptr;
    QWidget* _groupingWidget = nullptr;
    QWidget* _reductionWidget = nullptr;
};

}  // namespace Ovito
