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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/app/undo/UndoableTransaction.h>
#include <ovito/gui/desktop/widgets/general/MenuToolButton.h>
#include "PropertiesEditor.h"
#include "ParameterUI.h"

namespace Ovito {

/**
 * \brief Base class for UI components that let the user edit a property of
 *        an object that is stored in a reference field or a property field.
 */
class OVITO_GUI_EXPORT PropertyParameterUI : public ParameterUI
{
    OVITO_CLASS(PropertyParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* parent, const PropertyFieldDescriptor* propField);

    /// Returns the property or reference field being edited.
    const PropertyFieldDescriptor* propertyField() const { return _propField; }

    /// Indicates whether this parameter UI is representing a sub-object property (e.g. an animation controller).
    bool isReferenceFieldUI() const { return _propField->isReferenceField(); }

    /// Indicates whether this parameter UI is representing a PropertyField based property.
    bool isPropertyFieldUI() const { return !_propField->isReferenceField(); }

    /// This method is called when parameter object has been assigned to the reference field of the editable object
    /// this parameter UI is bound to.
    ///
    /// It is also called when the editable object itself has
    /// been replaced in the editor. The parameter UI should react to this change appropriately and
    /// show the property value for the new edit object in the UI. New implementations of this
    /// method must call the base implementation before any other action is taken.
    virtual void resetUI() override;

    /// Returns the menu tool button associated to this PropertyParameterUI
    [[nodiscard]] MenuToolButton* menuToolButton() const { return _menuToolButton; }

    /// Returns the menu tool button associated with this PropertyParameterUI.
    /// Creates a new MenuToolButton if one doesn't exist yet.
    [[nodiscard]] MenuToolButton* createMenuToolButton(QWidget* parent = nullptr);

    /// Adds a new action to the MenuToolButton with the given text and icon.
    /// Also creates the MenuToolButton if it doesn't exist yet.
    [[nodiscard]] QAction* createAction(const QString& text, const QIcon& icon);

    /// Adds a new action to the MenuToolButton that can be used to reset the parameter
    /// managed by this PropertyParameterUI to its default value.
    /// Also creates the MenuToolButton if it doesn't exist yet.
    QAction* createResetAction();

public:

    Q_PROPERTY(Ovito::RefTarget* parameterObject READ parameterObject)

protected Q_SLOTS:

    /// This slot is called when the user has changed the value of the parameter.
    /// It stores the new value in the application's settings store so that it can be used
    /// as the default initialization value next time when a new object of the same class is created.
    void memorizeDefaultParameterValue();

    /// Opens the animation key editor if the parameter managed by this UI class is animatable.
    void openAnimationKeyEditor();

protected:

    /// This method is called when a reference target changes.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

private:

    /// The controller or sub-object whose value is being edited.
    /// This may be \c nullptr either when there is no editable object selected in the parent editor
    /// or if the editable object's reference field is currently empty.
    DECLARE_MODIFIABLE_REFERENCE_FIELD(OORef<RefTarget>, parameterObject, setParameterObject);

    /// The property or reference field being edited.
    const PropertyFieldDescriptor* _propField = nullptr;

    /// The MenuToolButton associated to this PropertyParameterUI.
    QPointer<MenuToolButton> _menuToolButton = nullptr;
};

}   // End of namespace
