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
#include "PropertiesEditor.h"

namespace Ovito {

/**
 * \brief Base class for UI components that allow the user to edit a parameter
 *        of a RefTarget derived object in the PropertiesEditor.
 */
class OVITO_GUI_EXPORT ParameterUI : public QObject, public RefMaker
{
    OVITO_CLASS(ParameterUI)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(PropertiesEditor* editor);

    /// Returns the properties editor hosting this component.
    PropertiesEditor* editor() const { return _editor; }

    /// Returns the main window that is hosting this parameter UI.
    MainWindow& mainWindow() const { return editor()->mainWindow(); }

    /// Returns the current animation time.
    std::optional<AnimationTime> currentAnimationTime() const { return editor()->currentAnimationTime(); }

    /// Returns the enabled state of the component.
    bool isEnabled() const { return _enabled; }

    /// Returns the disabled state of the component. This is simply the inverse of the enabled state.
    bool isDisabled() const { return !isEnabled(); }

    /// Executes a functor and catches any exceptions thrown during its execution.
    /// If an exception is thrown by the functor, the error message is displayed to the user and this function returns false.
    template<typename Function>
    bool handleExceptions(Function&& func) const {
        return editor()->handleExceptions(std::forward<Function>(func));
    }

    /// Executes a functor provided by the caller that performs undoable actions in an interactive context.
    /// If an exception is thrown by the functor, the error message is displayed
    /// to the user, and this function returns false.
    template<typename Function>
    bool performActions(UndoableTransaction& transaction, Function&& func) {
        return editor()->performActions(transaction, std::forward<Function>(func));
    }

    /// Executes the passed functor and catches any exceptions thrown during its execution.
    /// If an exception is thrown by the functor, all data changes performed by the functor
    /// so far will be undone and an error message is shown to the user.
    template<typename Function>
    bool performTransaction(const QString& undoOperationName, Function&& func) const {
        return editor()->performTransaction(undoOperationName, std::forward<Function>(func));
    }

public:

    Q_PROPERTY(Ovito::RefTarget* editObject READ editObject)
    Q_PROPERTY(bool isEnabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(bool isDisabled READ isDisabled WRITE setDisabled)

Q_SIGNALS:

    /// This signal is emitted when the user is changing the value of the parameter by manipulating the UI widget.
    /// It is not emitted when the parameter value has been changed programmatically.
    void valueEntered();

public Q_SLOTS:

    /// \brief This method is called when a new editable object has been assigned to the properties owner
    ///       this parameter UI belongs to.
    ///
    /// The parameter UI should react to this change appropriately and
    /// show the properties value for the new edit object in the UI. The default implementation
    /// of this method just calls updateUI() to reflect the change.
    virtual void resetUI() { updateUI(); }

    /// \brief This method updates the displayed value of the parameter UI.
    ///
    /// This method should be overridden by derived classes.
    virtual void updateUI() {}

    /// \brief Sets the enabled state of the UI.
    virtual void setEnabled(bool enabled) { _enabled = enabled; }

    /// \brief Sets the enabled state of the UI. This is just the reverse of setEnabled().
    void setDisabled(bool disabled) { setEnabled(!disabled); }

    /// \brief Sets the object whose property is being displayed in this parameter UI.
    virtual void setEditObject(RefTarget* newObject) {
        _editObject.set(this, PROPERTY_FIELD(editObject), newObject);
        resetUI();
    }

private:

    /// The object whose parameters are being edited.
    DECLARE_REFERENCE_FIELD(RefTarget*, editObject);

    /// The editor hosting this parameter UI.
    PropertiesEditor* _editor;

    /// Indicates whether this UI component is currently enabled or disabled.
    bool _enabled = true;
};

}   // End of namespace
