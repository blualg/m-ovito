////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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

#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/mainwin/MainWindow.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/vulkan/VulkanSceneRenderer.h>
#include "VulkanViewportWindow.h"

namespace Ovito {

OVITO_REGISTER_VIEWPORT_WINDOW_IMPLEMENTATION(VulkanViewportWindow);

/******************************************************************************
* Constructor.
******************************************************************************/
VulkanViewportWindow::VulkanViewportWindow(Viewport* viewport, UserInterface* userInterface, QWidget* parentWidget) : 
        BaseViewportWindow(*userInterface, viewport)
{
    // Create the VulkanContext, a wrapper for the Vulkan logical device.
    // Share the same device with by all viewport windows.
    if(DataSet* dataset = userInterface->datasetContainer().currentSet()) {
        if(ViewportConfiguration* viewportConfig = dataset->viewportConfig()) {
            for(Viewport* vp : viewportConfig->viewports()) {
                if(VulkanViewportWindow* window = dynamic_cast<VulkanViewportWindow*>(vp->window())) {
                    _context = window->context();
                    break;
                }
            }
        }
    }
    if(!_context)
        _context = std::make_shared<VulkanContext>();

    // Release our own Vulkan resources right before the logical device is destroyed.
    connect(_context.get(), &VulkanContext::releaseResourcesRequested, this, &VulkanViewportWindow::reset);
    // Automatically recreate everything in case the logical device is lost.
    connect(_context.get(), &VulkanContext::logicalDeviceLost, this, &VulkanViewportWindow::ensureStarted);

    // Make this a Vulkan compatible window.
    setSurfaceType(QSurface::VulkanSurface);
    setVulkanInstance(context()->vulkanInstance());

    // Embed the QWindow in a QWidget container.
    _widget = QWidget::createWindowContainer(this, parentWidget);

    _widget->setMouseTracking(true);
    _widget->setFocusPolicy(Qt::StrongFocus);
    _widget->setAcceptDrops(false); // File drop events are handled by the root main window. This makes them propagate up the widget hierarchy.

    // Re-render window whenever requested by the system.
    connect(&scenePreparation(), &ScenePreparation::viewportUpdateRequest, this, &VulkanViewportWindow::renderLater);
}

/******************************************************************************
* Puts an update request event for this viewport on the event loop.
******************************************************************************/
void VulkanViewportWindow::renderLater()
{
    // Request a deferred refresh of the QWindow.
    _updateRequested = true;
    if(viewport() && !userInterface().areViewportUpdatesSuspended()) {
        QWindow::requestUpdate();
    }
}

/******************************************************************************
* If an update request is pending for this viewport window, immediately
* processes it and redraw the window contents.
******************************************************************************/
void VulkanViewportWindow::processViewportUpdate()
{
    if(_updateRequested) {
        OVITO_ASSERT_MSG(!userInterface().isRenderingInteractiveViewports(), "VulkanViewportWindow::processUpdateRequest()", "Recursive viewport repaint detected.");

        // Note: All we can do is request a deferred window update. 
        // A QWindow has no way of forcing an immediate repaint.
        QWindow::requestUpdate();
    }
}

/******************************************************************************
* Determines the object that is visible under the given mouse cursor position.
******************************************************************************/
ViewportPickResult VulkanViewportWindow::pick(const QPointF& pos)
{
    ViewportPickResult result;

    // Cannot perform picking while viewport is not visible or currently rendering or when updates are disabled.
    if(isVisible() && !userInterface().isRenderingInteractiveViewports() && !userInterface().areViewportUpdatesSuspended() && pickingRenderer()) {
        try {
            if(pickingRenderer()->isRefreshRequired()) {
                // A dataset is required for rendering.
                if(DataSet* dataset = userInterface().datasetContainer().currentSet()) {
                    // Let the viewport do the actual rendering work.
                    viewport()->renderInteractive(userInterface(), dataset, pickingRenderer());
                }
                else {
                    return result; // Return null result if no dataset is available.
                }
            }

            // Query which object is located at the given window position.
            const QPoint pixelPos = (pos * devicePixelRatio()).toPoint();
            const SceneRenderer::ObjectPickingRecord* objInfo;
            quint32 subobjectId;
            std::tie(objInfo, subobjectId) = pickingRenderer()->objectAtLocation(pixelPos);
            if(objInfo) {
                result.setPipelineNode(objInfo->objectNode);
                result.setPickInfo(objInfo->pickInfo);
                result.setHitLocation(pickingRenderer()->worldPositionFromLocation(pixelPos));
                result.setSubobjectId(subobjectId);
            }
        }
        catch(const Exception& ex) {
            userInterface().reportError(ex);
        }
    }

    return result;
}

/******************************************************************************
* Is called by the window system whenever an area of the window is invalidated, 
* for example due to the exposure in the windowing system changing
******************************************************************************/
void VulkanViewportWindow::exposeEvent(QExposeEvent*)
{
    if(isExposed()) {
        // Create the Vulkan resources when the window is first shown.
        ensureStarted();
    } 
    else {
        // Temporarily release Vulkan resources of the window while is not visible.
        releaseSwapChain();
        reset();
    }
}

/******************************************************************************
* Handles events sent to the window by the system.
******************************************************************************/
bool VulkanViewportWindow::event(QEvent* e)
{
    switch(e->type()) {
    case QEvent::UpdateRequest:
        beginFrame();
        break;
    // The swapchain must be destroyed before the surface as per spec. This is
    // not ideal for us because the surface is managed by the QPlatformWindow
    // which may be gone already when the unexpose comes, making the validation
    // layer scream. The solution is to listen to the PlatformSurface events.
    case QEvent::PlatformSurface:
        if(static_cast<QPlatformSurfaceEvent*>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            releaseSwapChain();
            reset();
        }
        break;
    default:
        break;
    }
    return QWindow::event(e);
}


/******************************************************************************
* Keeps trying to initialize the Vulkan window surface.
******************************************************************************/
void VulkanViewportWindow::ensureStarted()
{
    if(_status == StatusFailRetry)
        _status = StatusUninitialized;
    if(_status == StatusUninitialized) {
        init();
        if(_status == StatusDeviceReady)
            recreateSwapChain();
    }
    if(_status == StatusReady)
        requestUpdate();
}

/******************************************************************************
* Sets the preferred \a formats of the swapchain.
* By default no application-preferred format is set. In this case the
* surface's preferred format will be used or, in absence of that,
* \c{VK_FORMAT_B8G8R8A8_UNORM}.
*
* The list in \a formats is ordered. If the first format is not supported,
* the second will be considered, and so on. When no formats in the list are
* supported, the behavior is the same as in the default case.
* To query the actual format after initialization, call colorFormat().
*
* This function must be called before the window is made visible.
* 
* Reimplementing preInitResources() allows dynamically examining the list of 
* supported formats, should that be desired. There the surface is retrievable via
* QVulkanInstace::surfaceForWindow(), while this function can still safely be
* called to affect the later stages of initialization.
******************************************************************************/
void VulkanViewportWindow::setPreferredColorFormats(const QVector<VkFormat>& formats)
{
    if(_status != StatusUninitialized) {
        qWarning("VulkanViewportWindow: Attempted to set preferred color format when already initialized");
        return;
    }
    _requestedColorFormats = formats;
}

static struct {
    VkSampleCountFlagBits mask;
    int count;
} qvk_sampleCounts[] = {
    // keep this sorted by 'count'
    { VK_SAMPLE_COUNT_1_BIT, 1 },
    { VK_SAMPLE_COUNT_2_BIT, 2 },
    { VK_SAMPLE_COUNT_4_BIT, 4 },
    { VK_SAMPLE_COUNT_8_BIT, 8 },
    { VK_SAMPLE_COUNT_16_BIT, 16 },
    { VK_SAMPLE_COUNT_32_BIT, 32 },
    { VK_SAMPLE_COUNT_64_BIT, 64 }
};

/******************************************************************************
* Initializes the Vulkan objects of the window after it has been exposed for
* first time.  
******************************************************************************/
void VulkanViewportWindow::init()
{
    OVITO_ASSERT(_status == StatusUninitialized);
    qCDebug(lcVulkan, "VulkanViewportWindow init");

    _surface = QVulkanInstance::surfaceForWindow(this);
    if(_surface == VK_NULL_HANDLE) {
        qWarning("VulkanViewportWindow: Failed to retrieve Vulkan surface for window");
        _status = StatusFailRetry;
        return;
    }

    try {
        if(!context()->create(this)) {
            _status = StatusUninitialized;
            qCDebug(lcVulkan, "Attempting to restart in 2 seconds");
            QTimer::singleShot(2000, this, &VulkanViewportWindow::ensureStarted);
            return;
        }

        // Make an extra call to vkGetPhysicalDeviceSurfaceSupportKHR() specifically for this window's surface to make the Vulkan validation layer happy.
        if(!vulkanInstance()->supportsPresent(context()->physicalDevice(), context()->presentQueueFamilyIndex(), this))
            throw Exception(tr("The selected Vulkan queue family does not support presenting to this viewport window."));
    }
    catch(const Exception& ex) {
        userInterface().reportError(ex);
        _status = StatusFail;
        return;
    }

    if(!vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !vkGetPhysicalDeviceSurfaceFormatsKHR) {
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vulkanInstance()->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
        vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vulkanInstance()->getInstanceProcAddr("vkGetPhysicalDeviceSurfaceFormatsKHR"));
        if(!vkGetPhysicalDeviceSurfaceCapabilitiesKHR || !vkGetPhysicalDeviceSurfaceFormatsKHR) {
            qWarning("VulkanViewportWindow: Physical device surface queries not available");
            _status = StatusFail;
            return;
        }
    }
    // Figure out the color format here. Must not wait until recreateSwapChain()
    // because the renderpass should be available already from initResources (so that apps do not have to defer pipeline creation to initSwapChainResources), 
    // but the renderpass needs the final color format.
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context()->physicalDevice(), _surface, &formatCount, nullptr);
    QVector<VkSurfaceFormatKHR> formats(formatCount);
    if(formatCount)
        vkGetPhysicalDeviceSurfaceFormatsKHR(context()->physicalDevice(), _surface, &formatCount, formats.data());
    _colorFormat = VK_FORMAT_B8G8R8A8_UNORM; // our documented default if all else fails
    _colorSpace = VkColorSpaceKHR(0); // this is in fact VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    // Pick the preferred format, if there is one.
    if(!formats.isEmpty() && formats[0].format != VK_FORMAT_UNDEFINED) {
        _colorFormat = formats[0].format;
        _colorSpace = formats[0].colorSpace;
    }
    // Try to honor the user request.
    if(!formats.isEmpty() && !_requestedColorFormats.isEmpty()) {
        for(VkFormat reqFmt : qAsConst(_requestedColorFormats)) {
            auto r = std::find_if(formats.cbegin(), formats.cend(), [reqFmt](const VkSurfaceFormatKHR &sfmt) { return sfmt.format == reqFmt; });
            if(r != formats.cend()) {
                _colorFormat = r->format;
                _colorSpace = r->colorSpace;
                break;
            }
        }
    }
    qCDebug(lcVulkan, "Color format: %d", _colorFormat);

    _defaultRenderPass = context()->createDefaultRenderPass(_colorFormat, _sampleCount);
    if(_defaultRenderPass == VK_NULL_HANDLE)
        return;

    OVITO_ASSERT(!_viewportRenderer);
    OVITO_ASSERT(!_pickingRenderer);

    // Create the interactive viewport renderer. Pass our shared Vulkan device to the renderer.
    _viewportRenderer = OORef<ViewportVulkanSceneRenderer>::create(context());

    // Create the object picking renderer.
    _pickingRenderer = OORef<PickingVulkanSceneRenderer>::create(context(), this);

    _status = StatusDeviceReady;
}

