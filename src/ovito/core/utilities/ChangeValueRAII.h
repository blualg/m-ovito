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

namespace Ovito {

/**
 * Utility class that temporarily replaces the value of a variable, making
 * sure that its old value gets restored upon deconstruction of the RAII helper.
 */
template<typename T>
class ChangeValueRAII
{
public:

    /// Constructor.
    explicit ChangeValueRAII(T& storage, T&& newValue) : _storage(storage), _oldValue(std::exchange(storage, std::move(newValue))) {}

    /// Destructor.
    ~ChangeValueRAII() { _storage = std::move(_oldValue); }

    /// No copying allowed.
    ChangeValueRAII(const ChangeValueRAII& other) = delete;
    ChangeValueRAII& operator=(const ChangeValueRAII& other) = delete;

private:

    T& _storage;
    T _oldValue;
};

/**
 * Utility class that temporarily exchanges the values of two variables, making
 * sure that their old values get restored upon deconstruction of the RAII helper.
 */
template<typename T>
class SwapValueRAII
{
public:

    /// Constructor.
    explicit SwapValueRAII(T& storage1, T& storage2) : _storage1(storage1), _storage2(storage2) {
        using namespace std;
        swap(storage1, storage2);
    }

    /// Destructor.
    ~SwapValueRAII() {
        using namespace std;
        swap(_storage1, _storage2);
    }

    /// No copying allowed.
    SwapValueRAII(const SwapValueRAII& other) = delete;
    SwapValueRAII& operator=(const SwapValueRAII& other) = delete;

private:

    T& _storage1;
    T& _storage2;
};

}   // End of namespace
