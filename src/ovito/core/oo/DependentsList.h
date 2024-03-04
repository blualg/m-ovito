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

    using size_type = QVarLengthArray<RefMaker*, 2>::size_type;

    /// The list of RefMakers that currently hold a reference to this target.
    /// If a RefMaker has multiple references to this target, it will appear only once in this list.
	QVarLengthArray<RefMaker*, 2> _entries;

    /// Keeps track of the number of nested calls to visit().
    int _reentranceCounter = 0;

    /// Returns the mutex to be used for thread-safe access to the list.
    inline std::mutex& mutex() const {
        static std::mutex _mutexPool[131];
        return _mutexPool[reinterpret_cast<quintptr>(this) % (sizeof(_mutexPool)/sizeof(std::mutex))];
    }

public:

    /// Invokes the given visitor function for each entry in the list.
    /// It's allowed to modify the list while visiting it.
    template<class Callable>
    void visit(Callable&& fn) noexcept {
        std::unique_lock<std::mutex> lock(mutex());
        _reentranceCounter++;
#ifdef OVITO_DEBUG
        try {
        auto const originalSize = _entries.size();
#endif
        // Visit all entries in the list. Skip nullptr entries. The length of the list
        // may grow (but not shrink) during the iteration process.
        bool containsEmptySlots = false;
        for(size_type i = 0; i < _entries.size(); i++) {
            if(RefMaker* d = _entries[i]) {
                lock.unlock();
                fn(d);
                lock.lock();
            }
            else {
                containsEmptySlots = true;
            }
        }

        OVITO_ASSERT(originalSize <= _entries.size());

        // Compact the list if necessary.
        // But only do it if we are not currently visiting the list recursively or concurrently.
        if(--_reentranceCounter == 0 && containsEmptySlots) {
            _entries.removeAll(nullptr);
        }

#ifdef OVITO_DEBUG
        }
        catch(...) {
            OVITO_ASSERT_MSG(false, "RefTarget::visitDependents()", "Exception thrown by visitor function.");
        }
#endif
    }

    /// Adds a RefMaker to the list unless it is already in the list.
    void insert(RefMaker* dependent) noexcept {
        OVITO_ASSERT(dependent != nullptr);
        RefMaker** free_slot = nullptr;
        std::lock_guard<std::mutex> lock(mutex());
        for(auto& entry : _entries) {
            if(entry == dependent)
                return;
            else if(entry == nullptr && free_slot == nullptr)
                free_slot = &entry;
        }
        if(free_slot == nullptr)
            _entries.push_back(dependent);
        else
            *free_slot = dependent;
        OVITO_ASSERT(_entries.size() <= 255);
    }

    /// Removes a RefMaker from the list.
    void remove(RefMaker* dependent) noexcept {
        OVITO_ASSERT(dependent != nullptr);
        std::lock_guard<std::mutex> lock(mutex());
        auto idx = _entries.indexOf(dependent);
        OVITO_ASSERT(idx >= 0);
        _entries.replace(idx, nullptr);
        OVITO_ASSERT(!_entries.contains(dependent));
    }

#ifdef OVITO_DEBUG
    /// Checks if the list currently contains no RefMakers.
    inline bool empty() const {
        std::lock_guard<std::mutex> lock(mutex());
        for(RefMaker* entry : _entries) {
            if(entry != nullptr)
                return false;
        }
        return true;
    }
#endif
};

}   // End of namespace
