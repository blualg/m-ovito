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

#pragma once


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/base/viewport/BaseViewportWindow.h>
#include <ovito/vulkan/gui/ViewportVulkanSceneRenderer.h>
#include <ovito/vulkan/gui/PickingVulkanSceneRenderer.h>

#include <QWindow>

namespace Ovito {

/**
 * \brief A viewport window implementation that is based on Vulkan.
 */
class OVITO_VULKANRENDERERGUI_EXPORT VulkanViewportWindow : public QWindow, public BaseViewportWindow
{
    Q_OBJECT

public:

    /// Constructor.
    Q_INVOKABLE VulkanViewportWindow(Viewport* vp, UserInterface* userInterface, QWidget* parentWidget);

    /// Returns the QWidget that is associated with this viewport window.
    virtual QWidget* widget() override { return _widget; }

    /// Returns the interactive scene renderer used by the viewport window to render the graphics.
    virtual SceneRenderer* sceneRenderer() const override { return _viewportRenderer; }

    /// If an update request is pending for this viewport window, immediately
    /// processes it and redraw the window contents.
    virtual void processViewportUpdate() override;

    /// Sets the mouse cursor shape for the window. 
    virtual void setCursor(const QCursor& cursor) override { QWindow::setCursor(cursor); }

    /// Returns the current size of the viewport window (in device pixels).
    virtual QSize viewportWindowDeviceSize() override {
        return QWindow::size() * QWindow::devicePixelRatio();
    }

    /// Returns the current size of the viewport window (in device-independent pixels).
    virtual QSize viewportWindowDeviceIndependentSize() override {
        return QWindow::size();
    }

    /// Returns the device pixel ratio of the viewport window's canvas.
    virtual qreal devicePixelRatio() override {
        return QWindow::devicePixelRatio();
    }

    /// Lets the viewport window delete itself.
    /// This is called by the Viewport class destructor.
    virtual void destroyViewportWindow() override {
        _widget->deleteLater();
        this->deleteLater();
        BaseViewportWindow::destroyViewportWindow();
    }

    /// Returns the current position of the mouse cursor relative to the viewport window.
    virtual QPoint getCurrentMousePos() override { return _widget->mapFromGlobal(QCursor::pos()); }

    /// Returns whether the viewport window is currently visible on screen.
    virtual bool isVisible() const override { return _widget->isVisible(); }

    /// Returns the renderer generating an offscreen image of the scene used for object picking.
    PickingVulkanSceneRenderer* pickingRenderer() const { return _pickingRenderer; }

    /// Determines the object that is located under the given mouse cursor position.
    virtual ViewportPickResult pick(const QPointF& pos) override;

    /// Sets the preferred \a formats of the swapchain.
    void setPreferredColorFormats(const QVector<VkFormat>& formats);

    /// Returns a typical render pass with one sub-pass.
    VkRenderPass defaultRenderPass() const { return _defaultRenderPass; }

    /// Returns the color buffer format used by the swapchain.
    VkFormat colorFormat() const { return _colorFormat; }

    /// Returns the image size of the swapchain.
    /// This usually matches the size of the window, but may also differ in case vkGetPhysicalDeviceSurfaceCapabilitiesKHR reports a fixed size.
    QSize swapChainImageSize() const { return _swapChainImageSize; }

    /// Returns the current frame index in the range [0, concurrentFrameCount() - 1].
    int currentFrame() const { return _currentFrame; }

    /// Returns the active command buffer for the current swap chain image.
    VkCommandBuffer currentCommandBuffer() const { return _imageRes[_currentImage].cmdBuf; }

    /// Returns a VkFramebuffer for the current swapchain image using the default render pass.
    VkFramebuffer currentFramebuffer() const { return _imageRes[_currentImage].fb; }    

    /// Returns the current sample count as a \c VkSampleCountFlagBits value.
    /// When targeting the default render target, the \c rasterizationSamples field
    /// of \c VkPipelineMultisampleStateCreateInfo must be set to this value.
    VkSampleCountFlagBits sampleCountFlagBits() const { return _sampleCount; }

    /// Returns the Vulkan logical device handle.
    VkDevice logicalDevice() const { return context()->logicalDevice(); }

    /// Returns the device-specific Vulkan function table. 
    QVulkanDeviceFunctions* deviceFunctions() const { return context()->deviceFunctions(); }

    /// Returns the logical Vulkan device used by the window.
    const std::shared_ptr<VulkanContext>& context() const { return _context; }

    /// Returns the renderer of the interactive viewport window.
    ViewportVulkanSceneRenderer* renderer() const { return _viewportRenderer; }

public Q_SLOTS:

    /// \brief Puts an update request for this window in the event loop.
    virtual void renderLater() override;

protected:

    /// Handles events sent to the window by the system.
    virtual bool event(QEvent* e) override;

    /// Is called by the window system whenever an area of the window is invalidated, 
    /// for example due to the exposure in the windowing system changing
    virtual void exposeEvent(QExposeEvent* event) override;

