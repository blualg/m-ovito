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
#include <ovito/core/rendering/RendererResourceCache.h>
#include <ovito/core/rendering/ColorCodingGradient.h>

#include <QLoggingCategory>
#include <QVulkanInstance>
#include <QVulkanDeviceFunctions>
#include "vma/VulkanMemoryAllocator.h"

namespace Ovito {

OVITO_VULKANRENDERER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcVulkan);

/**
 * \brief Encapsulates a Vulkan logical device.
 */
class OVITO_VULKANRENDERER_EXPORT VulkanContext : public QObject, public RendererResourceCache
{
    Q_OBJECT

private:

    struct VulkanDataBuffer {
        VmaAllocator allocator = VK_NULL_HANDLE;
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;

        /// Default constructor.
        VulkanDataBuffer() noexcept = default;
        /// Don't allow copying.
        VulkanDataBuffer(const VulkanDataBuffer& other) = delete;
        VulkanDataBuffer& operator=(const VulkanDataBuffer& other) = delete;
        /// Move only.
        VulkanDataBuffer(VulkanDataBuffer&& other) noexcept : allocator(other.allocator), buffer{std::exchange(other.buffer, VkBuffer{VK_NULL_HANDLE})}, allocation(other.allocation) {}
        VulkanDataBuffer& operator=(VulkanDataBuffer&& other) noexcept {
            allocator = other.allocator;
            buffer = std::exchange(other.buffer, VkBuffer{VK_NULL_HANDLE});
            allocation = other.allocation;
            return *this;
        }
        /// Destructor.
        ~VulkanDataBuffer() {
            if(buffer != VK_NULL_HANDLE)
                vmaDestroyBuffer(allocator, buffer, allocation);
        }
    };

    struct VulkanDescriptorSet {
        VulkanContext* context = nullptr;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        /// Default constructor.
        VulkanDescriptorSet(VulkanContext* _context = nullptr) noexcept : context(_context) {}
        /// Don't allow copying.
        VulkanDescriptorSet(const VulkanDescriptorSet& other) = delete;
        VulkanDescriptorSet& operator=(const VulkanDescriptorSet& other) = delete;
        /// Move only.
        VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept : context(other.context), descriptorSet{std::exchange(other.descriptorSet, VkDescriptorSet{VK_NULL_HANDLE})} {}
        VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept {
            context = other.context;
            descriptorSet = std::exchange(other.descriptorSet, VkDescriptorSet{VK_NULL_HANDLE});
            return *this;
        }
        /// Destructor.
        ~VulkanDescriptorSet() {
            if(descriptorSet != VK_NULL_HANDLE)
                context->deviceFunctions()->vkFreeDescriptorSets(context->logicalDevice(), context->_descriptorPool, 1, &descriptorSet);
        }
    };

    struct VulkanImage {
        VulkanContext* context = nullptr;
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;

        /// Default constructor.
        VulkanImage() noexcept = default;
        /// Don't allow copying.
        VulkanImage(const VulkanImage& other) = delete;
        VulkanImage& operator=(const VulkanImage& other) = delete;
        /// Move only.
        VulkanImage(VulkanImage&& other) noexcept : context(other.context), image{std::exchange(other.image, VkImage{VK_NULL_HANDLE})}, allocation(other.allocation), imageView{std::exchange(other.imageView, VkImageView{VK_NULL_HANDLE})} {}
        VulkanImage& operator=(VulkanImage&& other) noexcept {
            context = other.context;
            image = std::exchange(other.image, VkImage{VK_NULL_HANDLE});
            allocation = other.allocation;
            imageView = std::exchange(other.imageView, VkImageView{VK_NULL_HANDLE});
            return *this;
        }
        /// Destructor.
        ~VulkanImage() {
            if(imageView != VK_NULL_HANDLE)
                context->deviceFunctions()->vkDestroyImageView(context->logicalDevice(), imageView, nullptr);
            if(image != VK_NULL_HANDLE)
                vmaDestroyImage(context->allocator(), image, allocation);
        }
    };

public:

    /// Returns a shared pointer to the global Vulkan instance.
    static std::shared_ptr<QVulkanInstance> vkInstance();

    /// Maximum number of rendering frames that can be potentially active at the same time.
    static const int MAX_CONCURRENT_FRAME_COUNT = 3;

