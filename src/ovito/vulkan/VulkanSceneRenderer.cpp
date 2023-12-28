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
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/DataSetContainer.h>
#include <ovito/core/app/UserInterface.h>
#include <ovito/core/viewport/Viewport.h>
#include <ovito/core/viewport/ViewportConfiguration.h>
#include <ovito/core/rendering/RenderSettings.h>
#include "VulkanSceneRenderer.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(VulkanSceneRenderer);

/******************************************************************************
* Is called by OVITO to query the class for any information that should be
* included in the application's system report.
******************************************************************************/
void VulkanSceneRenderer::OOMetaClass::querySystemInformation(QTextStream& stream, UserInterface& userInterface) const
{
    if(this == &VulkanSceneRenderer::OOClass()) {
        stream << "======== Vulkan info =======" << "\n";
        try {
            // Look up an existing Vulkan context from one of the interactive viewport windows.
            // All viewport windows share a single logical Vulkan device.
            std::shared_ptr<VulkanContext> context;
            if(DataSet* dataset = userInterface.datasetContainer().currentSet()) {
                for(Viewport* vp : dataset->viewportConfig()->viewports()) {
                    if(ViewportWindowInterface* window = vp->window()) {
                        if(VulkanSceneRenderer* renderer = dynamic_object_cast<VulkanSceneRenderer>(window->sceneRenderer())) {
                            context = renderer->context();
                            break;
                        }
                    }
                }
            }

            // Create an adhoc instance of VulkanContext if needed.
            if(!context)
                context = std::make_shared<VulkanContext>();

            stream << "Number of physical devices: " << context->availablePhysicalDevices().count() << "\n";
            uint32_t deviceIndex = 0;

            // Write the list of physical devices also to the application settings store, so that
            // the GeneralSettingsPage class from the GUI module can read it without direct access to the Vulkan plugin.
            QSettings settings;
            settings.beginGroup("rendering/vulkan");
            settings.beginWriteArray("available_devices");
            for(const VkPhysicalDeviceProperties& props : context->availablePhysicalDevices()) {
                stream << tr("[%8] %1 - Version %2.%3.%4 - API Version %5.%6.%7\n")
                                    .arg(props.deviceName)
                                    .arg(VK_VERSION_MAJOR(props.driverVersion)).arg(VK_VERSION_MINOR(props.driverVersion))
                                    .arg(VK_VERSION_PATCH(props.driverVersion))
                                    .arg(VK_VERSION_MAJOR(props.apiVersion)).arg(VK_VERSION_MINOR(props.apiVersion))
                                    .arg(VK_VERSION_PATCH(props.apiVersion))
                                    .arg(deviceIndex);
                settings.setArrayIndex(deviceIndex);
                settings.setValue("name", QString::fromUtf8(props.deviceName));
                settings.setValue("vendorID", props.vendorID);
                settings.setValue("deviceID", props.deviceID);
                settings.setValue("deviceType", static_cast<uint32_t>(props.deviceType));
                deviceIndex++;
            }
            settings.endArray();
            settings.setValue("selected_device", context->physicalDeviceIndex());

            if(context->logicalDevice()) {
                stream << "Active physical device index: [" << context->physicalDeviceIndex() << "]\n";
                stream << "Unified memory architecture: " << context->isUMA() << "\n";
                stream << "features.wideLines: " << context->supportsWideLines() << "\n";
                stream << "features.multiDrawIndirect: " << context->supportsMultiDrawIndirect() << "\n";
                stream << "features.drawIndirectFirstInstance: " << context->supportsDrawIndirectFirstInstance() << "\n";
                stream << "features.extendedDynamicState: " << context->supportsExtendedDynamicState() << "\n";
                stream << "limits.maxUniformBufferRange: " << context->physicalDeviceProperties()->limits.maxUniformBufferRange << "\n";
                stream << "limits.maxStorageBufferRange: " << context->physicalDeviceProperties()->limits.maxStorageBufferRange << "\n";
                stream << "limits.maxPushConstantsSize: " << context->physicalDeviceProperties()->limits.maxPushConstantsSize << "\n";
                stream << "limits.lineWidthRange: " << context->physicalDeviceProperties()->limits.lineWidthRange[0] << " - " << context->physicalDeviceProperties()->limits.lineWidthRange[1] << "\n";
                stream << "limits.lineWidthGranularity: " << context->physicalDeviceProperties()->limits.lineWidthGranularity << "\n";
                stream << "limits.maxDrawIndirectCount: " << context->physicalDeviceProperties()->limits.maxDrawIndirectCount << "\n";
            }
            else stream << "No active physical device\n";
        }
        catch(const Exception& ex) {
            stream << tr("Error: %1").arg(ex.message()) << "\n";
        }
    }
}

