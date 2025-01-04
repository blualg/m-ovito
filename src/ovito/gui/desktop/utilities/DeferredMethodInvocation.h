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
 * Utility class that invokes a member function of an object at some later time.
 * While another invocation is already queued, new calls will be ignored.
 *
 * The DeferredMethodInvocation class can be used to compress rapid update signals
 * into a single call to a widget's repaint method.
 *
 * Two class template parameters must be specified: The QObject derived class to which
 * the member function to be called belongs and the member function pointer.
 */
template<typename ObjectClass, void (ObjectClass::*method)()>
class DeferredMethodInvocation
{
public:

    void operator()(ObjectClass* obj, UserInterface& ui = *this_task::ui()) {
        OVITO_ASSERT(this_task::isMainThread());
        OVITO_ASSERT(QThread::currentThread()->loopLevel() != 0);

        // Unless another call is already underway, post an event to the event queue
        // to invoke the user function.
        if(!_event) {
            _event = new Event(this, obj, ui.shared_from_this());
            QCoreApplication::postEvent(obj, _event);
        }
    }

    ~DeferredMethodInvocation() {
        if(_event) _event->owner = nullptr;
    }

private:

    // A custom event class that can be put into the application's event queue.
    // It calls the user function from its destructor after the event has been
    // fetched from the queue.
    struct Event : public QEvent
    {
        DeferredMethodInvocation* owner;
        ObjectClass* object;
        std::shared_ptr<UserInterface> ui;
        Event(DeferredMethodInvocation* owner, ObjectClass* object, std::shared_ptr<UserInterface> ui) : QEvent(QEvent::None), owner(owner), object(object), ui(std::move(ui)) {
            OVITO_ASSERT(this->ui);
        }
        ~Event() {
            if(owner) {
                OVITO_ASSERT(owner->_event == this);
                owner->_event = nullptr;
                ui->handleExceptions([&]() {
                    (object->*method)();
                });
            }
        }
    };

    Event* _event = nullptr;
};

}   // End of namespace