    /// Handles double click events.
    virtual void mouseDoubleClickEvent(QMouseEvent* event) override { BaseViewportWindow::mouseDoubleClickEvent(event); }

    /// Handles mouse press events.
    virtual void mousePressEvent(QMouseEvent* event) override { BaseViewportWindow::mousePressEvent(event); }

    /// Handles mouse release events.
    virtual void mouseReleaseEvent(QMouseEvent* event) override { BaseViewportWindow::mouseReleaseEvent(event); }

    /// Handles mouse move events.
    virtual void mouseMoveEvent(QMouseEvent* event) override { BaseViewportWindow::mouseMoveEvent(event); }

    /// Handles mouse wheel events.
    virtual void wheelEvent(QWheelEvent* event) override { BaseViewportWindow::wheelEvent(event); }

    /// Is called when the widgets looses the input focus.
    virtual void focusOutEvent(QFocusEvent* event) override { BaseViewportWindow::focusOutEvent(event); }

    /// Handles key-press events.
    virtual void keyPressEvent(QKeyEvent* event) override { 
        BaseViewportWindow::keyPressEvent(event); 
        QWindow::keyPressEvent(event);
    }

private Q_SLOTS:

    /// Keeps trying to initialize the Vulkan window surface.
    void ensureStarted();

    /// Releases all Vulkan resources held by the window.
    void reset();

private:

    /// Initializes the Vulkan objects of the window after it has been exposed for first time.  
    void init();

    /// Recreates the Vulkan swapchain.  
    void recreateSwapChain();

    /// Releases the resources of the Vulkan swapchain.  
    void releaseSwapChain();

    /// Starts rendering a frame.
    void beginFrame();

    /// Finishes rendering a frame.
    void endFrame();

private:

    /// The container widget created for the QWindow.
    QWidget* _widget;

    /// This is the renderer of the interactive viewport.
    OORef<ViewportVulkanSceneRenderer> _viewportRenderer;

    /// This renderer generates an offscreen rendering of the scene that allows picking of objects.
    OORef<PickingVulkanSceneRenderer> _pickingRenderer;

    /// The logical Vulkan device used by the window.
    std::shared_ptr<VulkanContext> _context;

    /// A flag that indicates that a viewport update has been requested.
    bool _updateRequested = false;

    enum Status {
        StatusUninitialized,
        StatusFail,
        StatusFailRetry,
        StatusDeviceReady,
        StatusReady
    };

    Status _status = StatusUninitialized;
    VkSurfaceKHR _surface = VK_NULL_HANDLE;
    QVector<VkFormat> _requestedColorFormats;
    VkFormat _colorFormat;
    VkColorSpaceKHR _colorSpace;
    
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    
    static const int MAX_SWAPCHAIN_BUFFER_COUNT = 3;
    static const int MAX_CONCURRENT_FRAME_COUNT = VulkanContext::MAX_CONCURRENT_FRAME_COUNT;
    static const int MAX_FRAME_LAG = MAX_CONCURRENT_FRAME_COUNT;
    
    VkPresentModeKHR _presentMode = VK_PRESENT_MODE_FIFO_KHR;
    
    int _swapChainBufferCount = 2;
    QSize _swapChainImageSize;
    VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
    
    struct ImageResources {
        VkImage image = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
        VkFence cmdFence = VK_NULL_HANDLE;
        bool cmdFenceWaitable = false;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkCommandBuffer presTransCmdBuf = VK_NULL_HANDLE;
        VkImage msaaImage = VK_NULL_HANDLE;
        VkImageView msaaImageView = VK_NULL_HANDLE;
        VulkanContext::ResourceFrameHandle resourceFrame = 0;
    } _imageRes[MAX_SWAPCHAIN_BUFFER_COUNT];
    
    VkDeviceMemory _msaaImageMem = VK_NULL_HANDLE;
    
    uint32_t _currentImage;
    
    struct FrameResources {
        VkFence fence = VK_NULL_HANDLE;
        bool fenceWaitable = false;
        VkSemaphore imageSem = VK_NULL_HANDLE;
        VkSemaphore drawSem = VK_NULL_HANDLE;
        VkSemaphore presTransSem = VK_NULL_HANDLE;
        bool imageAcquired = false;
        bool imageSemWaitable = false;
    } _frameRes[MAX_FRAME_LAG];
    
    uint32_t _currentFrame;
    
    VkRenderPass _defaultRenderPass = VK_NULL_HANDLE;
    
    VkDeviceMemory _dsMem = VK_NULL_HANDLE;
    VkImage _dsImage = VK_NULL_HANDLE;
    VkImageView _dsView = VK_NULL_HANDLE;

    /// The sample count used by the Vulkan framebuffer.
    VkSampleCountFlagBits _sampleCount = VK_SAMPLE_COUNT_1_BIT;
};

}   // End of namespace
