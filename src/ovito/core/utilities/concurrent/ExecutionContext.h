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

namespace Ovito {

class OVITO_CORE_EXPORT ExecutionContext
{
public:

    /// The different types of contexts in which the program's actions may be performed.
    enum class Type {
        None,	    	///< No actions should be performed in this context.
        Scripting,		///< Actions are currently performed by a script.
        Interactive		///< Actions are currently performed by the user.
    };

    /// Returns the context the current thread performs its actions in.
    static const ExecutionContext& current() noexcept;

    /// Sets the context the current thread performs its actions in.
    static void setCurrent(const ExecutionContext& context) noexcept;

    /// Returns true if the current operation is performed by the user.
    static bool isInteractive() noexcept { return current().type() == Type::Interactive; }

    /// Returns true if the current operation is performed by a script.
    static bool isScripting() noexcept { return current().type() == Type::Scripting; }

    /// RAII helper class that can be used to temporarily set the current execution context.
    class Scope;

    /// Constructor creating a null execution context.
    ExecutionContext() noexcept = default;

    /// Constructor for a new execution context.
    explicit ExecutionContext(Type type, UserInterface& ui) noexcept : _type(type), _ui(&ui) { OVITO_ASSERT(isValid()); }

    /// Returns whether this context is not of type 'None'.
    bool isValid() const noexcept { return type() != Type::None; }

    /// Returns the type of this execution context.
    Type type() const noexcept { return _type; }

    /// Returns the user interface for this execution context.
    UserInterface& ui() const noexcept { 
        OVITO_ASSERT(isValid());
        OVITO_ASSERT(_ui != nullptr); 
        return *_ui; 
    }

private:

    Type _type = Type::None;
    UserInterface* _ui = nullptr;
};

/// RAII helper class that can be used to temporarily set the current execution context.
class OVITO_CORE_EXPORT ExecutionContext::Scope
{
public:

    /// Constructor.
    explicit Scope(const ExecutionContext& context) noexcept : _previous(ExecutionContext::current()) { ExecutionContext::setCurrent(context); }

    /// Constructor.
    explicit Scope(Type type, UserInterface& ui) noexcept : Scope(ExecutionContext(type, ui)) {}

    /// Destructor.
    ~Scope() { ExecutionContext::setCurrent(_previous); }

private:

    ExecutionContext _previous;
};

}	// End of namespace