    /// Constructor
    VulkanContext(QObject* parent = nullptr);

    /// Destructor.
    ~VulkanContext() { reset(); }

    /// Returns the Vulkan instance associated with the device.
    QVulkanInstance* vulkanInstance() const { return _vulkanInstance.get(); }

    /// Returns the list of properties for the supported physical devices in the system.
    /// This function can be called before making the window visible.
    QVector<VkPhysicalDeviceProperties> availablePhysicalDevices();

    /// Requests the usage of the physical device with index \a idx. The index
    /// corresponds to the list returned from availablePhysicalDevices().
    void setPhysicalDeviceIndex(int idx);

    /// Returns the index of the physical device to be used.
    int physicalDeviceIndex() const { return _physDevIndex; }

    /// Returns the list of the extensions that are supported by logical devices
    /// created from the physical device selected by setPhysicalDeviceIndex().
    QVulkanInfoVector<QVulkanExtension> supportedDeviceExtensions();

    /// Returns a pointer to the properties for the active physical device.
    const VkPhysicalDeviceProperties* physicalDeviceProperties() const {
        if(_physDevIndex < _physDevProps.count())
            return &_physDevProps[_physDevIndex];
        qWarning("VulkanContext: Physical device properties not available");
        return nullptr;
    }

    /// Sets the list of device \a extensions to be enabled. Unsupported extensions
    /// are ignored. The swapchain extension will always be added automatically,
    /// no need to include it in this list.
    void setDeviceExtensions(const QByteArrayList& extensions);

    /// Returns the active physical device.
    VkPhysicalDevice physicalDevice() const { return _physDevIndex < _physDevs.size() ? _physDevs[_physDevIndex] : VK_NULL_HANDLE; }

    /// Creates the logical Vulkan device.
    bool create(QWindow* window = nullptr);

    /// Releases all Vulkan resources.
    void reset();

    /// Returns the internal Vulkan logical device handle.
    VkDevice logicalDevice() const { return _device; }

    /// Returns the table of Vulkan device-independent functions.
    QVulkanFunctions* vulkanFunctions() const { return _vulkanFunctions; }

    /// Returns the device-specific Vulkan function table.
    QVulkanDeviceFunctions* deviceFunctions() const { return _deviceFunctions; }

    /// Returns the index of the queue family used for graphics rendering.
    uint32_t graphicsQueueFamilyIndex() const { return _gfxQueueFamilyIdx; }

    /// Returns the index of the queue family used for window presentation.
    uint32_t presentQueueFamilyIndex() const { return _presQueueFamilyIdx; }

    /// Returns the Vulkan queue used for graphics rendering.
    VkQueue graphicsQueue() const { return _gfxQueue; }

    /// The Vulkan queue used for window presentation.
    VkQueue presentQueue() const { return _presQueue; }

    /// Returns whether a separate present queue family is used.
    bool separatePresentQueue() const { return _presQueueFamilyIdx != _gfxQueueFamilyIdx; }

    /// Returns the command pool for creating commands for the graphics queue.
    VkCommandPool graphicsCommandPool() const { return _cmdPool; };

    /// Returns the command pool for creating commands for the present queue.
    VkCommandPool presentCommandPool() const { return _presCmdPool; };

    /// Returns a host visible memory type index suitable for general use.
    /// The returned memory type will be both host visible and coherent. In addition, it will also be cached, if possible.
    uint32_t hostVisibleMemoryIndex() const { return _hostVisibleMemIndex; }

    /// Returns a device local memory type index suitable for general use.
    /// Note: It is not guaranteed that this memory type is always suitable.
    /// The correct, cross-implementation solution - especially for device local images - is to manually
    /// pick a memory type after checking the mask returned from vkGetImageMemoryRequirements.
    uint32_t deviceLocalMemoryIndex() const { return _deviceLocalMemIndex; }

    /// Returns the Vulkan Memory Allocator (VMA) used for this device.
    VmaAllocator allocator() const { return _allocator; }

    /// Picks the right memory type for a Vulkan image.
    uint32_t chooseTransientImageMemType(VkImage img, uint32_t startIndex);

    /// Handles the situation when the Vulkan device was lost after a recent function call.
    bool checkDeviceLost(VkResult err);

    /// Returns the format to use for the standard depth-stencil buffer.
    VkFormat depthStencilFormat() const { return _dsFormat; }