/******************************************************************************
* Constructor.
******************************************************************************/
VulkanSceneRenderer::VulkanSceneRenderer(ObjectInitializationFlags flags, std::shared_ptr<VulkanContext> vulkanContext, int concurrentFrameCount)
    : SceneRenderer(flags),
    _context(std::move(vulkanContext)),
    _concurrentFrameCount(concurrentFrameCount)
{
    OVITO_ASSERT(_context);
    OVITO_ASSERT(_concurrentFrameCount >= 1);

    // Release our own Vulkan resources before the logical device gets destroyed.
    connect(context().get(), &VulkanContext::releaseResourcesRequested, this, &VulkanSceneRenderer::releaseVulkanDeviceResources);
}

/******************************************************************************
* Destructor.
******************************************************************************/
VulkanSceneRenderer::~VulkanSceneRenderer()
{
    // Verify that all Vulkan resources have already been released thanks to a call to aboutToBeDeleted().
    OVITO_ASSERT(_resourcesInitialized == false);
}

/******************************************************************************
* This method is called after the reference counter of this object has reached zero
* and before the object is being finally deleted.
******************************************************************************/
void VulkanSceneRenderer::aboutToBeDeleted()
{
    // Release any Vulkan resources managed by the renderer.
    releaseVulkanDeviceResources();

    SceneRenderer::aboutToBeDeleted();
}

/******************************************************************************
* Creates the Vulkan resources needed by this renderer.
******************************************************************************/
void VulkanSceneRenderer::initResources()
{
    // Create the resources of the rendering primitives.
    if(!_resourcesInitialized) {
        initImagePrimitivePipelines();
        _resourcesInitialized = true;
    }
}

/******************************************************************************
* This method is called just before renderFrame() is called.
******************************************************************************/
void VulkanSceneRenderer::beginFrame(AnimationTime time, Scene* scene, const ViewProjectionParameters& params, Viewport* vp, const QRect& viewportRect, FrameBuffer* frameBuffer)
{
    // Convert viewport rect from logical device coordinates to Vulkan framebuffer coordinates.
    QRect vulkanViewportRect(viewportRect.x() * antialiasingLevel(), viewportRect.y() * antialiasingLevel(), viewportRect.width() * antialiasingLevel(), viewportRect.height() * antialiasingLevel());

    SceneRenderer::beginFrame(time, scene, params, vp, vulkanViewportRect, frameBuffer);

    // This method may only be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == context()->thread());

    // Make sure our Vulkan objects have been created.
    initResources();

    // Specify dynamic Vulkan viewport area.
    VkViewport viewport;
    viewport.x = vulkanViewportRect.x();
    viewport.y = vulkanViewportRect.y();
    viewport.width = vulkanViewportRect.width();
    viewport.height = vulkanViewportRect.height();
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    deviceFunctions()->vkCmdSetViewport(currentCommandBuffer(), 0, 1, &viewport);

    // Specify dynamic Vulkan scissor rectangle.
    VkRect2D scissor;
    scissor.offset.x = vulkanViewportRect.x();
    scissor.offset.y = vulkanViewportRect.y();
    scissor.extent.width = vulkanViewportRect.width();
    scissor.extent.height = vulkanViewportRect.height();
    deviceFunctions()->vkCmdSetScissor(currentCommandBuffer(), 0, 1, &scissor);

    // Enable depth tests by default.
    setDepthTestEnabled(true);
}

