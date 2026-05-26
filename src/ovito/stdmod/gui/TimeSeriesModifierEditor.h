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

#include <ovito/stdmod/gui/StdModGui.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

namespace Ovito {

class CustomParameterUI;

/**
 * A properties editor for the TimeSeriesModifier class.
 */
class TimeSeriesModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(TimeSeriesModifierEditor)
    Q_OBJECT

protected:
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;

private Q_SLOTS:
    void updateTargetWidgets();
    void updateAttributeList();
    void runSeries();

private:
    void populateAttributeList(const QString& currentValue);

    QPointer<QStackedWidget> _targetStack;
    QPointer<QComboBox> _attributeCombo;
    QPointer<CustomParameterUI> _attributeUI;
    QPointer<QWidget> _reductionWidget;
    QPointer<QPushButton> _runButton;
};

}   // End of namespace
