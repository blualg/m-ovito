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

#include <ovito/pyscript/PyScript.h>
#include <ovito/gui/desktop/properties/PropertiesEditor.h>

class QPushButton;
class QTextEdit;
class QGroupBox;
class QFormLayout;
class QWidget;

namespace PyScript {

using namespace Ovito;

class PythonScriptModifier;

class PythonScriptModifierEditor : public PropertiesEditor
{
    OVITO_CLASS(PythonScriptModifierEditor)
    Q_OBJECT

public:
    Q_INVOKABLE PythonScriptModifierEditor() = default;

protected:
    virtual void createUI(const RolloutInsertionParameters& rolloutParams) override;
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private Q_SLOTS:
    void updateUserInterface();
    void onOpenEditor();
    void onOpenDataInspector();

private:
    struct ParameterWidgetBinding {
        QString name;
        QString kind;
        QWidget* widget = nullptr;
    };

    void rebuildParameterWidgets(PythonScriptModifier* modifier);
    void syncParameterWidgetValues(PythonScriptModifier* modifier);
    void commitParameterValue(PythonScriptModifier* modifier, const QString& name, const QVariant& value);

    QPushButton* _editScriptButton = nullptr;
    QPushButton* _openInspectorButton = nullptr;
    QGroupBox* _parametersGroup = nullptr;
    QFormLayout* _parametersLayout = nullptr;
    QTextEdit* _outputDisplay = nullptr;
    QVariantList _displayedSchema;
    QVector<ParameterWidgetBinding> _parameterBindings;
    QString _lastAutoOpenedExecutionOutput;
};

}  // namespace PyScript