/******************************************************************************
* Renders the current animation frame.
******************************************************************************/
bool VulkanSceneRenderer::renderFrame(const QRect& viewportRect)
{
    // Render the 3D scene objects.
    if(renderScene()) {

        // Call virtual method to render additional content that is only visible in the interactive viewports.
        if(viewport() && isInteractive()) {
            renderInteractiveContent();
        }

        // Render translucent objects in a second pass.
        for(const auto& [tm, primitive] : _translucentParticles) {
            setWorldTransform(tm);
            renderParticlesImplementation(primitive);
        }
        _translucentParticles.clear();
        for(const auto& [tm, primitive] : _translucentCylinders) {
            setWorldTransform(tm);
            renderCylindersImplementation(primitive);
        }
        _translucentCylinders.clear();
        for(const auto& [tm, primitive] : _translucentMeshes) {
            setWorldTransform(tm);
            renderMeshImplementation(primitive);
        }
        _translucentMeshes.clear();
    }

    return !this_task::isCanceled();
}

/******************************************************************************
* Renders the overlays/underlays of the viewport into the framebuffer.
******************************************************************************/
bool VulkanSceneRenderer::renderOverlays(bool underlays, const QRect& logicalViewportRect, const QRect& physicalViewportRect)
{
    // Convert viewport rect from logical device coordinates to OpenGL framebuffer coordinates.
    QRect vulkanViewportRect(physicalViewportRect.x() * antialiasingLevel(), physicalViewportRect.y() * antialiasingLevel(), physicalViewportRect.width() * antialiasingLevel(), physicalViewportRect.height() * antialiasingLevel());

    // Delegate rendering work to base class.
    return SceneRenderer::renderOverlays(underlays, logicalViewportRect, vulkanViewportRect);
}

/******************************************************************************
* Temporarily enables/disables the depth test while rendering.
******************************************************************************/
void VulkanSceneRenderer::setDepthTestEnabled(bool enabled)
{
    _depthTestEnabled = enabled;
}

/******************************************************************************
* Activates the special highlight rendering mode.
******************************************************************************/
void VulkanSceneRenderer::setHighlightMode(int pass)
{
}

