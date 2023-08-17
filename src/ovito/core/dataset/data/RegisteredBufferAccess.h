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
/// from the Python side. Instances of the class register themselves with a TaskManager,
/// which allows to shutdown all existing memory accessor before the SYCL queue gets closed.
#ifndef OVITO_USE_SYCL
class OVITO_CORE_EXPORT RegisteredBufferAccess
{
public:

    /// Constructor.
    RegisteredBufferAccess(DataBuffer& buffer) : _accessor(&buffer), _buffer(&buffer) {}

    /// Returns a C pointer to the buffer's internal storage.
    const void* dataPointer() { return _accessor.cdata(); }

    /// Returns a pointer to the buffer object.
    DataBuffer* buffer() const { return _buffer.get(); }

private:

    /// To keep the buffer object alive while it is being accessed.
    OORef<DataBuffer> _buffer;

    /// Internal memory accessor.
    RawBufferReadAccess _accessor;
};
#else
class OVITO_CORE_EXPORT RegisteredBufferAccess
{
public:

    /// The internal SYCL buffer accessor.
    using accessor_type = SYCL_NS::host_accessor<std::byte, 1, SYCL_NS::access_mode::read_write>;

    /// Constructor.
    RegisteredBufferAccess(DataBuffer& buffer, TaskManager& taskManager) : _buffer(&buffer), _syclAccessor(buffer.size() != 0 ? accessor_type{buffer.syclBuffer()} : accessor_type{}), _next(taskManager._registeredBufferAccessors), _listHead(&taskManager._registeredBufferAccessors) {
        if(_next) _next->_prev = this;
        *_listHead = this;
    }

    /// Returns a pointer to the buffer object.
    DataBuffer* buffer() const { return _buffer.get(); }

    ~RegisteredBufferAccess() {
        if(_prev == nullptr) {
            *_listHead = _next;
            if(_next) _next->_prev = nullptr;
        }
        else {
            _prev->_next = _next;
            if(_next) _next->_prev = _prev;
        }
    }

    /// Returns a C pointer to the buffer's internal storage.
    const void* dataPointer() { return !_syclAccessor.empty() ? _syclAccessor.get_pointer() : nullptr; }

private:

    /// To keep the buffer object alive while it is being accessed.
    OORef<DataBuffer> _buffer;

    /// SYCL host memory accessor.
    accessor_type _syclAccessor;

    /// Linked list to keep track of all existing instances of this class.
    /// The linked list is maintained by the TaskManager this SYCL accessor is associated with.
    RegisteredBufferAccess* _next = nullptr;
    RegisteredBufferAccess* _prev = nullptr;
    RegisteredBufferAccess** _listHead = nullptr;

    friend class TaskManager;
};
#endif

}   // End of namespace