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

#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/DataBufferAccess.h>
#include <ovito/core/dataset/DataSet.h>
#include "OpenGLSceneRenderer.h"
#include "OpenGLShaderHelper.h"

#include <boost/range/irange.hpp>

namespace Ovito {

/******************************************************************************
* Renders a triangle mesh.
******************************************************************************/
void OpenGLSceneRenderer::renderMeshImplementation(const MeshPrimitive& primitive)
{
    QOpenGLTexture* colorMapTexture = nullptr;

    // Make sure there is something to be rendered. Otherwise, step out early.
	if(!primitive.mesh() || primitive.mesh()->faceCount() == 0)
		return;
    if(primitive.useInstancedRendering() && primitive.perInstanceTMs()->size() == 0)
        return;

	rebindVAO();

    // Render wireframe lines.
    if(primitive.emphasizeEdges() && !isPicking())
        renderMeshWireframeImplementation(primitive);

    // The mesh object to be rendered.
    const TriMeshObject& mesh = *primitive.mesh();

    // Check size limits of the mesh.
    if(mesh.faceCount() > std::numeric_limits<int32_t>::max() / (3 * sizeof(MeshPrimitive::RenderVertex))) {
        qWarning() << "WARNING: OpenGL renderer - Mesh to be rendered has too many faces, exceeding device limits.";
        return;
    }
    if(primitive.useInstancedRendering() && primitive.perInstanceTMs()->size() > std::numeric_limits<int32_t>::max() / (3 * sizeof(Vector_4<float>))) {
        qWarning() << "WARNING: OpenGL renderer - Number of mesh instances to be rendered exceeds device limits.";
        return;
    }

    // Decide whether per-pixel pseudo-color mapping is used.
    bool renderWithPseudoColorMapping = false;
    if(primitive.pseudoColorMapping().isValid() && !isPicking() && !primitive.useInstancedRendering()) {
        if(!mesh.hasVertexColors() && mesh.hasVertexPseudoColors())
            renderWithPseudoColorMapping = true;
        else if(!mesh.hasFaceColors() && mesh.hasFacePseudoColors())
            renderWithPseudoColorMapping = true;
    }

	// Activate the right OpenGL shader program.
	OpenGLShaderHelper shader(this);
    if(!primitive.useInstancedRendering()) {
        if(isPicking())
			shader.load("mesh_picking", "mesh/mesh_picking.vert", "mesh/mesh_picking.frag");
        else if(renderWithPseudoColorMapping)
			shader.load("mesh_color_mapping", "mesh/mesh_color_mapping.vert", "mesh/mesh_color_mapping.frag");
        else
			shader.load("mesh", "mesh/mesh.vert", "mesh/mesh.frag");
        shader.setInstanceCount(1);
    }
    else {
        OVITO_ASSERT(!renderWithPseudoColorMapping); // Note: Color mapping has not been implemented yet for instanced mesh primtives.
        if(!isPicking()) {
            if(!primitive.perInstanceColors())
				shader.load("mesh_instanced", "mesh/mesh_instanced.vert", "mesh/mesh_instanced.frag");
            else
				shader.load("mesh_instanced_with_colors", "mesh/mesh_instanced_with_colors.vert", "mesh/mesh_instanced_with_colors.frag");
        }
        else {
			shader.load("mesh_instanced_picking", "mesh/mesh_instanced_picking.vert", "mesh/mesh_instanced_picking.frag");
        }
        shader.setInstanceCount(primitive.perInstanceTMs()->size());
    }
    shader.setVerticesPerInstance(mesh.faceCount() * 3);

    // Are we rendering a semi-transparent mesh?
    bool useBlending = !isPicking() && !primitive.isFullyOpaque() && !orderIndependentTransparency();
	if(useBlending) shader.enableBlending();

    // Turn back-face culling off if requested.
	if(!primitive.cullFaces()) {
		OVITO_CHECK_OPENGL(this, glDisable(GL_CULL_FACE));
	}

    // Apply optional positive depth-offset to mesh faces to make the wireframe lines fully visible.
	if(primitive.emphasizeEdges() && !isPicking()) {
		OVITO_CHECK_OPENGL(this, glEnable(GL_POLYGON_OFFSET_FILL));
		OVITO_CHECK_OPENGL(this, glPolygonOffset(1.0f, 1.0f));
	}

	// Pass picking base ID to shader.
	if(isPicking()) {
		shader.setPickingBaseId(registerSubObjectIDs(primitive.useInstancedRendering() ? primitive.perInstanceTMs()->size() : mesh.faceCount()));
	}

    // The lookup key for the buffer cache.
    RendererResourceKey<struct MeshBufferCache, DataOORef<const TriMeshObject>, std::vector<ColorA>, ColorA, Color> meshCacheKey{
        primitive.mesh(),
        primitive.materialColors(),
        primitive.uniformColor(),
        primitive.faceSelectionColor()
    };

    // Upload vertex buffer to GPU memory.
    QOpenGLBuffer meshBuffer = shader.createCachedBuffer(std::move(meshCacheKey), sizeof(MeshPrimitive::RenderVertex), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerVertex, [&](void* buffer) {
        bool highlightSelectedFaces = isInteractive() && !isPicking();
    	primitive.generateRenderableVertices(reinterpret_cast<MeshPrimitive::RenderVertex*>(buffer), highlightSelectedFaces, renderWithPseudoColorMapping);
    });

	// Bind vertex buffer to vertex attributes.
	shader.bindBuffer(meshBuffer, "position", GL_FLOAT, 3, sizeof(MeshPrimitive::RenderVertex), offsetof(MeshPrimitive::RenderVertex, position), OpenGLShaderHelper::PerVertex);
    if(!isPicking())
    	shader.bindBuffer(meshBuffer, "normal",   GL_FLOAT, 3, sizeof(MeshPrimitive::RenderVertex), offsetof(MeshPrimitive::RenderVertex, normal),   OpenGLShaderHelper::PerVertex);
        
	if(!renderWithPseudoColorMapping) {
        if(!isPicking() && (!primitive.useInstancedRendering() || !primitive.perInstanceColors())) {
            // Rendering with true RGBA colors.
            shader.bindBuffer(meshBuffer, "color", GL_FLOAT, 4, sizeof(MeshPrimitive::RenderVertex), offsetof(MeshPrimitive::RenderVertex, color), OpenGLShaderHelper::PerVertex);
        }
    }
    else {
        OVITO_ASSERT(!isPicking());
        
        // Rendering  with pseudo-colors and a color mapping function.
        shader.bindBuffer(meshBuffer, "pseudocolor", GL_FLOAT, 2, sizeof(MeshPrimitive::RenderVertex), offsetof(MeshPrimitive::RenderVertex, color), OpenGLShaderHelper::PerVertex);
        shader.setUniformValue("opacity", primitive.uniformColor().a());
        shader.setUniformValue("selection_color", ColorA(primitive.faceSelectionColor()));
        float minValue = primitive.pseudoColorMapping().minValue();
        float maxValue = primitive.pseudoColorMapping().maxValue();
        // Avoid division by zero due to degenerate value interval.
        if(minValue == maxValue) maxValue = std::nextafter(maxValue, std::numeric_limits<float>::max());
        shader.setUniformValue("color_range_min", minValue);
        shader.setUniformValue("color_range_max", maxValue);

        // Upload color map as a 1-d OpenGL texture.
        colorMapTexture = OpenGLResourceManager::instance()->uploadColorMap(primitive.pseudoColorMapping().gradient(), currentResourceFrame());
        colorMapTexture->bind();
    }

    if(primitive.useInstancedRendering()) {
        // Upload the per-instance TMs to GPU memory.
        QOpenGLBuffer instanceTMBuffer = getMeshInstanceTMBuffer(primitive, shader);

        // Bind buffer with the instance matrices.
		shader.bindBuffer(instanceTMBuffer, "instance_tm_row1", GL_FLOAT, 4, 3 * sizeof(Vector_4<float>), 0 * sizeof(Vector_4<float>), OpenGLShaderHelper::PerInstance);
		shader.bindBuffer(instanceTMBuffer, "instance_tm_row2", GL_FLOAT, 4, 3 * sizeof(Vector_4<float>), 1 * sizeof(Vector_4<float>), OpenGLShaderHelper::PerInstance);
		shader.bindBuffer(instanceTMBuffer, "instance_tm_row3", GL_FLOAT, 4, 3 * sizeof(Vector_4<float>), 2 * sizeof(Vector_4<float>), OpenGLShaderHelper::PerInstance);

        if(primitive.perInstanceColors() && !isPicking()) {
            // Upload the per-instance colors to GPU memory.
            QOpenGLBuffer instanceColorBuffer = shader.uploadDataBuffer(primitive.perInstanceColors(), OpenGLShaderHelper::PerInstance, QOpenGLBuffer::VertexBuffer);

            // Bind buffer with the instance colors.
      		shader.bindBuffer(instanceColorBuffer, "instance_color", GL_FLOAT, 4, sizeof(ColorAT<float>), 0, OpenGLShaderHelper::PerInstance);
        }
    }

    if(!useBlending) {
		// Draw triangles in regular storage order (not sorted).
		shader.drawArrays(GL_TRIANGLES);
	}
    else if(primitive.depthSortingMode() == MeshPrimitive::ConvexShapeMode) {
        OVITO_ASSERT(!orderIndependentTransparency() && !isPicking());

        // Assuming that the input mesh is convex, render semi-transparent triangles in two passes: 
        // First, render triangles facing away from the viewer, then render triangles facing toward the viewer.
        // Each time we pass the entire triangle list to OpenGL and use OpenGL's backface/frontfrace culling
        // option to render the right subset of triangles.
        if(!primitive.cullFaces()) {
            // First pass is only needed if backface culling is not active.
			glCullFace(GL_FRONT);
			glEnable(GL_CULL_FACE);
    		shader.drawArrays(GL_TRIANGLES);
        }
        // Now render front-facing triangles only.
		glCullFace(GL_BACK);
		glEnable(GL_CULL_FACE);
        shader.drawArrays(GL_TRIANGLES);
    }
    else if(!primitive.useInstancedRendering()) {
        OVITO_ASSERT(!orderIndependentTransparency() && !isPicking());

        // Create a buffer for an indexed drawing command to render the triangles in back-to-front order. 

        // Viewing direction in object space:
        const Vector3 direction = modelViewTM().inverse().column(2);

        // The caching key for the index buffer.
        RendererResourceKey<struct DepthSortingCache, DataOORef<const TriMeshObject>, Vector3> indexBufferCacheKey{ primitive.mesh(), direction };

        // Create index buffer with three entries per triangle face.
        QOpenGLBuffer indexBuffer = shader.createCachedBuffer(std::move(indexBufferCacheKey), sizeof(GLsizei), QOpenGLBuffer::IndexBuffer, OpenGLShaderHelper::PerVertex, [&](void* buffer) {

            // Compute each face's center point.
            std::vector<Vector_3<float>> faceCenters(mesh.faceCount());
            auto tc = faceCenters.begin();
            for(auto face = mesh.faces().cbegin(); face != mesh.faces().cend(); ++face, ++tc) {
                // Compute centroid of triangle.
                const auto& v1 = mesh.vertex(face->vertex(0));
                const auto& v2 = mesh.vertex(face->vertex(1));
                const auto& v3 = mesh.vertex(face->vertex(2));
                tc->x() = (float)(v1.x() + v2.x() + v3.x()) / 3.0f;
                tc->y() = (float)(v1.y() + v2.y() + v3.y()) / 3.0f;
                tc->z() = (float)(v1.z() + v2.z() + v3.z()) / 3.0f;
            }

            // Next, compute distance of each face from the camera along the viewing direction (=camera z-axis).
            std::vector<FloatType> distances(mesh.faceCount());
            boost::transform(faceCenters, distances.begin(), [direction = direction.toDataType<float>()](const Vector_3<float>& v) {
                return direction.dot(v);
            });

            // Create index array with all face indices.
            std::vector<uint32_t> sortedIndices(mesh.faceCount());
            std::iota(sortedIndices.begin(), sortedIndices.end(), (uint32_t)0);

            // Sort face indices with respect to distance (back-to-front order).
            std::sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] < distances[b];
            });

            // Fill the index buffer with vertex indices to render.
            GLsizei* dst = reinterpret_cast<GLsizei*>(buffer);
            for(uint32_t index : sortedIndices) {
                *dst++ = index * 3;
                *dst++ = index * 3 + 1;
                *dst++ = index * 3 + 2;
            }
        });

        // Bind index buffer.
		if(!indexBuffer.bind())
			throwRendererException(QStringLiteral("Failed to bind OpenGL index buffer for shader '%1'.").arg(shader.shaderObject().objectName()));

        // Draw triangles in sorted order.
		OVITO_CHECK_OPENGL(this, glDrawElements(GL_TRIANGLES, mesh.faceCount() * 3, GL_UNSIGNED_INT, nullptr));

		indexBuffer.release();
    }
	else {
        OVITO_ASSERT(!orderIndependentTransparency() && !isPicking());
        // Render the mesh instances in back-to-front order. 

        // Viewing direction in object space:
        const Vector3 direction = modelViewTM().inverse().column(2);

        // The caching key for the ordering of the primitives.
        RendererResourceKey<struct OrderingCache, ConstDataBufferPtr, Vector3> orderingCacheKey{ primitive.perInstanceTMs(), direction };

        // Render the primitives.
        shader.drawArraysOrdered(GL_TRIANGLES, std::move(orderingCacheKey), [&]() {

            // First, compute distance of each instance from the camera along the viewing direction (=camera z-axis).
            std::vector<FloatType> distances(shader.instanceCount());
			boost::transform(boost::irange<size_t>(0, shader.instanceCount()), distances.begin(), [direction, tmArray = ConstDataBufferAccess<AffineTransformation>(primitive.perInstanceTMs())](size_t i) {
				return direction.dot(tmArray[i].translation());
			});

            // Create index array with all indices.
            std::vector<uint32_t> sortedIndices(shader.instanceCount());
            std::iota(sortedIndices.begin(), sortedIndices.end(), (uint32_t)0);

            // Sort indices with respect to distance (back-to-front order).
            std::sort(sortedIndices.begin(), sortedIndices.end(), [&](uint32_t a, uint32_t b) {
                return distances[a] < distances[b];
            });

            return sortedIndices;
        });
	}

	// Reset depth offset.
	if(primitive.emphasizeEdges() && !isPicking()) {
		glDisable(GL_POLYGON_OFFSET_FILL);
	}

    // Unbind color mapping texture.
    if(colorMapTexture) {
        colorMapTexture->release();
    }
}

