////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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


#include <ovito/core/Core.h>
#include <ovito/core/oo/OORef.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

/**
 * \brief This class records a change to a Qt property to a QObject derived class.
 *
 * This UndoableOperation can be used to record
 * a change to a Qt property of an object. The property must defined through the
 * standard Qt mechanism using the \c Q_PROPERTY macro.
 */
class OVITO_CORE_EXPORT SimplePropertyChangeOperation : public UndoableOperation
{
public:

    /// \brief Constructor.
    /// \param obj The object whose property is being changed.
    /// \param propName The identifier of the property that is changed. This is the identifier
    ///                 name given to the property in the \c Q_PROPERTY macro.
    /// \note This class does not make a copy of the property name parameter.
    ///       So the caller should only pass constant string literals to this constructor.
    SimplePropertyChangeOperation(OvitoObject* obj, const char* propName) :
        _object(obj), _propertyName(propName)
    {
        // Make a copy of the current property value.
        _oldValue = _object->property(_propertyName);
        OVITO_ASSERT_MSG(_oldValue.isValid(), "SimplePropertyChangeOperation", "The object does not have a property with the given name.");
    }

    /// \brief Restores the old property value.
    virtual void undo() override {
        // Swap old value and current property value.
        QVariant temp = _object->property(_propertyName);
        _object->setProperty(_propertyName, _oldValue);
        _oldValue = temp;
    }

    virtual QString displayName() const override {
        return QStringLiteral("Set property %1 of %2").arg(_propertyName).arg(_object->getOOClass().name());
    }

private:

    /// The object whose property has been changed.
    OORef<OvitoObject> _object;

    /// The name of the property that has been changed.
    const char* _propertyName;

    /// The old value of the property.
    QVariant _oldValue;
};

/**
 * \brief This undo record simply generates a TargetChanged event for a RefTarget whenever an operation is undone.
 */
class OVITO_CORE_EXPORT TargetChangedUndoOperation : public UndoableOperation
{
public:

    /// \brief Constructor.
    /// \param target The object that is being changed.
    TargetChangedUndoOperation(RefTarget* target) : _target(target) {}

    virtual void undo() override;
    virtual void redo() override {}

    virtual QString displayName() const override {
        return QStringLiteral("Target changed undo operation");
    }

private:

    /// The object that has been changed.
    OORef<RefTarget> _target;
};

/**
 * \brief This undo record simply generates a TargetChanged event for a RefTarget whenever an operation is redone.
 */
class OVITO_CORE_EXPORT TargetChangedRedoOperation : public UndoableOperation
{
public:

    /// \brief Constructor.
    /// \param target The object that is being changed.
    TargetChangedRedoOperation(RefTarget* target) : _target(target) {}

    virtual void undo() override {}
    virtual void redo() override;

    virtual QString displayName() const override {
        return QStringLiteral("Target changed redo operation");
    }

private:

    /// The object that has been changed.
    OORef<RefTarget> _target;
};

}   // End of namespace