    /// Returns the global Vulkan pipeline cache.
    VkPipelineCache pipelineCache() const { return _pipelineCache; }

    /// Helper routine for creating a Vulkan image.
    bool createVulkanImage(const QSize size, VkFormat format, VkSampleCountFlagBits sampleCount, VkImageUsageFlags usage, VkImageAspectFlags aspectMask, VkImage* images, VkDeviceMemory* mem, VkImageView* views, int count);

    /// Creates a default Vulkan render pass.
    VkRenderPass createDefaultRenderPass(VkFormat colorFormat, VkSampleCountFlagBits sampleCount);

    /// Loads a SPIR-V shader from a file.
    VkShaderModule createShader(const QString& filename);

    static inline VkDeviceSize aligned(VkDeviceSize v, VkDeviceSize byteAlign) {
        return (v + byteAlign - 1) & ~(byteAlign - 1);
    }

    /// Synchronously executes some memory transfer commands.
    void immediateTransferSubmit(std::function<void(VkCommandBuffer)>&& function);

    /// Returns the standard texture sampler, which uses nearest interpolation.
    VkSampler samplerNearest() const { return _samplerNearest; }

    /// Creates a new descriptor set from the pool and caches it, or returns an existing one for the given cache key.
    template<typename KeyType>
    std::pair<VkDescriptorSet, bool> createDescriptorSet(VkDescriptorSetLayout layout, KeyType&& cacheKey, ResourceFrameHandle resourceFrame) {
        // Check if this descriptor set with for the cache key has already been created.
        VulkanDescriptorSet& descriptorSet = lookup<VulkanDescriptorSet>(std::forward<KeyType>(cacheKey), resourceFrame);
        if(descriptorSet.descriptorSet != VK_NULL_HANDLE)
            return { descriptorSet.descriptorSet, false };

        // Otherwise create new descriptor set and store it in the cache.
        descriptorSet = createDescriptorSetImpl(layout);
        return { descriptorSet.descriptorSet, true };
    }

    /// Uploads an OVITO DataBuffer to the Vulkan device.
    VkBuffer uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, ResourceFrameHandle resourceFrame, VkBufferUsageFlagBits usage);

    /// Uploads some data to the Vulkan device as a buffer object and caches it.
    template<typename KeyType>
    VkBuffer createCachedBuffer(KeyType&& cacheKey, VkDeviceSize bufferSize, ResourceFrameHandle resourceFrame, VkBufferUsageFlagBits usage, std::function<void(void*)>&& fillMemoryFunc) {
        // Check if this OVITO data buffer has already been uploaded to the GPU.
        VulkanDataBuffer& dataBufferInfo = lookup<VulkanDataBuffer>(std::forward<KeyType>(cacheKey), resourceFrame);

        // If not, do it now.
        if(dataBufferInfo.buffer == VK_NULL_HANDLE)
            dataBufferInfo = createCachedBufferImpl(bufferSize, usage, std::move(fillMemoryFunc));

        return dataBufferInfo.buffer;
    }

    /// Uploads an image to the Vulkan device as a texture image.
    VkImageView uploadImage(const QImage& image, ResourceFrameHandle resourceFrame);

    /// Indicates whether the current Vulkan device uses a unified memory architecture, i.e.,
    /// the device-local memory heap is also the CPU-local memory heap.
    /// On UMA devices, no staging buffers are required.
    bool isUMA() const { return _isUMA; }

    /// Indicates whether the current Vulkan device supports the 'wideLines' feature.
    bool supportsWideLines() const { return _supportsWideLines; }

    /// Indicates whether the current Vulkan device supports the 'multiDrawIndirect' feature.
    bool supportsMultiDrawIndirect() const { return _supportsMultiDrawIndirect; }

    /// Indicates whether the current Vulkan device supports the 'drawIndirectFirstInstance' feature.
    bool supportsDrawIndirectFirstInstance() const { return _supportsDrawIndirectFirstInstance; }

    /// Indicates whether the current Vulkan device supports the 'extendedDynamicState' feature.
    bool supportsExtendedDynamicState() const { return _supportsExtendedDynamicState; }

