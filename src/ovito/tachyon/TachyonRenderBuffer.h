////////////////////////////////////////////////////////////////////////////////////////
//
//  Tachyon renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ovito/core/Core.h>
#include <ovito/core/rendering/RenderBuffer.h>

namespace Ovito {

class OVITO_TACHYONRENDERER_EXPORT TachyonRenderBuffer : public RenderBuffer
{
    OVITO_CLASS(TachyonRenderBuffer)

public:

    using RenderBuffer::initializeObject;
};

}   // End of namespace
