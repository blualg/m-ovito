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

#include <ovito/core/Core.h>
#include <ovito/core/app/Application.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/rendering/SceneRenderer.h>
#include "VulkanContext.h"

#include <QLibrary>

namespace Ovito {

// The logging category used for Vulkan-related information.
Q_LOGGING_CATEGORY(lcVulkan, "ovito.vulkan");

/******************************************************************************
* Callback function for Vulkan debug layers.
******************************************************************************/
static bool vulkanDebugFilter(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage)
{
    return false;
}

/******************************************************************************
* Constructor
******************************************************************************/
VulkanContext::VulkanContext(QObject* parent) : QObject(parent)
{
    setDeviceExtensions(QByteArrayList()
        << VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME
        << VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME
        << VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
        << VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

    QSettings settings;
    setPhysicalDeviceIndex(settings.value("rendering/vulkan/selected_device", 0).toInt());
}

/******************************************************************************
* Returns a reference to the global Vulkan instance.
******************************************************************************/
std::shared_ptr<QVulkanInstance> VulkanContext::vkInstance()
{
    static std::weak_ptr<QVulkanInstance> globalInstance;
    if(std::shared_ptr<QVulkanInstance> inst = globalInstance.lock()) {
        return inst;
    }
    else {
#ifdef Q_OS_LINUX
        // Workaround for Qt not finding libvulkan.so.1 on Ubuntu systems.
        // The implementation of QVulkanInstance looks for libvulkan.so only.
        // In order to make it find libvulkan.so.1, we preload that library here.
        if(qEnvironmentVariableIsSet("QT_VULKAN_LIB") == false) {
            QLibrary vulkanLib("vulkan", 1);
            if(vulkanLib.resolve("vkGetInstanceProcAddr")) {
                qCDebug(lcVulkan) << "Preloaded libvulkan shared library: " << vulkanLib.fileName();
                qputenv("QT_VULKAN_LIB", QFile::encodeName(vulkanLib.fileName()));
            }
        }
#endif
        inst = std::make_shared<QVulkanInstance>();
#ifdef OVITO_DEBUG
        inst->setLayers(QByteArrayList() << "VK_LAYER_LUNARG_standard_validation");
        inst->installDebugOutputFilter(&vulkanDebugFilter);
#endif
        inst->setExtensions(QByteArrayList()
            << VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
            << VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

        if(!inst->create()) {
            throw SceneRenderer::RendererException(tr("Failed to initialize Vulkan interface (error code %1). Please make sure the Vulkan library is installed on your system and the graphics driver supports at least Vulkan API 1.0. "
                "If the Vulkan interface doesn't work, you can change the rendering interface back to OpenGL in the application settings dialog of OVITO.").arg(inst->errorCode()));
        }
        globalInstance = inst;
        return inst;
    }
}

/******************************************************************************
* Returns the list of properties for the supported physical devices in the system.
* This function can be called before creating the logical device.
******************************************************************************/
QVector<VkPhysicalDeviceProperties> VulkanContext::availablePhysicalDevices()
{
    if(!_physDevs.isEmpty() && !_physDevProps.isEmpty())
        return _physDevProps;

    QVulkanFunctions* f = vulkanInstance()->functions();
    uint32_t count = 1;
    VkResult err = f->vkEnumeratePhysicalDevices(vulkanInstance()->vkInstance(), &count, nullptr);
    if (err != VK_SUCCESS) {
        qWarning("VulkanContext: Failed to get physical device count: %d", err);
        return _physDevProps;
    }
    qCDebug(lcVulkan, "%d physical devices", count);
    if (!count)
        return _physDevProps;
    QVector<VkPhysicalDevice> devs(count);
    err = f->vkEnumeratePhysicalDevices(vulkanInstance()->vkInstance(), &count, devs.data());
    if(err != VK_SUCCESS) {
        qWarning("VulkanContext: Failed to enumerate physical devices: %d", err);
        return _physDevProps;
    }
    _physDevs = devs;
    _physDevProps.resize(count);
    for(uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties* p = &_physDevProps[i];
        f->vkGetPhysicalDeviceProperties(_physDevs.at(i), p);
        qCDebug(lcVulkan, "Physical device [%d]: name '%s' version %d.%d.%d", i, p->deviceName,
                VK_VERSION_MAJOR(p->driverVersion), VK_VERSION_MINOR(p->driverVersion),
                VK_VERSION_PATCH(p->driverVersion));
    }
    return _physDevProps;
}

/******************************************************************************
* Requests the usage of the physical device with index \a idx. The index
* corresponds to the list returned from availablePhysicalDevices().
* By default the first physical device is used.
*
* This function must be called before the logical device is created.
******************************************************************************/
void VulkanContext::setPhysicalDeviceIndex(int idx)
{
    if(_device != VK_NULL_HANDLE) {
        qWarning("VulkanContext: Attempted to set physical device when already initialized");
        return;
    }
    const int count = availablePhysicalDevices().count();
    if(idx < 0 || idx >= count) {
        qWarning("VulkanContext: Invalid physical device index %d (total physical devices: %d)", idx, count);
        return;
    }
    _physDevIndex = idx;
}

/******************************************************************************
* Returns the list of the extensions that are supported by logical devices
* created from the physical device selected by setPhysicalDeviceIndex().
*
* This function can be called before making creating the logical device.
******************************************************************************/
QVulkanInfoVector<QVulkanExtension> VulkanContext::supportedDeviceExtensions()
{
    availablePhysicalDevices();
    if(_physDevs.isEmpty()) {
        qWarning("VulkanContext: No physical devices found");
        return QVulkanInfoVector<QVulkanExtension>();
    }
    VkPhysicalDevice physDev = _physDevs.at(_physDevIndex);

    // Look up extensions in the cache.
    if(_supportedDevExtensions.contains(physDev))
        return _supportedDevExtensions.value(physDev);

    QVulkanFunctions* f = vulkanInstance()->functions();
    uint32_t count = 0;
    VkResult err = f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, nullptr);
    if(err == VK_SUCCESS) {
        QVector<VkExtensionProperties> extProps(count);
        err = f->vkEnumerateDeviceExtensionProperties(physDev, nullptr, &count, extProps.data());
        if(err == VK_SUCCESS) {
            QVulkanInfoVector<QVulkanExtension> exts;
            for(const VkExtensionProperties& prop : extProps) {
                QVulkanExtension ext;
                ext.name = prop.extensionName;
                ext.version = prop.specVersion;
                exts.append(ext);
            }
            _supportedDevExtensions.insert(physDev, exts);
//            qDebug(lcVulkan) << "Supported device extensions:" << exts;
            return exts;
        }
    }
    qWarning("VulkanContext: Failed to query device extension count: %d", err);
    return {};
}

/******************************************************************************
* Sets the list of device \a extensions to be enabled. Unsupported extensions
* are ignored.
*
* This function must be called before the logical device is created.
******************************************************************************/
void VulkanContext::setDeviceExtensions(const QByteArrayList& extensions)
{
    if(_device != VK_NULL_HANDLE) {
        qWarning("VulkanContext: Attempted to set device extensions when already initialized");
        return;
    }
    _requestedDevExtensions = extensions;
}

/******************************************************************************
* Creates the logical Vulkan device.
******************************************************************************/
bool VulkanContext::create(QWindow* window)
{
    OVITO_ASSERT(vulkanInstance());

    // Is the device already created?
    if(_device != VK_NULL_HANDLE)
        return true;

    _vulkanFunctions = vulkanInstance()->functions();

    qCDebug(lcVulkan, "VulkanContext create");

    // Get the list of available physical devices.
    availablePhysicalDevices();
    if(_physDevs.isEmpty())
        throw SceneRenderer::RendererException(tr("No Vulkan devices present in the system."));

    if(_physDevIndex < 0 || _physDevIndex >= _physDevs.count()) {
        qWarning("VulkanContext: Invalid physical device index; defaulting to 0");
        _physDevIndex = 0;
    }

    qCDebug(lcVulkan, "Using physical device [%d]", _physDevIndex);

    VkPhysicalDevice physDev = physicalDevice();

    // Enumerate the device's queue families.
    uint32_t queueCount = 0;
    vulkanFunctions()->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, nullptr);
    QVector<VkQueueFamilyProperties> queueFamilyProps(queueCount);
    vulkanFunctions()->vkGetPhysicalDeviceQueueFamilyProperties(physDev, &queueCount, queueFamilyProps.data());

    _gfxQueueFamilyIdx = uint32_t(-1);
    _presQueueFamilyIdx = uint32_t(-1);
    for(int i = 0; i < queueFamilyProps.count(); ++i) {
        const bool supportsPresent = vulkanInstance()->supportsPresent(physDev, i, window);
        qCDebug(lcVulkan, "queue family %d: flags=0x%x count=%d supportsPresent=%d", i,
                queueFamilyProps[i].queueFlags, queueFamilyProps[i].queueCount, supportsPresent);
        if(_gfxQueueFamilyIdx == uint32_t(-1)
                && (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                && supportsPresent)
            _gfxQueueFamilyIdx = i;
    }
    if(_gfxQueueFamilyIdx != uint32_t(-1))
        _presQueueFamilyIdx = _gfxQueueFamilyIdx;
    else {
        qCDebug(lcVulkan, "No queue with graphics+present; trying separate queues");
        for(int i = 0; i < queueFamilyProps.count(); ++i) {
            if(_gfxQueueFamilyIdx == uint32_t(-1) && (queueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                _gfxQueueFamilyIdx = i;
            if(_presQueueFamilyIdx == uint32_t(-1) && vulkanInstance()->supportsPresent(physDev, i, window))
                _presQueueFamilyIdx = i;
        }
    }
    if(_gfxQueueFamilyIdx == uint32_t(-1))
        throw Exception(tr("Cannot initialize Vulkan rendering device. No graphics queue family found."));
    if(_presQueueFamilyIdx == uint32_t(-1))
        throw Exception(tr("Cannot initialize Vulkan rendering device. No present queue family found."));

#ifdef OVITO_DEBUG
    // Allow testing the separate present queue case in debug builds on AMD cards
    if(qEnvironmentVariableIsSet("QT_VK_PRESENT_QUEUE_INDEX"))
        _presQueueFamilyIdx = qEnvironmentVariableIntValue("QT_VK_PRESENT_QUEUE_INDEX");
#endif

    qCDebug(lcVulkan, "Using queue families: graphics = %u present = %u", _gfxQueueFamilyIdx, _presQueueFamilyIdx);

    // Filter out unsupported extensions in order to keep symmetry
    // with how QVulkanInstance behaves. Add the swapchain extension when
    // the device is to be used for a window.
    QVector<const char*> devExts;
    QVulkanInfoVector<QVulkanExtension> supportedExtensions = supportedDeviceExtensions();
    QByteArrayList reqExts = _requestedDevExtensions;
    if(window != nullptr)
        reqExts.append("VK_KHR_swapchain");
    for(const QByteArray& ext : reqExts) {
        if(supportedExtensions.contains(ext))
            devExts.append(ext.constData());
    }
    qCDebug(lcVulkan) << "Enabling device extensions:" << devExts;

    // Prepare data structure for logical device creation.
    VkDeviceQueueCreateInfo queueInfo[2];
    const float prio[] = { 0.0f };
    memset(queueInfo, 0, sizeof(queueInfo));
    queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo[0].queueFamilyIndex = _gfxQueueFamilyIdx;
    queueInfo[0].queueCount = 1;
    queueInfo[0].pQueuePriorities = prio;
    if(_gfxQueueFamilyIdx != _presQueueFamilyIdx) {
        queueInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[1].queueFamilyIndex = _presQueueFamilyIdx;
        queueInfo[1].queueCount = 1;
        queueInfo[1].pQueuePriorities = prio;
    }

    VkDeviceCreateInfo devInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    devInfo.queueCreateInfoCount = !separatePresentQueue() ? 1 : 2;
    devInfo.pQueueCreateInfos = queueInfo;
    devInfo.enabledExtensionCount = devExts.count();
    devInfo.ppEnabledExtensionNames = devExts.constData();
    // Device layers are not supported by this implementation since that's an already deprecated
    // API. However, have a workaround for systems with older API and layers (f.ex. L4T
    // 24.2 for the Jetson TX1 provides API 1.0.13 and crashes when the validation layer
    // is enabled for the instance but not the device).
    uint32_t apiVersion = _physDevProps[_physDevIndex].apiVersion;
    if(VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) == 0 && VK_VERSION_PATCH(apiVersion) <= 13) {
        // Make standard validation work at least.
        const QByteArray stdValName = QByteArrayLiteral("VK_LAYER_LUNARG_standard_validation");
        const char* stdValNamePtr = stdValName.constData();
        if(vulkanInstance()->layers().contains(stdValName)) {
            uint32_t count = 0;
            VkResult err = vulkanFunctions()->vkEnumerateDeviceLayerProperties(physDev, &count, nullptr);
            if(err == VK_SUCCESS) {
                QVector<VkLayerProperties> layerProps(count);
                err = vulkanFunctions()->vkEnumerateDeviceLayerProperties(physDev, &count, layerProps.data());
                if(err == VK_SUCCESS) {
                    for(const VkLayerProperties &prop : layerProps) {
                        if(!strncmp(prop.layerName, stdValNamePtr, stdValName.size())) {
                            devInfo.enabledLayerCount = 1;
                            devInfo.ppEnabledLayerNames = &stdValNamePtr;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Query the device's available features.
    VkPhysicalDeviceFeatures2 availableFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceFeatures& availableFeatures = availableFeatures2.features;
    // Query whether the device supports the 'extendedDynamicState' feature.
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT };
    availableFeatures2.pNext = &extendedDynamicStateFeatures;
    VkPhysicalDeviceFeatures2 requestedFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    VkPhysicalDeviceFeatures& requestedFeatures = requestedFeatures2.features;
    if(vulkanInstance()->extensions().contains(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)vulkanInstance()->getInstanceProcAddr("vkGetPhysicalDeviceFeatures2KHR");
        vkGetPhysicalDeviceFeatures2KHR(physDev, &availableFeatures2);
        devInfo.pNext = &requestedFeatures2;
    }
    else {
        vulkanFunctions()->vkGetPhysicalDeviceFeatures(physDev, &availableFeatures);
        devInfo.pEnabledFeatures = &requestedFeatures;
    }

    // Enable the feature which we can use.
    memset(&requestedFeatures, 0, sizeof(requestedFeatures));

    // Enable the 'wideLines' feature, which is used by VulkanLinePrimitive to render lines that are more than 1 pixel wide.
    _supportsWideLines = availableFeatures.wideLines;
    if(availableFeatures.wideLines)
        requestedFeatures.wideLines = VK_TRUE;

    // Enable the 'multiDrawIndirect' feature, which is used for depth-sorted particle drawing.
    _supportsMultiDrawIndirect = availableFeatures.multiDrawIndirect;
    if(availableFeatures.multiDrawIndirect)
        requestedFeatures.multiDrawIndirect = VK_TRUE;

    // Enable the 'drawIndirectFirstInstance' feature, which is used for depth-sorted particle drawing.
    _supportsDrawIndirectFirstInstance = availableFeatures.drawIndirectFirstInstance;
    if(availableFeatures.drawIndirectFirstInstance)
        requestedFeatures.drawIndirectFirstInstance = VK_TRUE;

    // Enable the 'extendedDynamicState' feature, which wllows to temporarily disable depth tests without pipeline duplication.
    _supportsExtendedDynamicState = extendedDynamicStateFeatures.extendedDynamicState;
    if(extendedDynamicStateFeatures.extendedDynamicState) {
        extendedDynamicStateFeatures.pNext = requestedFeatures2.pNext;
        requestedFeatures2.pNext = &extendedDynamicStateFeatures;
    }

    VkResult err = vulkanFunctions()->vkCreateDevice(physDev, &devInfo, nullptr, &_device);
    if(err == VK_ERROR_DEVICE_LOST) {
        qWarning("VulkanContext: Physical device lost");
        Q_EMIT physicalDeviceLost();
        // Clear the caches so the list of physical devices is re-queried
        _physDevs.clear();
        _physDevProps.clear();
        return false;
    }
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to create logical Vulkan device (error code %1).").arg(err));

    // Get the function pointers for device-specific Vulkan functions.
    _deviceFunctions = vulkanInstance()->deviceFunctions(_device);
    OVITO_ASSERT(_deviceFunctions);
    // Query function pointers for optional extensions.
    vkCmdSetDepthTestEnableEXT = (PFN_vkCmdSetDepthTestEnableEXT)vulkanInstance()->getInstanceProcAddr("vkCmdSetDepthTestEnableEXT");
    vkCmdSetCullModeEXT = (PFN_vkCmdSetCullModeEXT)vulkanInstance()->getInstanceProcAddr("vkCmdSetCullModeEXT");

    // Initialize Vulkan Memory Allocator.
    VmaAllocatorCreateInfo allocatorInfo = {};
    //allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorInfo.physicalDevice = physicalDevice();
    allocatorInfo.device = logicalDevice();
    allocatorInfo.instance = vulkanInstance()->vkInstance();
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT; // OVITO's Vulkan renderer is not thread-safe anyway.
    VmaVulkanFunctions vulkanFunctionsTable = {};
    vulkanFunctionsTable.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)vulkanInstance()->getInstanceProcAddr("vkGetPhysicalDeviceProperties");
    vulkanFunctionsTable.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)vulkanInstance()->getInstanceProcAddr("vkGetPhysicalDeviceMemoryProperties");
    vulkanFunctionsTable.vkAllocateMemory = (PFN_vkAllocateMemory)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkAllocateMemory");
    vulkanFunctionsTable.vkFreeMemory = (PFN_vkFreeMemory)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkFreeMemory");
    vulkanFunctionsTable.vkMapMemory = (PFN_vkMapMemory)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkMapMemory");
    vulkanFunctionsTable.vkUnmapMemory = (PFN_vkUnmapMemory)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkUnmapMemory");
    vulkanFunctionsTable.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkFlushMappedMemoryRanges");
    vulkanFunctionsTable.vkInvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkInvalidateMappedMemoryRanges");
    vulkanFunctionsTable.vkBindBufferMemory = (PFN_vkBindBufferMemory)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkBindBufferMemory");
    vulkanFunctionsTable.vkBindImageMemory = (PFN_vkBindImageMemory)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkBindImageMemory");
    vulkanFunctionsTable.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkGetBufferMemoryRequirements");
    vulkanFunctionsTable.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkGetImageMemoryRequirements");
    vulkanFunctionsTable.vkCreateBuffer = (PFN_vkCreateBuffer)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkCreateBuffer");
    vulkanFunctionsTable.vkDestroyBuffer = (PFN_vkDestroyBuffer)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkDestroyBuffer");
    vulkanFunctionsTable.vkCreateImage = (PFN_vkCreateImage)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkCreateImage");
    vulkanFunctionsTable.vkDestroyImage = (PFN_vkDestroyImage)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkDestroyImage");
    vulkanFunctionsTable.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkCmdCopyBuffer");

    // VK_KHR_dedicated_allocation is a Vulkan extension which can be used to improve performance on some GPUs.
    // It augments Vulkan API with possibility to query driver whether it prefers particular buffer or image to have its own,
    // dedicated allocation (separate VkDeviceMemory block) for better efficiency - to be able to do some internal optimizations.
    if(reqExts.contains(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) && reqExts.contains(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME)) {
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        vulkanFunctionsTable.vkGetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkGetBufferMemoryRequirements2KHR");
        vulkanFunctionsTable.vkGetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2KHR)vulkanFunctions()->vkGetDeviceProcAddr(logicalDevice(), "vkGetImageMemoryRequirements2KHR");
    }

    allocatorInfo.pVulkanFunctions = &vulkanFunctionsTable;
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    // Retrieve the queue handles from the device.
    deviceFunctions()->vkGetDeviceQueue(logicalDevice(), _gfxQueueFamilyIdx, 0, &_gfxQueue);
    if(!separatePresentQueue())
        _presQueue = _gfxQueue;
    else
        deviceFunctions()->vkGetDeviceQueue(logicalDevice(), _presQueueFamilyIdx, 0, &_presQueue);

    // Create command pools.
    VkCommandPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.queueFamilyIndex = _gfxQueueFamilyIdx;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    err = deviceFunctions()->vkCreateCommandPool(logicalDevice(), &poolInfo, nullptr, &_cmdPool);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to create Vulkan command pool (error code %1).").arg(err));
    if(separatePresentQueue()) {
        poolInfo.queueFamilyIndex = _presQueueFamilyIdx;
        poolInfo.flags = 0;
        err = deviceFunctions()->vkCreateCommandPool(logicalDevice(), &poolInfo, nullptr, &_presCmdPool);
        if(err != VK_SUCCESS)
            throw SceneRenderer::RendererException(tr("Failed to create Vulkan command pool for present queue (error code %1).").arg(err));
    }

    // Create command pool used for data transfers.
    poolInfo.queueFamilyIndex = _gfxQueueFamilyIdx;
    poolInfo.flags = 0;
    err = deviceFunctions()->vkCreateCommandPool(logicalDevice(), &poolInfo, nullptr, &_transferCmdPool);
    if(err != VK_SUCCESS)
        throw Exception(tr("Failed to create Vulkan transfer command pool (error code %1).").arg(err));
    // Create fence for synchronizing data transfers.
    VkFenceCreateInfo fenceInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    deviceFunctions()->vkCreateFence(logicalDevice(), &fenceInfo, nullptr, &_transferFence);

    _hostVisibleMemIndex = 0;
    VkPhysicalDeviceMemoryProperties physDevMemProps;
    bool hostVisibleMemIndexSet = false;
    vulkanFunctions()->vkGetPhysicalDeviceMemoryProperties(physicalDevice(), &physDevMemProps);
    for(uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
        const VkMemoryType* memType = physDevMemProps.memoryTypes;
        qCDebug(lcVulkan, "memtype %d: flags=0x%x", i, memType[i].propertyFlags);
        // Find a host visible, host coherent memtype. If there is one that is
        // cached as well (in addition to being coherent), prefer that.
        const int hostVisibleAndCoherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if((memType[i].propertyFlags & hostVisibleAndCoherent) == hostVisibleAndCoherent) {
            if(!hostVisibleMemIndexSet || (memType[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)) {
                hostVisibleMemIndexSet = true;
                _hostVisibleMemIndex = i;
            }
        }
    }
    qCDebug(lcVulkan, "Picked memtype %d for host visible memory", _hostVisibleMemIndex);

    _deviceLocalMemIndex = 0;
    for(uint32_t i = 0; i < physDevMemProps.memoryTypeCount; ++i) {
        const VkMemoryType* memType = physDevMemProps.memoryTypes;
        // Just pick the first device local memtype.
        if(memType[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            _deviceLocalMemIndex = i;
            break;
        }
    }
    qCDebug(lcVulkan, "Picked memtype %d for device local memory", _deviceLocalMemIndex);

    // Determine if this device uses a unified memory architecture, i.e.,
    // all device-local memory heaps are also the CPU-local memory heaps.
    _isUMA = true;
    for(uint32_t heapIndex = 0; heapIndex < physDevMemProps.memoryHeapCount; heapIndex++) {
        if(!(physDevMemProps.memoryHeaps[heapIndex].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT))
            _isUMA = false;
    }
    qCDebug(lcVulkan, "Is UMA device: %d", _isUMA);

    const VkFormat dsFormatCandidates[] = {
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D16_UNORM_S8_UINT
    };
    const int dsFormatCandidateCount = sizeof(dsFormatCandidates) / sizeof(VkFormat);
    int dsFormatIdx = 0;
    while(dsFormatIdx < dsFormatCandidateCount) {
        _dsFormat = dsFormatCandidates[dsFormatIdx];
        VkFormatProperties fmtProp;
        vulkanFunctions()->vkGetPhysicalDeviceFormatProperties(physicalDevice(), _dsFormat, &fmtProp);
        if(fmtProp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            break;
        ++dsFormatIdx;
    }
    if(dsFormatIdx == dsFormatCandidateCount)
        qWarning("VulkanContext: Failed to find an optimal depth-stencil format");
    qCDebug(lcVulkan, "Depth-stencil format: %d", _dsFormat);

    // Create pipeline cache.
    VkPipelineCacheCreateInfo pipelineCacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    err = deviceFunctions()->vkCreatePipelineCache(logicalDevice(), &pipelineCacheInfo, nullptr, &_pipelineCache);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to create Vulkan pipeline cache (error code %1).").arg(err));

    // Create a standard texture sampler.
    VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerInfo.minLod = samplerInfo.maxLod = 0.0f;
    err = deviceFunctions()->vkCreateSampler(logicalDevice(), &samplerInfo, nullptr, &_samplerNearest);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to create Vulkan texture sampler (error code %1).").arg(err));

    // Create the descriptor pool.
    std::array<VkDescriptorPoolSize, 2> poolSizes;
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 100;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 100;
    VkDescriptorPoolCreateInfo descriptorPoolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = 200;
    err = deviceFunctions()->vkCreateDescriptorPool(logicalDevice(), &descriptorPoolInfo, nullptr, &_descriptorPool);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to create Vulkan descriptor pool (error code %1).").arg(err));

    return true;
}

/******************************************************************************
* Picks the right memory type for a Vulkan image.
******************************************************************************/
uint32_t VulkanContext::chooseTransientImageMemType(VkImage img, uint32_t startIndex)
{
    VkPhysicalDeviceMemoryProperties physDevMemProps;
    vulkanFunctions()->vkGetPhysicalDeviceMemoryProperties(_physDevs[_physDevIndex], &physDevMemProps);
    VkMemoryRequirements memReq;
    deviceFunctions()->vkGetImageMemoryRequirements(logicalDevice(), img, &memReq);
    uint32_t memTypeIndex = uint32_t(-1);
    if(memReq.memoryTypeBits) {
        // Find a device local + lazily allocated, or at least device local memtype.
        const VkMemoryType *memType = physDevMemProps.memoryTypes;
        bool foundDevLocal = false;
        for(uint32_t i = startIndex; i < physDevMemProps.memoryTypeCount; ++i) {
            if(memReq.memoryTypeBits & (1 << i)) {
                if(memType[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                    if(!foundDevLocal) {
                        foundDevLocal = true;
                        memTypeIndex = i;
                    }
                    if(memType[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
                        memTypeIndex = i;
                        break;
                    }
                }
            }
        }
    }
    return memTypeIndex;
}

/******************************************************************************
* Releases all Vulkan resources.
******************************************************************************/
void VulkanContext::reset()
{
    if(logicalDevice() == VK_NULL_HANDLE)
        return;

    // Tell clients of the class to also release their Vulkan resources.
    Q_EMIT releaseResourcesRequested();

    // Make sure our clients have release their resources properly.
    OVITO_ASSERT(RendererResourceCache::empty());

    qCDebug(lcVulkan, "VulkanContext reset");

    // Release command buffer pool used for graphics rendering.
    if(graphicsCommandPool() != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyCommandPool(logicalDevice(), graphicsCommandPool(), nullptr);
        _cmdPool = VK_NULL_HANDLE;
    }

    // Release command buffer pool used for presentation.
    if(presentCommandPool() != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyCommandPool(logicalDevice(), presentCommandPool(), nullptr);
        _presCmdPool = VK_NULL_HANDLE;
    }

    // Release command buffer pool used for data uploads.
    if(_transferCmdPool != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyCommandPool(logicalDevice(), _transferCmdPool, nullptr);
        _transferCmdPool = VK_NULL_HANDLE;
    }

    // Release the fence object.
    if(_transferFence != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyFence(logicalDevice(), _transferFence, nullptr);
        _transferFence = VK_NULL_HANDLE;
    }

    // Release the texture samplers.
    if(_samplerNearest != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroySampler(logicalDevice(), _samplerNearest, nullptr);
        _samplerNearest = VK_NULL_HANDLE;
    }

    // Release the descriptor sets.
    if(_descriptorPool != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyDescriptorPool(logicalDevice(), _descriptorPool, nullptr);
        _descriptorPool = VK_NULL_HANDLE;
    }

    // Release pipeline cache.
    if(pipelineCache() != VK_NULL_HANDLE) {

#if 0
        // Retrieve pipeline cache data and save it to disk.
        size_t cacheDataSize;
        if(deviceFunctions()->vkGetPipelineCacheData(logicalDevice(), pipelineCache(), &cacheDataSize, nullptr) == VK_SUCCESS && cacheDataSize != 0) {
            std::vector<uint8_t> cacheData(cacheDataSize);
            if(deviceFunctions()->vkGetPipelineCacheData(logicalDevice(), pipelineCache(), &cacheDataSize, cacheData.data()) == VK_SUCCESS) {
                qDebug() << "Pipeline cache size:" << cacheDataSize;

            }
        }
#endif

        deviceFunctions()->vkDestroyPipelineCache(logicalDevice(), pipelineCache(), nullptr);
        _pipelineCache = VK_NULL_HANDLE;
    }

    // Destroy the Vulkan Memory Allocator.
    vmaDestroyAllocator(_allocator);

    // Release the logical device.
    deviceFunctions()->vkDestroyDevice(logicalDevice(), nullptr);
    // Discard cached device function pointers held by Qt.
    vulkanInstance()->resetDeviceFunctions(logicalDevice());

    // Reset internal handles.
    _device = VK_NULL_HANDLE;
    _deviceFunctions = nullptr;
}

/******************************************************************************
* Handles the situation when the Vulkan device was lost after a recent function call.
******************************************************************************/
bool VulkanContext::checkDeviceLost(VkResult err)
{
    if(err == VK_ERROR_DEVICE_LOST) {
        qWarning("VulkanContext: Device lost");
        qCDebug(lcVulkan, "Releasing all resources due to device lost");
        reset();
        qCDebug(lcVulkan, "Restarting after device lost");
        Q_EMIT logicalDeviceLost(); // This calls VulkanViewportWindow::ensureStarted()
        return true;
    }
    return false;
}

/******************************************************************************
* Helper routine for creating a Vulkan image.
******************************************************************************/
bool VulkanContext::createVulkanImage(const QSize size,
                                        VkFormat format,
                                        VkSampleCountFlagBits sampleCount,
                                        VkImageUsageFlags usage,
                                        VkImageAspectFlags aspectMask,
                                        VkImage* images,
                                        VkDeviceMemory* mem,
                                        VkImageView* views,
                                        int count)
{
#ifdef Q_OS_MAC
    if(size.width() > 16384 || size.height() > 16384)
        throw SceneRenderer::RendererException(tr("The requested render buffer dimensions exceed the graphics device limits on the macOS platform."));
#endif
    VkMemoryRequirements memReq;
    VkResult err;
    for(int i = 0; i < count; ++i) {
        VkImageCreateInfo imgInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = format;
        imgInfo.extent.width = size.width();
        imgInfo.extent.height = size.height();
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = sampleCount;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = usage;
        err = deviceFunctions()->vkCreateImage(logicalDevice(), &imgInfo, nullptr, images + i);
        if(err != VK_SUCCESS) {
            qWarning("VulkanContext: Failed to create image: %d", err);
            return false;
        }
        // Assume the reqs are the same since the images are same in every way.
        // Still, call GetImageMemReq for every image, in order to prevent the
        // validation layer from complaining.
        deviceFunctions()->vkGetImageMemoryRequirements(logicalDevice(), images[i], &memReq);
    }
    VkMemoryAllocateInfo memInfo;
    memset(&memInfo, 0, sizeof(memInfo));
    memInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memInfo.allocationSize = VulkanContext::aligned(memReq.size, memReq.alignment) * count;
    uint32_t startIndex = 0;
    do {
        memInfo.memoryTypeIndex = chooseTransientImageMemType(images[0], startIndex);
        if(memInfo.memoryTypeIndex == uint32_t(-1)) {
            qWarning("VulkanContext: No suitable memory type found");
            return false;
        }
        startIndex = memInfo.memoryTypeIndex + 1;
        qCDebug(lcVulkan, "Allocating %u bytes for transient image (memtype %u) with format %u", uint32_t(memInfo.allocationSize), memInfo.memoryTypeIndex, format);
        err = deviceFunctions()->vkAllocateMemory(logicalDevice(), &memInfo, nullptr, mem);
        if(err != VK_SUCCESS && err != VK_ERROR_OUT_OF_DEVICE_MEMORY) {
            qWarning("VulkanContext: Failed to allocate image memory: %d", err);
            return false;
        }
    }
    while(err != VK_SUCCESS);
    VkDeviceSize ofs = 0;
    for(int i = 0; i < count; ++i) {
        err = deviceFunctions()->vkBindImageMemory(logicalDevice(), images[i], *mem, ofs);
        if(err != VK_SUCCESS) {
            qWarning("VulkanContext: Failed to bind image memory: %d", err);
            return false;
        }
        ofs += VulkanContext::aligned(memReq.size, memReq.alignment);
        VkImageViewCreateInfo imgViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        imgViewInfo.image = images[i];
        imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imgViewInfo.format = format;
        imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        imgViewInfo.subresourceRange.aspectMask = aspectMask;
        imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;
        err = deviceFunctions()->vkCreateImageView(logicalDevice(), &imgViewInfo, nullptr, views + i);
        if(err != VK_SUCCESS) {
            qWarning("VulkanContext: Failed to create image view: %d", err);
            return false;
        }
    }
    return true;
}

/******************************************************************************
* Creates a default Vulkan render pass.
******************************************************************************/
VkRenderPass VulkanContext::createDefaultRenderPass(VkFormat colorFormat, VkSampleCountFlagBits sampleCount)
{
    VkAttachmentDescription attDesc[3];
    memset(attDesc, 0, sizeof(attDesc));
    const bool msaa = sampleCount > VK_SAMPLE_COUNT_1_BIT;
    // This is either the non-msaa render target or the resolve target.
    attDesc[0].format = colorFormat;
    attDesc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // ignored when msaa
    attDesc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attDesc[1].format = depthStencilFormat();
    attDesc[1].samples = sampleCount;
    attDesc[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    if(msaa) {
        // msaa render target
        attDesc[2].format = colorFormat;
        attDesc[2].samples = sampleCount;
        attDesc[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attDesc[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attDesc[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attDesc[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attDesc[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attDesc[2].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference resolveRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference dsRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subPassDesc = {};
    subPassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subPassDesc.colorAttachmentCount = 1;
    subPassDesc.pColorAttachments = &colorRef;
    subPassDesc.pDepthStencilAttachment = &dsRef;
    VkRenderPassCreateInfo rpInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpInfo.attachmentCount = 2;
    rpInfo.pAttachments = attDesc;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subPassDesc;
    if(msaa) {
        colorRef.attachment = 2;
        subPassDesc.pResolveAttachments = &resolveRef;
        rpInfo.attachmentCount = 3;
    }
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult err = deviceFunctions()->vkCreateRenderPass(logicalDevice(), &rpInfo, nullptr, &renderPass);
    if(err != VK_SUCCESS) {
        qWarning("VulkanContext: Failed to create renderpass: %d", err);
        return VK_NULL_HANDLE;
    }
    return renderPass;
}

/******************************************************************************
* Loads a SPIR-V shader from a file.
******************************************************************************/
VkShaderModule VulkanContext::createShader(const QString& filename)
{
    QFile file(filename);
    if(!file.open(QIODevice::ReadOnly))
        throw SceneRenderer::RendererException(tr("File to load Vulkan shader file '%1': %2").arg(filename).arg(file.errorString()));
    QByteArray blob = file.readAll();
    file.close();

    VkShaderModuleCreateInfo shaderInfo;
    memset(&shaderInfo, 0, sizeof(shaderInfo));
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = blob.size();
    shaderInfo.pCode = reinterpret_cast<const uint32_t*>(blob.constData());
    VkShaderModule shaderModule;
    VkResult err = deviceFunctions()->vkCreateShaderModule(_device, &shaderInfo, nullptr, &shaderModule);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("File to create Vulkan shader module '%1'. Error code: %2").arg(filename).arg(err));

    return shaderModule;
}

/******************************************************************************
* Synchronously executes some memory transfer commands.
******************************************************************************/
void VulkanContext::immediateTransferSubmit(std::function<void(VkCommandBuffer)>&& function)
{
    // This method must be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == this->thread());

    // Allocate the default command buffer that we will use for the instant commands.
    VkCommandBufferAllocateInfo cmdAllocInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, _transferCmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1 };
    VkCommandBuffer cmdBuf;
    VkResult err = deviceFunctions()->vkAllocateCommandBuffers(logicalDevice(), &cmdAllocInfo, &cmdBuf);
    if(err != VK_SUCCESS) {
        qWarning("VulkanContext: Failed to allocate transfer command buffer: %d", err);
        throw SceneRenderer::RendererException(QStringLiteral("Failed to allocate Vulkan transfer command buffer."));
    }

    // Begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that.
    VkCommandBufferBeginInfo cmdBufBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT , nullptr };
    err = deviceFunctions()->vkBeginCommandBuffer(cmdBuf, &cmdBufBeginInfo);
    if(err != VK_SUCCESS) {
        qWarning("VulkanContext: Failed to begin transfer command buffer: %d", err);
        throw SceneRenderer::RendererException(QStringLiteral("Failed to begin Vulkan transfer command buffer."));
    }

    // Execute the function supplied by the caller.
    function(cmdBuf);

    // End recording commands.
    err = deviceFunctions()->vkEndCommandBuffer(cmdBuf);
    if(err != VK_SUCCESS) {
        qWarning("VulkanContext: Failed to end transfer command buffer: %d", err);
        throw SceneRenderer::RendererException(QStringLiteral("Failed to end Vulkan transfer command buffer."));
    }

    // Submit command buffer to the queue and execute it.
    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    err = deviceFunctions()->vkQueueSubmit(graphicsQueue(), 1, &submitInfo, _transferFence);
    if(err != VK_SUCCESS) {
        qWarning("VulkanContext: Failed to submit transfer commands to Vulkan queue: %d", err);
        throw SceneRenderer::RendererException(QStringLiteral("Failed to submit transfer commands to Vulkan queue."));
    }

    // Block until the transfer operation completes.
    deviceFunctions()->vkWaitForFences(logicalDevice(), 1, &_transferFence, VK_TRUE, UINT64_MAX);
    // Reset the fence object.
    deviceFunctions()->vkResetFences(logicalDevice(), 1, &_transferFence);

    // Clear the command pool. This will free the command buffer too.
    deviceFunctions()->vkResetCommandPool(logicalDevice(), _transferCmdPool, 0);
}

/******************************************************************************
* Uploads some data to the Vulkan device as a buffer object.
******************************************************************************/
VulkanContext::VulkanDataBuffer VulkanContext::createCachedBufferImpl(VkDeviceSize bufferSize, VkBufferUsageFlagBits usage, std::function<void(void*)>&& fillMemoryFunc)
{
    OVITO_ASSERT(logicalDevice());

    // This method must be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == this->thread());

    // Prepare the data structure that represents the OVITO data buffer uploaded to the GPU.
    VulkanDataBuffer bufferInfo;
    bufferInfo.allocator = allocator();

    // Create a Vulkan buffer.
    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // The buffer will only be used from the graphics queue, so we can stick to exclusive access.
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    VkResult err = vmaCreateBuffer(allocator(), &bufferCreateInfo, &allocInfo, &bufferInfo.buffer, &bufferInfo.allocation, nullptr);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to allocate Vulkan buffer object of size %1 (error code %2).").arg(bufferSize).arg(err));

    // Fill the buffer with data.
    void* p;
    err = vmaMapMemory(allocator(), bufferInfo.allocation, &p);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to map memory of Vulkan data buffer (error code %1).").arg(err));
    vmaFlushAllocation(allocator(), bufferInfo.allocation, 0, VK_WHOLE_SIZE);
    // Call the user-supplied function that fills the buffer with data to be uploaded to GPU memory.
    std::move(fillMemoryFunc)(p);
    vmaUnmapMemory(allocator(), bufferInfo.allocation);

    return bufferInfo;
}

/******************************************************************************
* Uploads an OVITO DataBuffer to the Vulkan device.
******************************************************************************/
VkBuffer VulkanContext::uploadDataBuffer(const ConstDataBufferPtr& dataBuffer, ResourceFrameHandle resourceFrame, VkBufferUsageFlagBits usage)
{
    OVITO_ASSERT(dataBuffer);

    // Determine the required buffer size.
    VkDeviceSize bufferSize;
    if(dataBuffer->dataType() == DataBuffer::Float32 || dataBuffer->dataType() == DataBuffer::Float64) {
        bufferSize = dataBuffer->size() * dataBuffer->componentCount() * sizeof(float);

        // When uploading the data to a SSBO, automatically convert vec3 to vec4, because of the 16-byte alignment requirement of Vulkan.
        if(usage == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT && dataBuffer->componentCount() == 3)
            bufferSize = dataBuffer->size() * 4 * sizeof(float);
    }
    else {
        OVITO_ASSERT(false);
        throw SceneRenderer::RendererException(tr("Cannot create Vulkan vertex buffer for DataBuffer with data type %1.").arg(dataBuffer->dataType()));
    }

    // Create a Vulkan buffer object and fill it with the data from the OVITO DataBuffer object.
    return createCachedBuffer(dataBuffer, bufferSize, resourceFrame, usage, [&](void* p) {
        if(dataBuffer->dataType() == DataBuffer::Float32 || dataBuffer->dataType() == DataBuffer::Int8 || dataBuffer->dataType() == DataBuffer::Int32) {
            OVITO_ASSERT(usage != VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            BufferReadAccess bufferAccess(dataBuffer);
            // Data types of source and destination are the same. Can do a simple memcpy.
            std::memcpy(p, bufferAccess.cdata(), bufferAccess.size() * bufferAccess.stride());
        }
        else if(dataBuffer->dataType() == DataBuffer::Float64) {
            // Convert from FloatType to float data type.
            BufferReadAccess<double*> bufferAccess(dataBuffer);
            size_t srcStride = dataBuffer->componentCount();
            float* dst = static_cast<float*>(p);
            size_t dstStride = dataBuffer->componentCount();

            // When uploading the data to a SSBO, automatically convert vec3 to vec4, because of the 16-byte alignment requirement of Vulkan.
            if(usage == VK_BUFFER_USAGE_STORAGE_BUFFER_BIT && srcStride == 3)
                dstStride = 4;

            if(dstStride == srcStride && dataBuffer->stride() == sizeof(FloatType) * srcStride) {
                // Strides are the same for source and destination. Need only a single loop for copying.
                for(const auto* src = bufferAccess.cbegin(); src != bufferAccess.cend(); ++src, ++dst)
                    *dst = static_cast<float>(*src);
            }
            else {
                // Strides are the different for source and destination. Need nested loops for copying.
                for(const auto* src = bufferAccess.cbegin(); src != bufferAccess.cend(); src += srcStride, dst += dstStride) {
                    for(size_t i = 0; i < srcStride; i++)
                        dst[i] = static_cast<float>(src[i]);
                }
            }
        }
    });
}

/******************************************************************************
* Uploads an image to the Vulkan device as a texture image.
******************************************************************************/
VkImageView VulkanContext::uploadImage(const QImage& image, ResourceFrameHandle resourceFrame)
{
    OVITO_ASSERT(!image.isNull());
    OVITO_ASSERT(image.format() == QImage::Format_ARGB32 || image.format() == QImage::Format_ARGB32_Premultiplied || image.format() == QImage::Format_RGB32);
    OVITO_ASSERT(logicalDevice());

    // This method must be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == this->thread());

    // Check if this image has already been uploaded to the GPU.
    VulkanImage& textureInfo = lookup<VulkanImage>(image.cacheKey(), resourceFrame);
    if(textureInfo.imageView != VK_NULL_HANDLE)
        return textureInfo.imageView;

    // Prepare new image structure.
    textureInfo.context = this;

    // Determine the required staging buffer size.
    VkDeviceSize bufferSize = image.bytesPerLine() * image.height();
    // Allocate the staging buffer.
    VkBufferCreateInfo bufferCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferCreateInfo.size = bufferSize;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // The buffer will only be used from the graphics queue, so we can stick to exclusive access.
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VkResult err = vmaCreateBuffer(allocator(), &bufferCreateInfo, &allocInfo, &stagingBuffer, &stagingAllocation, nullptr);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(QStringLiteral("Failed to create Vulkan image staging buffer (error code %1).").arg(err));

    // Fill the staging buffer with the image data.
    void* p;
    err = vmaMapMemory(allocator(), stagingAllocation, &p);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(QStringLiteral("Failed to map memory of Vulkan image staging buffer (error code %1).").arg(err));
    memcpy(p, image.constBits(), bufferSize);
    vmaFlushAllocation(allocator(), stagingAllocation, 0, VK_WHOLE_SIZE);
    vmaUnmapMemory(allocator(), stagingAllocation);

    // Create the Vulkan image.
    VkImageCreateInfo imgCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgCreateInfo.extent.width = static_cast<uint32_t>(image.width());
    imgCreateInfo.extent.height = static_cast<uint32_t>(image.height());
    imgCreateInfo.extent.depth = 1;
    imgCreateInfo.arrayLayers = 1;
    imgCreateInfo.mipLevels = 1;
    imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VmaAllocationCreateInfo imgAllocInfo = {};
    imgAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    err = vmaCreateImage(allocator(), &imgCreateInfo, &imgAllocInfo, &textureInfo.image, &textureInfo.allocation, nullptr);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(QStringLiteral("Failed to allocate and create Vulkan texture image (error code %1).").arg(err));

    // Perform upload transfer from staging buffer to destination image.
    immediateTransferSubmit([&](VkCommandBuffer cmdBuf) {
        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        // Perform image layout transition from undefined to destination optimal layout.
        VkImageMemoryBarrier imageTransferBarrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        imageTransferBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageTransferBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageTransferBarrier.image = textureInfo.image;
        imageTransferBarrier.subresourceRange = range;
        imageTransferBarrier.srcAccessMask = 0;
        imageTransferBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        deviceFunctions()->vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageTransferBarrier);
        // Copy the staging buffer into the image.
        VkBufferImageCopy copyRegion = {};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;
        copyRegion.bufferImageHeight = 0;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent = imgCreateInfo.extent;
        deviceFunctions()->vkCmdCopyBufferToImage(cmdBuf, stagingBuffer, textureInfo.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
        // Perform image layout transition from destination optimal to shader readable layout.
        VkImageMemoryBarrier imageTransitionBarrier = imageTransferBarrier;
        imageTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageTransitionBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageTransitionBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deviceFunctions()->vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageTransitionBarrier);
    });

    // Destroy the staging buffer.
    vmaDestroyBuffer(allocator(), stagingBuffer, stagingAllocation);

    // Create the image view.
    VkImageViewCreateInfo imgViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    imgViewInfo.image = textureInfo.image;
    imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgViewInfo.format = imgCreateInfo.format;
    imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_B;
    imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
    imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_R;
    imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
    imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imgViewInfo.subresourceRange.levelCount = imgViewInfo.subresourceRange.layerCount = 1;
    err = deviceFunctions()->vkCreateImageView(logicalDevice(), &imgViewInfo, nullptr, &textureInfo.imageView);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(QStringLiteral("Failed to create Vulkan texture image view (error code %1).").arg(err));

    return textureInfo.imageView;
}

/******************************************************************************
* Creates a new descriptor set from the pool.
******************************************************************************/
VulkanContext::VulkanDescriptorSet VulkanContext::createDescriptorSetImpl(VkDescriptorSetLayout layout)
{
    OVITO_ASSERT(logicalDevice());

    // This method must be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == this->thread());

    // Create a descriptor set.
    VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = _descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;
    VulkanDescriptorSet descriptorSet(this);
    VkResult err = deviceFunctions()->vkAllocateDescriptorSets(logicalDevice(), &allocInfo, &descriptorSet.descriptorSet);
    if(err != VK_SUCCESS)
        throw SceneRenderer::RendererException(tr("Failed to create Vulkan descriptor set (error code %1).").arg(err));

    return descriptorSet;
}

}   // End of namespace
