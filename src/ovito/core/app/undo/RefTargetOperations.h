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


#include <ovito/core/Core.h>
#include <ovito/core/oo/OORef.h>
#include <ovito/core/oo/RefTarget.h>

namespace Ovito {

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
