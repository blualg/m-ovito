////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "RefMaker.h"

namespace Ovito {

/**
 * \brief A thread-safe container keeping track of all references to a RefTarget object.
 *        This container is only used internally by the RefTarget class.
 */
class OVITO_CORE_EXPORT DependentsList
{
private:

    /// The list of RefMakers that currently hold a reference to this target.
    /// If a RefMaker has multiple references to this target, it will appear only once in this list.
	QVarLengthArray<RefMaker*, 2> _entries;

    using size_type = QVarLengthArray<RefMaker*, 2>::size_type;

#ifdef OVITO_DEBUG
    bool _reentranceFlag = false;
#endif

public:

    /// Invokes the given visitor function for each entry in the list.
    /// It's allowed to modify the list while visiting it.
    template<class Callable>
    void visit_all(Callable&& fn) {
#ifdef OVITO_DEBUG
        OVITO_ASSERT(_reentranceFlag == false);
        _reentranceFlag = true;
#endif

        size_type new_size = 0;
        for(size_type i = 0; i < _entries.size(); i++) {
            if(RefMaker* d = _entries[i]) {
                if(i != new_size) {
                    _entries[new_size] = d;
                    _entries[i] = nullptr;
                }
                new_size++;
                fn(d);
            }
        }
        if(new_size != _entries.size())
            _entries.resize(new_size);

#ifdef OVITO_DEBUG
        _reentranceFlag = false;
#endif
    }

    /// Adds a RefMaker to the list unless it is already in the list.
    void insert(RefMaker* dependent) noexcept {
        OVITO_ASSERT(dependent != nullptr);
        size_type free_slot = -1;
        size_type i = 0;
        for(RefMaker* entry : _entries) {
            if(entry == dependent)
                return;
            else if(free_slot == -1 && entry == nullptr)
                free_slot = i;
            i++;
        }
        if(free_slot == -1)
            _entries.push_back(dependent);
        else
            _entries[free_slot] = dependent;
        OVITO_ASSERT(_entries.size() <= 255);
    }

    /// Removes a RefMaker from the list.
    void remove(RefMaker* dependent) noexcept {
        OVITO_ASSERT(dependent != nullptr);
        auto idx = _entries.indexOf(dependent);
        OVITO_ASSERT(idx >= 0);
        _entries[idx] = nullptr;
    }

    /// Checks if the list currently contains no RefMakers.
    inline bool empty() const {
        for(RefMaker* entry : _entries) {
            if(entry != nullptr)
                return false;
        }
        return true;
    }
};

}   // End of namespace
