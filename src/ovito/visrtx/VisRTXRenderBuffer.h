////////////////////////////////////////////////////////////////////////////////////////
//
//  VisRTX renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/RenderBuffer.h>

namespace Ovito {

class OVITO_VISRTXRENDERER_EXPORT VisRTXRenderBuffer : public RenderBuffer
{
    OVITO_CLASS(VisRTXRenderBuffer)

public:

    using RenderBuffer::initializeObject;
};

}   // End of namespace
