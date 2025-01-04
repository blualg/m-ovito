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

/******************************************************************************
 * \brief A thread-specific container for objects of type T.
 *
 * This class allows to create multiple objects of type T, one for each thread
 * from which the create() method is called. The objects are stored in a map
 * that is indexed by the thread ID.
******************************************************************************/
template<typename T>
class EnumerableThreadSpecific
{
public:

    template<typename... Args>
    T& create(Args&&... args) {
        std::lock_guard<std::mutex> lock(_mutex);
        return _data.try_emplace(std::this_thread::get_id(), std::forward<Args>(args)...).first->second;
    }

    template<typename Function>
    void visitEach(Function&& function) {
        std::lock_guard<std::mutex> lock(_mutex);
        for(auto& entry : _data)
            function(entry.second);
    }

private:

    std::map<std::thread::id, T> _data;
    std::mutex _mutex;
};

}   // End of namespace
