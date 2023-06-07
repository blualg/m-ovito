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
#include <ovito/core/dataset/data/DataBuffer.h>

namespace Ovito {


/// Helper class that gets created when direct access to a property's memory is requested
/// from the Python side. The BufferPythonAccessGuard instance exists as long as at least one
/// NumPy view references the property object. During this time, a PropertyContainer
/// protects the DataBuffer from operations that might trigger a reallocation of the memory
/// buffer.
class OVITO_CORE_EXPORT BufferPythonAccessGuard
{
#ifndef OVITO_USE_SYCL

public:
    explicit BufferPythonAccessGuard(DataBuffer& p) : bufferReference(&p), memoryAccessor(&p) {}
    const void* dataPointer() { return memoryAccessor.cdata(); }
private:
    OORef<DataBuffer> bufferReference;
    RawBufferReadAccess memoryAccessor;

#else

    /// When using a SYCL accessor, we don't need to keep the DataBuffer alive.
    /// It's sufficent that the internal SYCL buffer object remains alive while its memory is being accessed.
public:
    using accessor_type = cl::sycl::host_accessor<std::byte, 1, cl::sycl::access_mode::read_write>;
    explicit BufferPythonAccessGuard(DataBuffer& p, BufferPythonAccessGuard** listHead) : memoryAccessor(p.size() != 0 ? accessor_type{p.syclBuffer()} : accessor_type{}), _next(*listHead), _listHead(listHead) {
        if(_next) _next->_prev = this;
        *listHead = this;
    }
    ~BufferPythonAccessGuard() {
        if(_prev == nullptr) {
            *_listHead = _next;
            if(_next) _next->_prev = nullptr;
        }
        else {
            _prev->_next = _next;
            if(_next) _next->_prev = _prev;
        }
    }
    const void* dataPointer() { return !memoryAccessor.empty() ? memoryAccessor.get_pointer() : nullptr; }
private:
    accessor_type memoryAccessor;
    /// Linked list to keep track of all existing instances of this class.
    /// The linked list is maintained by the TaskManager this SYCL accessor is associated with.
    BufferPythonAccessGuard* _next = nullptr;
    BufferPythonAccessGuard* _prev = nullptr;
    BufferPythonAccessGuard** _listHead = nullptr;

    friend class TaskManager;
#endif
};

}   // End of namespace