/******************************************************************************
* Prepares the OpenGL buffer with the per-instance transformation matrices for 
* rendering a set of meshes.
******************************************************************************/
QOpenGLBuffer OpenGLSceneRenderer::getMeshInstanceTMBuffer(const MeshPrimitive& primitive, OpenGLShaderHelper& shader)
{
    OVITO_ASSERT(primitive.useInstancedRendering());
    OVITO_ASSERT(primitive.perInstanceTMs());

    // The lookup key for storing the per-instance TMs in the cache.
    RendererResourceKey<struct InstanceTMCache, ConstDataBufferPtr> cacheKey(primitive.perInstanceTMs());

    // Upload the per-instance TMs to GPU memory.
    return shader.createCachedBuffer(std::move(cacheKey), 3 * sizeof(Vector_4<float>), QOpenGLBuffer::VertexBuffer, OpenGLShaderHelper::PerInstance, [&](void* buffer) {
        Vector_4<float>* row = reinterpret_cast<Vector_4<float>*>(buffer);
        for(const AffineTransformation& tm : ConstDataBufferAccess<AffineTransformation>(primitive.perInstanceTMs())) {
            *row++ = tm.row(0).toDataType<float>();
            *row++ = tm.row(1).toDataType<float>();
            *row++ = tm.row(2).toDataType<float>();
        }
    });
}

