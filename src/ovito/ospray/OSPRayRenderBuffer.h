////////////////////////////////////////////////////////////////////////////////////////
//
//  OSPRay renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/RenderBuffer.h>

namespace Ovito {

class OVITO_OSPRAYRENDERER_EXPORT OSPRayRenderBuffer : public RenderBuffer
{
    OVITO_CLASS(OSPRayRenderBuffer)

public:

    using RenderBuffer::initializeObject;
};

}   // End of namespace