/******************************************************************************
* Releases all Vulkan resources held by the renderer class.
******************************************************************************/
void VulkanSceneRenderer::releaseVulkanDeviceResources()
{
    // This method may only be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == context()->thread());

    if(!_resourcesInitialized)
        return;

    OVITO_ASSERT(deviceFunctions());

    // Destroy the resources of the rendering primitives.
    releaseLinePrimitivePipelines();
    releaseParticlePrimitivePipelines();
    releaseCylinderPrimitivePipelines();
    releaseMeshPrimitivePipelines();
    releaseImagePrimitivePipelines();

    if(_globalUniformsDescriptorSetLayout != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyDescriptorSetLayout(logicalDevice(), _globalUniformsDescriptorSetLayout, nullptr);
        _globalUniformsDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if(_colorMapDescriptorSetLayout != VK_NULL_HANDLE) {
        deviceFunctions()->vkDestroyDescriptorSetLayout(logicalDevice(), _colorMapDescriptorSetLayout, nullptr);
        _colorMapDescriptorSetLayout = VK_NULL_HANDLE;
    }

    _resourcesInitialized = false;
}

/******************************************************************************
* Renders a line primitive.
******************************************************************************/
void VulkanSceneRenderer::renderLines(const LinePrimitive& primitive)
{
    OVITO_ASSERT(!isBoundingBoxPass());
    renderLinesImplementation(primitive);
}

/******************************************************************************
* Renders a particle primitive.
******************************************************************************/
void VulkanSceneRenderer::renderParticles(const ParticlePrimitive& primitive)
{
    OVITO_ASSERT(!isBoundingBoxPass());

    // Render primitives now if they are all fully opaque. Otherwise defer rendering to a later time to
    // draw the semi-transparent objects after everything else has been drawn.
    if(isPicking() || !primitive.transparencies())
        renderParticlesImplementation(primitive);
    else
        _translucentParticles.emplace_back(worldTransform(), primitive);
}

/******************************************************************************
* Renders a cylinder primitive.
******************************************************************************/
void VulkanSceneRenderer::renderCylinders(const CylinderPrimitive& primitive)
{
    OVITO_ASSERT(!isBoundingBoxPass());

    // Render primitives now if they are all fully opaque. Otherwise defer rendering to a later time to
    // draw the semi-transparent objects after everything else has been drawn.
    if(isPicking() || !primitive.transparencies())
        renderCylindersImplementation(primitive);
    else
        _translucentCylinders.emplace_back(worldTransform(), primitive);
}

/******************************************************************************
* Renders a mesh primitive.
******************************************************************************/
void VulkanSceneRenderer::renderMesh(const MeshPrimitive& primitive)
{
    OVITO_ASSERT(!isBoundingBoxPass());

    // Render primitives now if they are all fully opaque. Otherwise defer rendering to a later time to
    // draw the semi-transparent objects after everything else has been drawn.
    if(isPicking() || primitive.isFullyOpaque())
        renderMeshImplementation(primitive);
    else
        _translucentMeshes.emplace_back(worldTransform(), primitive);
}

/******************************************************************************
* Renders an image primitive.
******************************************************************************/
void VulkanSceneRenderer::renderImage(const ImagePrimitive& primitive)
{
    OVITO_ASSERT(!isBoundingBoxPass());
    renderImageImplementation(primitive);
}

/******************************************************************************
* Renders a text primitive.
******************************************************************************/
void VulkanSceneRenderer::renderText(const TextPrimitive& primitive)
{
    OVITO_ASSERT(!isBoundingBoxPass());
    renderTextDefaultImplementation(primitive);
}

/******************************************************************************
* Returns the descriptor set layout for the global uniforms buffer.
******************************************************************************/
VkDescriptorSetLayout VulkanSceneRenderer::globalUniformsDescriptorSetLayout()
{
    if(_globalUniformsDescriptorSetLayout == VK_NULL_HANDLE) {

        // Specify the descriptor layout binding.
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = 0;
        layoutBinding.descriptorCount = 1;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        // Create descriptor set layout.
        VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &layoutBinding;
        VkResult err = deviceFunctions()->vkCreateDescriptorSetLayout(logicalDevice(), &layoutInfo, nullptr, &_globalUniformsDescriptorSetLayout);
        if(err != VK_SUCCESS)
            throw RendererException(QStringLiteral("Failed to create Vulkan descriptor set layout (error code %1).").arg(err));
    }

    return _globalUniformsDescriptorSetLayout;
}

/******************************************************************************
* Returns the descriptor set layout for the color gradient maps.
******************************************************************************/
VkDescriptorSetLayout VulkanSceneRenderer::colorMapDescriptorSetLayout()
{
    if(_colorMapDescriptorSetLayout == VK_NULL_HANDLE) {

        // Specify the descriptor layout binding.
        VkDescriptorSetLayoutBinding layoutBinding = {};
        layoutBinding.binding = 0;
        layoutBinding.descriptorCount = 1;
        layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        // Create descriptor set layout.
        VkDescriptorSetLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &layoutBinding;
        VkResult err = deviceFunctions()->vkCreateDescriptorSetLayout(logicalDevice(), &layoutInfo, nullptr, &_colorMapDescriptorSetLayout);
        if(err != VK_SUCCESS)
            throw RendererException(QStringLiteral("Failed to create Vulkan descriptor set layout (error code %1).").arg(err));
    }

    return _colorMapDescriptorSetLayout;
}

/******************************************************************************
* Returns the Vulkan descriptor set for the global uniforms structure, which
* can be bound to a pipeline.
******************************************************************************/
VkDescriptorSet VulkanSceneRenderer::getGlobalUniformsDescriptorSet()
{
    // Update the information in the uniforms data structure.
    GlobalUniforms uniforms;
    uniforms.projectionMatrix = (clipCorrection() * projParams().projectionMatrix).toDataType<float>();
    uniforms.inverseProjectionMatrix = (projParams().inverseProjectionMatrix * clipCorrection().inverse()).toDataType<float>();
    uniforms.viewportOrigin = Point_2<float>(0,0);
    uniforms.inverseViewportSize = Vector_2<float>(2.0f / (float)frameBufferSize().width(), 2.0f / (float)frameBufferSize().height());
    uniforms.znear = static_cast<float>(projParams().znear);
    uniforms.zfar = static_cast<float>(projParams().zfar);

    // Upload uniforms buffer to GPU memory (only if it has changed).
    VkBuffer uniformsBuffer = context()->createCachedBuffer(uniforms, sizeof(uniforms), currentResourceFrame(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, [&](void* buffer) {
        memcpy(buffer, &uniforms, sizeof(uniforms));
    });

    // Use the VkBuffer as strongly-typed cache key to look up descriptor set.
    RendererResourceKey<GlobalUniforms, VkBuffer> cacheKey{ uniformsBuffer };

    // Create the descriptor set (only if a new Vulkan buffer has been created).
    std::pair<VkDescriptorSet, bool> descriptorSet = context()->createDescriptorSet(globalUniformsDescriptorSetLayout(), cacheKey, currentResourceFrame());

    // Initialize the descriptor set if it was newly created.
    if(descriptorSet.second) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformsBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE ;
        VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptorWrite.dstSet = descriptorSet.first;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        deviceFunctions()->vkUpdateDescriptorSets(logicalDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    return descriptorSet.first;
}

/******************************************************************************
* Uploads a color coding map to the Vulkan device as a uniforms buffer.
******************************************************************************/
VkDescriptorSet VulkanSceneRenderer::uploadColorMap(ColorCodingGradient* gradient)
{
    OVITO_ASSERT(gradient);
    OVITO_ASSERT(logicalDevice());

    // This method must be called from the main thread where the Vulkan device lives.
    OVITO_ASSERT(QThread::currentThread() == this->thread());

    // Sampling resolution of the tabulated color gradient.
    constexpr int resolution = 256;

    // Upload tabulated color gradient to GPU memory (only if it has changed).
    VkBuffer uniformsBuffer = context()->createCachedBuffer(RendererResourceKey<VulkanContext, OORef<ColorCodingGradient>>{gradient}, sizeof(ColorAT<float>) * resolution, currentResourceFrame(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, [&](void* buffer) {
        ColorAT<float>* table = static_cast<ColorAT<float>*>(buffer);
        for(int x = 0; x < resolution; x++)
            table[x] = gradient->valueToColor((FloatType)x / (resolution - 1)).toDataType<float>();
    });

    // Create the descriptor set (only if a new Vulkan buffer has been created).
    std::pair<VkDescriptorSet, bool> descriptorSet = context()->createDescriptorSet(colorMapDescriptorSetLayout(), RendererResourceKey<ColorCodingGradient, VkBuffer>{uniformsBuffer}, currentResourceFrame());

    // Initialize the descriptor set if it was newly created.
    if(descriptorSet.second) {
        VkDescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformsBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet descriptorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        descriptorWrite.dstSet = descriptorSet.first;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        deviceFunctions()->vkUpdateDescriptorSets(logicalDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    return descriptorSet.first;
}

}   // End of namespace