/******************************************************************************
* Generates the wireframe line elements for the visible edges of a mesh.
******************************************************************************/
ConstDataBufferPtr OpenGLSceneRenderer::generateMeshWireframeLines(const MeshPrimitive& primitive)
{
    OVITO_ASSERT(primitive.emphasizeEdges());
    OVITO_ASSERT(primitive.mesh());

    // Cache the wireframe geometry generated for the current mesh.
    RendererResourceKey<struct WireframeCache, DataOORef<const TriMeshObject>> cacheKey(primitive.mesh());
    ConstDataBufferPtr& wireframeLines = OpenGLResourceManager::instance()->lookup<ConstDataBufferPtr>(std::move(cacheKey), currentResourceFrame());

    if(!wireframeLines) {
        wireframeLines = primitive.generateWireframeLines();
    }

    return wireframeLines;
}

/******************************************************************************
* Renders just the edges of a triangle mesh as a wireframe model.
******************************************************************************/
void OpenGLSceneRenderer::renderMeshWireframeImplementation(const MeshPrimitive& primitive)
{
    OVITO_ASSERT(!isPicking());

	OpenGLShaderHelper shader(this);
    if(!primitive.useInstancedRendering())
		shader.load("mesh_wireframe", "mesh/mesh_wireframe.vert", "mesh/mesh_wireframe.frag");
	else
		shader.load("mesh_wireframe_instanced", "mesh/mesh_wireframe_instanced.vert", "mesh/mesh_wireframe_instanced.frag");

    bool useBlending = (primitive.uniformColor().a() < 1.0) && !orderIndependentTransparency();
	if(useBlending) shader.enableBlending();

	// Pass uniform line color to fragment shader as a uniform value.
    ColorA wireframeColor(0.1, 0.1, 0.1, primitive.uniformColor().a());
	shader.setUniformValue("color", wireframeColor);

    // Get the wireframe lines geometry.
    ConstDataBufferPtr wireframeLinesBuffer = generateMeshWireframeLines(primitive);
    shader.setVerticesPerInstance(wireframeLinesBuffer->size());
    shader.setInstanceCount(primitive.useInstancedRendering() ? primitive.perInstanceTMs()->size() : 1);

    if(shader.verticesPerInstance() > std::numeric_limits<int32_t>::max() / shader.instanceCount() / wireframeLinesBuffer->stride()) {
        qWarning() << "WARNING: OpenGL renderer - Wireframe mesh consists of too many lines, exceeding device limits.";
        return;
    }

    // Bind vertex buffer for wireframe vertex positions.
    QOpenGLBuffer buffer = shader.uploadDataBuffer(wireframeLinesBuffer, OpenGLShaderHelper::PerVertex, QOpenGLBuffer::VertexBuffer);
	shader.bindBuffer(buffer, "position", GL_FLOAT, 3, sizeof(Point_3<float>), 0, OpenGLShaderHelper::PerVertex);

    // Bind vertex buffer for instance TMs.
    if(primitive.useInstancedRendering()) {
        // Upload the per-instance TMs to GPU memory.
        QOpenGLBuffer instanceTMBuffer = getMeshInstanceTMBuffer(primitive, shader);
        // Bind buffer with the instance matrices.
		shader.bindBuffer(instanceTMBuffer, "instance_tm_row1", GL_FLOAT, 4, 3 * sizeof(Vector_4<float>), 0 * sizeof(Vector_4<float>), OpenGLShaderHelper::PerInstance);
		shader.bindBuffer(instanceTMBuffer, "instance_tm_row2", GL_FLOAT, 4, 3 * sizeof(Vector_4<float>), 1 * sizeof(Vector_4<float>), OpenGLShaderHelper::PerInstance);
		shader.bindBuffer(instanceTMBuffer, "instance_tm_row3", GL_FLOAT, 4, 3 * sizeof(Vector_4<float>), 2 * sizeof(Vector_4<float>), OpenGLShaderHelper::PerInstance);
    }

    // Draw lines.
	shader.drawArrays(GL_LINES);
}

}	// End of namespace