/******************************************************************************
* Recreates the Vulkan swapchain.  
******************************************************************************/
void VulkanViewportWindow::recreateSwapChain()
{
    OVITO_ASSERT(_status >= StatusDeviceReady);
    _swapChainImageSize = size() * devicePixelRatio(); // note: may change below due to surfaceCaps
    if(_swapChainImageSize.isEmpty()) // handle null window size gracefully
        return;
    QVulkanInstance* inst = vulkanInstance();
    QVulkanFunctions* f = inst->functions();
    deviceFunctions()->vkDeviceWaitIdle(logicalDevice());
    if(!vkCreateSwapchainKHR) {
        vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(f->vkGetDeviceProcAddr(logicalDevice(), "vkCreateSwapchainKHR"));
        vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(f->vkGetDeviceProcAddr(logicalDevice(), "vkDestroySwapchainKHR"));
        vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(f->vkGetDeviceProcAddr(logicalDevice(), "vkGetSwapchainImagesKHR"));
        vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(f->vkGetDeviceProcAddr(logicalDevice(), "vkAcquireNextImageKHR"));
        vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(f->vkGetDeviceProcAddr(logicalDevice(), "vkQueuePresentKHR"));
    }
    VkPhysicalDevice physDev = context()->physicalDevice();
    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDev, _surface, &surfaceCaps);
    uint32_t reqBufferCount = _swapChainBufferCount;
    if(surfaceCaps.maxImageCount)
        reqBufferCount = qBound(surfaceCaps.minImageCount, reqBufferCount, surfaceCaps.maxImageCount);
    VkExtent2D bufferSize = surfaceCaps.currentExtent;
    if(bufferSize.width == uint32_t(-1)) {
        Q_ASSERT(bufferSize.height == uint32_t(-1));
        bufferSize.width = _swapChainImageSize.width();
        bufferSize.height = _swapChainImageSize.height();
    } 
    else {
        _swapChainImageSize = QSize(bufferSize.width, bufferSize.height);
    }
    VkSurfaceTransformFlagBitsKHR preTransform =
        (surfaceCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
        : surfaceCaps.currentTransform;
    VkCompositeAlphaFlagBitsKHR compositeAlpha =
        (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
        : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if(requestedFormat().hasAlpha()) {
        if(surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        else if (surfaceCaps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkSwapchainKHR oldSwapChain = _swapChain;
    VkSwapchainCreateInfoKHR swapChainInfo = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    swapChainInfo.surface = _surface;
    swapChainInfo.minImageCount = reqBufferCount;
    swapChainInfo.imageFormat = _colorFormat;
    swapChainInfo.imageColorSpace = _colorSpace;
    swapChainInfo.imageExtent = bufferSize;
    swapChainInfo.imageArrayLayers = 1;
    swapChainInfo.imageUsage = usage;
    swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainInfo.preTransform = preTransform;
    swapChainInfo.compositeAlpha = compositeAlpha;
    swapChainInfo.presentMode = _presentMode;
    swapChainInfo.clipped = true;
    swapChainInfo.oldSwapchain = oldSwapChain;
    qCDebug(lcVulkan, "Creating new swap chain of %d buffers, size %dx%d", reqBufferCount, bufferSize.width, bufferSize.height);
    VkSwapchainKHR newSwapChain;
    VkResult err = vkCreateSwapchainKHR(logicalDevice(), &swapChainInfo, nullptr, &newSwapChain);
    if(err != VK_SUCCESS) {
        qWarning("VulkanViewportWindow: Failed to create swap chain: %d", err);
        return;
    }
    if(oldSwapChain)
        releaseSwapChain();
    _swapChain = newSwapChain;
    uint32_t actualSwapChainBufferCount = 0;
    err = vkGetSwapchainImagesKHR(logicalDevice(), _swapChain, &actualSwapChainBufferCount, nullptr);
    if(err != VK_SUCCESS || actualSwapChainBufferCount < 2) {
        qWarning("VulkanViewportWindow: Failed to get swapchain images: %d (count=%d)", err, actualSwapChainBufferCount);
        return;
    }
    qCDebug(lcVulkan, "Actual swap chain buffer count: %d", actualSwapChainBufferCount);
    if(actualSwapChainBufferCount > MAX_SWAPCHAIN_BUFFER_COUNT) {
        qWarning("VulkanViewportWindow: Too many swapchain buffers (%d)", actualSwapChainBufferCount);
        return;
    }
    _swapChainBufferCount = actualSwapChainBufferCount;
    VkImage swapChainImages[MAX_SWAPCHAIN_BUFFER_COUNT];
    err = vkGetSwapchainImagesKHR(logicalDevice(), _swapChain, &actualSwapChainBufferCount, swapChainImages);
    if(err != VK_SUCCESS) {
        qWarning("VulkanViewportWindow: Failed to get swapchain images: %d", err);
        return;
    }
    if(!context()->createVulkanImage(_swapChainImageSize, 
                              context()->depthStencilFormat(),
                              sampleCountFlagBits(),
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                              VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                              &_dsImage,
                              &_dsMem,
                              &_dsView,
                              1))
    {
        return;
    }
    const bool msaa = sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT;
    VkImage msaaImages[MAX_SWAPCHAIN_BUFFER_COUNT];
    VkImageView msaaViews[MAX_SWAPCHAIN_BUFFER_COUNT];
    if(msaa) {
        if(!context()->createVulkanImage(_swapChainImageSize,
                                  _colorFormat,
                                  sampleCountFlagBits(),
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  msaaImages,
                                  &_msaaImageMem,
                                  msaaViews,
                                  _swapChainBufferCount))
        {
            return;
        }
    }
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT };
    for(int i = 0; i < _swapChainBufferCount; ++i) {
        ImageResources &image(_imageRes[i]);
        image.image = swapChainImages[i];
        if(msaa) {
            image.msaaImage = msaaImages[i];
            image.msaaImageView = msaaViews[i];
        }
        VkImageViewCreateInfo imgViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imgViewInfo.image = swapChainImages[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = _colorFormat;
        imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;
        err = deviceFunctions()->vkCreateImageView(logicalDevice(), &imgViewInfo, nullptr, &image.imageView);
        if(err != VK_SUCCESS) {
            qWarning("VulkanViewportWindow: Failed to create swapchain image view %d: %d", i, err);
            return;
        }
        err = deviceFunctions()->vkCreateFence(logicalDevice(), &fenceInfo, nullptr, &image.cmdFence);
        if(err != VK_SUCCESS) {
            qWarning("VulkanViewportWindow: Failed to create command buffer fence: %d", err);
            return;
        }
        image.cmdFenceWaitable = true; // Fence was created in signaled state
        VkImageView views[3] = { image.imageView,
                                 _dsView,
                                 msaa ? image.msaaImageView : VK_NULL_HANDLE };
        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = _defaultRenderPass;
        fbInfo.attachmentCount = msaa ? 3 : 2;
        fbInfo.pAttachments = views;
        fbInfo.width = _swapChainImageSize.width();
        fbInfo.height = _swapChainImageSize.height();
        fbInfo.layers = 1;
        VkResult err = deviceFunctions()->vkCreateFramebuffer(logicalDevice(), &fbInfo, nullptr, &image.fb);
        if(err != VK_SUCCESS) {
            qWarning("VulkanViewportWindow: Failed to create framebuffer: %d", err);
            return;
        }
        if(context()->separatePresentQueue()) {
            // Pre-build the static image-acquire-on-present-queue command buffer.
            VkCommandBufferAllocateInfo cmdBufInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, context()->presentCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
            err = deviceFunctions()->vkAllocateCommandBuffers(logicalDevice(), &cmdBufInfo, &image.presTransCmdBuf);
            if(err != VK_SUCCESS) {
                qWarning("VulkanViewportWindow: Failed to allocate acquire-on-present-queue command buffer: %d", err);
                return;
            }
            VkCommandBufferBeginInfo cmdBufBeginInfo = {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, nullptr };
            err = deviceFunctions()->vkBeginCommandBuffer(image.presTransCmdBuf, &cmdBufBeginInfo);
            if(err != VK_SUCCESS) {
                qWarning("VulkanViewportWindow: Failed to begin acquire-on-present-queue command buffer: %d", err);
                return;
            }
            VkImageMemoryBarrier presTrans;
            memset(&presTrans, 0, sizeof(presTrans));
            presTrans.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            presTrans.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            presTrans.oldLayout = presTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            presTrans.srcQueueFamilyIndex = context()->graphicsQueueFamilyIndex();
            presTrans.dstQueueFamilyIndex = context()->presentQueueFamilyIndex();
            presTrans.image = image.image;
            presTrans.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            presTrans.subresourceRange.levelCount = presTrans.subresourceRange.layerCount = 1;
            deviceFunctions()->vkCmdPipelineBarrier(image.presTransCmdBuf,
                                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                           0, 0, nullptr, 0, nullptr,
                                           1, &presTrans);
            err = deviceFunctions()->vkEndCommandBuffer(image.presTransCmdBuf);
            if(err != VK_SUCCESS) {
                qWarning("VulkanViewportWindow: Failed to end acquire-on-present-queue command buffer: %d", err);
                return;
            }
        }
    }
    _currentImage = 0;
    VkSemaphoreCreateInfo semInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, nullptr, 0 };
    for(int i = 0; i < renderer()->concurrentFrameCount(); ++i) {
        FrameResources& frame = _frameRes[i];
        frame.imageAcquired = false;
        frame.imageSemWaitable = false;
        deviceFunctions()->vkCreateFence(logicalDevice(), &fenceInfo, nullptr, &frame.fence);
        frame.fenceWaitable = true; // Fence was created in signaled state
        deviceFunctions()->vkCreateSemaphore(logicalDevice(), &semInfo, nullptr, &frame.imageSem);
        deviceFunctions()->vkCreateSemaphore(logicalDevice(), &semInfo, nullptr, &frame.drawSem);
        if(context()->separatePresentQueue())
            deviceFunctions()->vkCreateSemaphore(logicalDevice(), &semInfo, nullptr, &frame.presTransSem);
    }
    _currentFrame = 0;
    _status = StatusReady;
}

/******************************************************************************
* Releases the resources of the Vulkan swapchain.  
******************************************************************************/
void VulkanViewportWindow::releaseSwapChain()
{
    if(!logicalDevice() || !_swapChain) // Do not rely on 'status', a half done init must be cleaned properly too
        return;
    qCDebug(lcVulkan, "Releasing swapchain");
    deviceFunctions()->vkDeviceWaitIdle(logicalDevice());
    for(int i = 0; i < renderer()->concurrentFrameCount(); ++i) {
        FrameResources& frame = _frameRes[i];
        if(frame.fence) {
            if(frame.fenceWaitable)
                deviceFunctions()->vkWaitForFences(logicalDevice(), 1, &frame.fence, VK_TRUE, UINT64_MAX);
            deviceFunctions()->vkDestroyFence(logicalDevice(), frame.fence, nullptr);
            frame.fence = VK_NULL_HANDLE;
            frame.fenceWaitable = false;
        }
        if(frame.imageSem) {
            deviceFunctions()->vkDestroySemaphore(logicalDevice(), frame.imageSem, nullptr);
            frame.imageSem = VK_NULL_HANDLE;
        }
        if(frame.drawSem) {
            deviceFunctions()->vkDestroySemaphore(logicalDevice(), frame.drawSem, nullptr);
            frame.drawSem = VK_NULL_HANDLE;
        }
        if(frame.presTransSem) {
            deviceFunctions()->vkDestroySemaphore(logicalDevice(), frame.presTransSem, nullptr);
            frame.presTransSem = VK_NULL_HANDLE;
        }
    }
    for(int i = 0; i < _swapChainBufferCount; ++i) {
        ImageResources& image = _imageRes[i];
        if(image.cmdFence) {
            if(image.cmdFenceWaitable)
                deviceFunctions()->vkWaitForFences(logicalDevice(), 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
            deviceFunctions()->vkDestroyFence(logicalDevice(), image.cmdFence, nullptr);
            image.cmdFence = VK_NULL_HANDLE;
            image.cmdFenceWaitable = false;
        }
        if(image.fb) {
            deviceFunctions()->vkDestroyFramebuffer(logicalDevice(), image.fb, nullptr);
            image.fb = VK_NULL_HANDLE;
        }
        if(image.imageView) {
            deviceFunctions()->vkDestroyImageView(logicalDevice(), image.imageView, nullptr);
            image.imageView = VK_NULL_HANDLE;
        }
        if(image.cmdBuf) {
            deviceFunctions()->vkFreeCommandBuffers(logicalDevice(), context()->graphicsCommandPool(), 1, &image.cmdBuf);
            image.cmdBuf = VK_NULL_HANDLE;
        }
        if(image.presTransCmdBuf) {
            deviceFunctions()->vkFreeCommandBuffers(logicalDevice(), context()->presentCommandPool(), 1, &image.presTransCmdBuf);
            image.presTransCmdBuf = VK_NULL_HANDLE;
        }
        if(image.msaaImageView) {
            deviceFunctions()->vkDestroyImageView(logicalDevice(), image.msaaImageView, nullptr);
            image.msaaImageView = VK_NULL_HANDLE;
        }
        if(image.msaaImage) {
            deviceFunctions()->vkDestroyImage(logicalDevice(), image.msaaImage, nullptr);
            image.msaaImage = VK_NULL_HANDLE;
        }
    }
    if(_msaaImageMem) {
        deviceFunctions()->vkFreeMemory(logicalDevice(), _msaaImageMem, nullptr);
        _msaaImageMem = VK_NULL_HANDLE;
    }
    if(_dsView) {
        deviceFunctions()->vkDestroyImageView(logicalDevice(), _dsView, nullptr);
        _dsView = VK_NULL_HANDLE;
    }
    if(_dsImage) {
        deviceFunctions()->vkDestroyImage(logicalDevice(), _dsImage, nullptr);
        _dsImage = VK_NULL_HANDLE;
    }
    if(_dsMem) {
        deviceFunctions()->vkFreeMemory(logicalDevice(), _dsMem, nullptr);
        _dsMem = VK_NULL_HANDLE;
    }
    if(_swapChain) {
        vkDestroySwapchainKHR(logicalDevice(), _swapChain, nullptr);
        _swapChain = VK_NULL_HANDLE;
    }
    if(_status == StatusReady)
        _status = StatusDeviceReady;
}

/******************************************************************************
* Starts rendering a frame.
******************************************************************************/
void VulkanViewportWindow::beginFrame()
{
    if(!_swapChain)
        return;

    // Handle window being resized.
    if(size() * devicePixelRatio() != _swapChainImageSize) {
        recreateSwapChain();
        if(!_swapChain)
            return;
    }

    FrameResources& frame = _frameRes[_currentFrame];
    if(!frame.imageAcquired) {
        // Wait if we are too far ahead, i.e. the thread gets throttled based on the presentation rate
        // (note that we are using FIFO mode -> vsync)
        if(frame.fenceWaitable) {
            deviceFunctions()->vkWaitForFences(logicalDevice(), 1, &frame.fence, VK_TRUE, UINT64_MAX);
            deviceFunctions()->vkResetFences(logicalDevice(), 1, &frame.fence);
            frame.fenceWaitable = false;
        }
        // Move on to next swapchain image
        VkResult err = vkAcquireNextImageKHR(logicalDevice(), _swapChain, UINT64_MAX, frame.imageSem, frame.fence, &_currentImage);
        if(err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR) {
            frame.imageSemWaitable = true;
            frame.imageAcquired = true;
            frame.fenceWaitable = true;
        } 
        else if(err == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            requestUpdate();
            return;
        } 
        else {
            if(!context()->checkDeviceLost(err))
                qWarning("VulkanViewportWindow: Failed to acquire next swapchain image: %d", err);
            requestUpdate();
            return;
        }
    }
    
    // Make sure the previous draw for the same image has finished.
    ImageResources& image = _imageRes[_currentImage];
    if(image.cmdFenceWaitable) {
        deviceFunctions()->vkWaitForFences(logicalDevice(), 1, &image.cmdFence, VK_TRUE, UINT64_MAX);
        deviceFunctions()->vkResetFences(logicalDevice(), 1, &image.cmdFence);
        image.cmdFenceWaitable = false;
    }
    // Release resources of the old rendering pass.
    if(image.resourceFrame != 0) {
        context()->releaseResourceFrame(image.resourceFrame);
        image.resourceFrame = 0;
    }

    // Instead of releasing/recreating the draw command buffer, create it only once and reuse it.
    if(image.cmdBuf == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo cmdBufInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, context()->graphicsCommandPool(), VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
        VkResult err = deviceFunctions()->vkAllocateCommandBuffers(logicalDevice(), &cmdBufInfo, &image.cmdBuf);
        if(err != VK_SUCCESS) {
            if(!context()->checkDeviceLost(err))
                qWarning("VulkanViewportWindow: Failed to allocate frame command buffer: %d", err);
            return;
        }
    }
    else {
        // Reset entire command pool.
        VkResult err = deviceFunctions()->vkResetCommandBuffer(image.cmdBuf, 0);
        if(err != VK_SUCCESS) {
            if(!context()->checkDeviceLost(err))
                qWarning("VulkanViewportWindow: Failed to reset frame command buffer: %d", err);
            return;
        }
    }
    VkCommandBufferBeginInfo cmdBufBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    cmdBufBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult err = err = deviceFunctions()->vkBeginCommandBuffer(image.cmdBuf, &cmdBufBeginInfo);
    if(err != VK_SUCCESS) {
        if(!context()->checkDeviceLost(err))
            qWarning("VulkanViewportWindow: Failed to begin frame command buffer: %d", err);
        return;
    }

    // Tell resource manager about the new frame being rendered.
    image.resourceFrame = context()->acquireResourceFrame();

    renderer()->setCurrentSwapChainFrame(currentFrame());
    renderer()->setCurrentCommandBuffer(currentCommandBuffer());
    renderer()->setDefaultRenderPass(defaultRenderPass());
    renderer()->setCurrentResourceFrame(image.resourceFrame);

    const QSize sz = swapChainImageSize();
    renderer()->setFrameBufferSize(sz);

    // A dataset is required for rendering.
    DataSet* dataset = userInterface().datasetContainer().currentSet();

    // Use the viewport's background color to clear frame buffer.
    ColorA backgroundColor{0,0,0,1};
    if(!viewport()->renderPreviewMode())
        backgroundColor = Viewport::viewportColor(ViewportSettings::COLOR_VIEWPORT_BKG);
    else if(dataset && dataset->renderSettings())
        backgroundColor = dataset->renderSettings()->backgroundColorAt(viewport()->scene() ? viewport()->scene()->animationSettings()->currentTime() : userInterface().datasetContainer().currentAnimationTime());

    VkClearColorValue clearColor = {{ (float)backgroundColor.r(), (float)backgroundColor.g(), (float)backgroundColor.b(), (float)backgroundColor.a() }};
    VkClearDepthStencilValue clearDS = { 1, 0 };
    VkClearValue clearValues[3];
    memset(clearValues, 0, sizeof(clearValues));
    clearValues[0].color = clearValues[2].color = clearColor;
    clearValues[1].depthStencil = clearDS;

    VkRenderPassBeginInfo rpBeginInfo;
    memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = defaultRenderPass();
    rpBeginInfo.framebuffer = currentFramebuffer();
    rpBeginInfo.renderArea.extent.width = sz.width();
    rpBeginInfo.renderArea.extent.height = sz.height();
    rpBeginInfo.clearValueCount = (sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT) ? 3 : 2;
    rpBeginInfo.pClearValues = clearValues;
    deviceFunctions()->vkCmdBeginRenderPass(currentCommandBuffer(), &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Invalidate picking buffer every time the visible contents of the viewport change.
    pickingRenderer()->resetPickingBuffer();

    // Mark the window contents as valid again.
    _updateRequested = false;

    // Do not re-enter rendering function of the same viewport.
    if(viewport() && !userInterface().isRenderingInteractiveViewports() && dataset) {
        if(!userInterface().areViewportUpdatesSuspended()) {
            try {
                // Let the Viewport class do the actual rendering work.
                viewport()->renderInteractive(userInterface(), dataset, _viewportRenderer);
            }
            catch(Exception& ex) {
                ex.prependGeneralMessage(tr("An unexpected error occurred while rendering the viewport contents. The program will quit."));
                userInterface().exitWithFatalError(ex);
            }
        }
        else {
            // Make sure viewport gets refreshed as soon as updates are enabled again.
            userInterface().updateViewports();
        }
    }

    deviceFunctions()->vkCmdEndRenderPass(currentCommandBuffer());

    endFrame();
}

/******************************************************************************
* Finishes rendering a frame.
******************************************************************************/
void VulkanViewportWindow::endFrame()
{
    FrameResources& frame = _frameRes[_currentFrame];
    ImageResources& image = _imageRes[_currentImage];
    if(context()->separatePresentQueue()) {
        // Add the swapchain image release to the command buffer that will be
        // submitted to the graphics queue.
        VkImageMemoryBarrier presTrans = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        presTrans.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        presTrans.oldLayout = presTrans.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presTrans.srcQueueFamilyIndex = context()->graphicsQueueFamilyIndex();
        presTrans.dstQueueFamilyIndex = context()->presentQueueFamilyIndex();
        presTrans.image = image.image;
        presTrans.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        presTrans.subresourceRange.levelCount = presTrans.subresourceRange.layerCount = 1;
        deviceFunctions()->vkCmdPipelineBarrier(image.cmdBuf,
                                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                       0, 0, nullptr, 0, nullptr,
                                       1, &presTrans);
    }
    VkResult err = deviceFunctions()->vkEndCommandBuffer(image.cmdBuf);
    if(err != VK_SUCCESS) {
        if(!context()->checkDeviceLost(err))
            qWarning("VulkanViewportWindow: Failed to end frame command buffer: %d", err);
        return;
    }
    // Submit draw calls
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &image.cmdBuf;
    if(frame.imageSemWaitable) {
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &frame.imageSem;
    }
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frame.drawSem;
    VkPipelineStageFlags psf = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    submitInfo.pWaitDstStageMask = &psf;
    Q_ASSERT(!image.cmdFenceWaitable);
    err = deviceFunctions()->vkQueueSubmit(context()->graphicsQueue(), 1, &submitInfo, image.cmdFence);
    if(err == VK_SUCCESS) {
        frame.imageSemWaitable = false;
        image.cmdFenceWaitable = true;
    } 
    else {
        if(!context()->checkDeviceLost(err))
            qWarning("VulkanViewportWindow: Failed to submit to graphics queue: %d", err);
        return;
    }
    if(context()->separatePresentQueue()) {
        // Submit the swapchain image acquire to the present queue.
        submitInfo.pWaitSemaphores = &frame.drawSem;
        submitInfo.pSignalSemaphores = &frame.presTransSem;
        submitInfo.pCommandBuffers = &image.presTransCmdBuf; // must be USAGE_SIMULTANEOUS
        err = deviceFunctions()->vkQueueSubmit(context()->presentQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        if(err != VK_SUCCESS) {
            if(!context()->checkDeviceLost(err))
                qWarning("VulkanViewportWindow: Failed to submit to present queue: %d", err);
            return;
        }
    }
    // Queue present
    VkPresentInfoKHR presInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presInfo.swapchainCount = 1;
    presInfo.pSwapchains = &_swapChain;
    presInfo.pImageIndices = &_currentImage;
    presInfo.waitSemaphoreCount = 1;
    presInfo.pWaitSemaphores = !context()->separatePresentQueue() ? &frame.drawSem : &frame.presTransSem;
    err = vkQueuePresentKHR(context()->graphicsQueue(), &presInfo);
    if(err != VK_SUCCESS) {
        if(err == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            requestUpdate();
            return;
        } 
        else if(err != VK_SUBOPTIMAL_KHR) {
            if(!context()->checkDeviceLost(err))
                qWarning("VulkanViewportWindow: Failed to present: %d", err);
            return;
        }
    }
    frame.imageAcquired = false;
    vulkanInstance()->presentQueued(this);
    renderer()->setCurrentSwapChainFrame(-1);
    renderer()->setCurrentResourceFrame(0);

    // Release command buffer resources from any previously completed frames.
    OVITO_ASSERT(image.resourceFrame != 0);
    for(ImageResources& img : _imageRes) {
        if(img.resourceFrame != 0 && &img != &image) {
            if(img.cmdFenceWaitable) {
                if(deviceFunctions()->vkGetFenceStatus(logicalDevice(), img.cmdFence) != VK_SUCCESS) {
                    continue; // Frame is still in flight.
                }
            }
            context()->releaseResourceFrame(img.resourceFrame);
            img.resourceFrame = 0;
        }
    }

    _currentFrame = (_currentFrame + 1) % renderer()->concurrentFrameCount();
}

/******************************************************************************
* Releases all Vulkan resources.
******************************************************************************/
void VulkanViewportWindow::reset()
{
    if(!logicalDevice()) // Do not rely on 'status', a half done init must be cleaned properly too
        return;
    releaseSwapChain();
    // Release per-swapchain image rendering resources.
    for(ImageResources& frame : _imageRes) {
        if(frame.resourceFrame != 0) {
            context()->releaseResourceFrame(frame.resourceFrame);
            frame.resourceFrame = 0;
        }
    }
    qCDebug(lcVulkan, "VulkanViewportWindow reset");
    deviceFunctions()->vkDeviceWaitIdle(logicalDevice());
    // Release the viewport renderers used by the window.
    _viewportRenderer.reset();
    _pickingRenderer.reset();
    if(_defaultRenderPass) {
        deviceFunctions()->vkDestroyRenderPass(logicalDevice(), _defaultRenderPass, nullptr);
        _defaultRenderPass = VK_NULL_HANDLE;
    }
    _surface = VK_NULL_HANDLE;
    _status = StatusUninitialized;
}

}   // End of namespace
