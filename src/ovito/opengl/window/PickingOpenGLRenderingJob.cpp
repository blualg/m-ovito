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

#include <ovito/core/Core.h>
#include "PickingOpenGLRenderingJob.h"

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(PickingOpenGLRenderingJob);

/******************************************************************************
* Returns an instance of this rendering job class that is shared among all viewport windows.
******************************************************************************/
OORef<PickingOpenGLRenderingJob> PickingOpenGLRenderingJob::createSharedInstance(std::shared_ptr<RendererResourceCache> visCache, OORef<const OpenGLRenderer> renderer)
{
    static OOWeakRef<PickingOpenGLRenderingJob> sharedJob;
    OORef<PickingOpenGLRenderingJob> job = sharedJob.lock();
    if(!job) {
        job = OORef<PickingOpenGLRenderingJob>::create(std::move(visCache), std::move(renderer));
        sharedJob = job;
    }
    else {
        OVITO_ASSERT(job->visCache() == visCache);
        OVITO_ASSERT(job->sceneRenderer() == renderer);
    }
    return job;
}

}   // End of namespace