Q_SIGNALS:

    /// Is emitted when the logical device is lost, meaning that a Vulkan failed with VK_ERROR_DEVICE_LOST.
    void logicalDeviceLost();

    /// Is emitted when the physical device is lost, meaning the creation of the logical device fails with VK_ERROR_DEVICE_LOST.
    void physicalDeviceLost();

    /// Is emitted right before the logical device is going to be destroyed (or was lost) and clients should release their Vulkan resources too.
    void releaseResourcesRequested();

private:

    /// Creates a new descriptor set from the pool.
    VulkanDescriptorSet createDescriptorSetImpl(VkDescriptorSetLayout layout);

    /// Uploads some data to the Vulkan device as a buffer object.
    VulkanDataBuffer createCachedBufferImpl(VkDeviceSize bufferSize, VkBufferUsageFlagBits usage, std::function<void(void*)>&& fillMemoryFunc);

private:

    /// The global Vulkan instance associated with the device.
    std::shared_ptr<QVulkanInstance> _vulkanInstance = vkInstance();

    /// The table of Vulkan device-independent functions.
    QVulkanFunctions* _vulkanFunctions = nullptr;

    /// The internal Vulkan logical device handle.
    VkDevice _device = VK_NULL_HANDLE;

    /// The device-specific Vulkan function table.
    QVulkanDeviceFunctions* _deviceFunctions = nullptr;

    /// The selected physical device index from which the logical device is created.
    int _physDevIndex = 0;

    /// The list of physical Vulkan devices.
    QVector<VkPhysicalDevice> _physDevs;

    /// The properties of each physical Vulkan device in the system.
    QVector<VkPhysicalDeviceProperties> _physDevProps;

    /// The extensions supported by each physical Vulkan device.
    QHash<VkPhysicalDevice, QVulkanInfoVector<QVulkanExtension>> _supportedDevExtensions;

    /// The list of device extensions requested by the user of the class.
    QByteArrayList _requestedDevExtensions;

    /// The queue family used for graphics rendering.
    uint32_t _gfxQueueFamilyIdx;

    /// The queue family used for window presentation.
    uint32_t _presQueueFamilyIdx;

    /// The Vulkan queue used for graphics rendering.
    VkQueue _gfxQueue;

    /// The Vulkan queue used for window presentation.
    VkQueue _presQueue;

    /// The command pool for creating commands for the graphics queue.
    VkCommandPool _cmdPool = VK_NULL_HANDLE;

    /// The command pool for creating commands for the presentation queue.
    VkCommandPool _presCmdPool = VK_NULL_HANDLE;

    /// The command pool used for transferring data buffers and images to GPU memory.
    VkCommandPool _transferCmdPool = VK_NULL_HANDLE;

    /// Fence object used for synchronized data transfers to the GPU.
    VkFence _transferFence = VK_NULL_HANDLE;

    /// The standard texture sampler, which uses nearest interpolation.
    VkSampler _samplerNearest = VK_NULL_HANDLE;

    /// The format to use for the depth-stencil buffer.
    VkFormat _dsFormat = VK_FORMAT_D24_UNORM_S8_UINT;

    /// A host visible memory type index suitable for general use.
    uint32_t _hostVisibleMemIndex;

    /// A device local memory type index suitable for general use.
    uint32_t _deviceLocalMemIndex;

    /// Indicates that this device uses a unified memory architecture, i.e.,
    /// the device-local memory heap is also the CPU-local memory heap.
    /// On UMA devices, no staging buffers are needed.
    bool _isUMA = false;

    /// Indicates that the current Vulkan device supports the 'wideLines' feature.
    bool _supportsWideLines = false;

    /// Indicates that the current Vulkan device supports the 'multiDrawIndirect' feature.
    bool _supportsMultiDrawIndirect = false;

    /// Indicates that the current Vulkan device supports the 'drawIndirectFirstInstance' feature.
    bool _supportsDrawIndirectFirstInstance = false;

    /// Indicates that the current Vulkan device supports the 'extendedDynamicState' feature.
    bool _supportsExtendedDynamicState = false;

    /// The Vulkan Memory Allocator used for this device.
    VmaAllocator _allocator = VK_NULL_HANDLE;

    /// The Vulkan pipeline cache.
    VkPipelineCache _pipelineCache = VK_NULL_HANDLE;

    /// The pool for creating descriptor sets.
    VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;

public:

    /// Pointer to optional Vulkan extension function.
    PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT = nullptr;

    /// Pointer to optional Vulkan extension function.
    PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT = nullptr;
};

}   // End of namespace
