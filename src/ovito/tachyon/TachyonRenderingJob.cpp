////////////////////////////////////////////////////////////////////////////////////////
//
//  Tachyon renderer plugin for OVITO.
//
////////////////////////////////////////////////////////////////////////////////////////

#include <ovito/core/Core.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/rendering/CylinderPrimitive.h>
#include <ovito/core/rendering/FrameBuffer.h>
#include <ovito/core/rendering/ImagePrimitive.h>
#include <ovito/core/rendering/LinePrimitive.h>
#include <ovito/core/rendering/MeshPrimitive.h>
#include <ovito/core/rendering/ParticlePrimitive.h>
#include <ovito/core/rendering/VolumePrimitive.h>
#include "TachyonRenderBuffer.h"
#include "TachyonRenderingJob.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <set>
#include <unordered_map>

extern "C" {
#include <tachyon.h>
}

namespace Ovito {
namespace {

std::once_flag tachyonInitFlag;
std::mutex tachyonRenderMutex;

void ensureTachyonInitialized()
{
    std::call_once(tachyonInitFlag, []() {
        int argc = 1;
        char arg0[] = "ovito-tachyon";
        char* argvArray[] = { arg0, nullptr };
        char** argv = argvArray;
        if(rt_initialize(&argc, &argv) < 0)
            throw RendererException(QObject::tr("Failed to initialize the Tachyon ray-tracing library."));
    });
}

FloatType clamp01(FloatType value)
{
    return std::clamp(value, FloatType(0), FloatType(1));
}

apivector toApiVector(const Point3& p)
{
    return rt_vector(p.x(), p.y(), p.z());
}

apivector toApiVector(const Vector3& v)
{
    return rt_vector(v.x(), v.y(), v.z());
}

apicolor toApiColor(const ColorA& c)
{
    return rt_color(clamp01(c.r()), clamp01(c.g()), clamp01(c.b()));
}

std::array<float, 3> toFloat3(const Point3& p)
{
    return { static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()) };
}

std::array<float, 3> toFloat3(const Vector3& v)
{
    return { static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()) };
}

ColorA colorWithOpacity(const Color& color, FloatType opacity)
{
    return ColorA(color.r(), color.g(), color.b(), clamp01(opacity));
}

ColorA readRgbColor(const ConstDataBufferPtr& buffer, size_t index, const ColorA& fallback)
{
    if(!buffer || buffer->componentCount() != 3)
        return fallback;

    if(buffer->dataType() == DataBuffer::Float32) {
        const ColorF& c = BufferReadAccess<ColorF>(buffer)[index];
        return ColorA(c.r(), c.g(), c.b(), fallback.a());
    }
    if(buffer->dataType() == DataBuffer::Float64) {
        const Color& c = BufferReadAccess<Color>(buffer)[index];
        return ColorA(c.r(), c.g(), c.b(), fallback.a());
    }
    return fallback;
}

ColorA readRgbaColor(const ConstDataBufferPtr& buffer, size_t index, const ColorA& fallback)
{
    if(!buffer || buffer->componentCount() != 4)
        return fallback;

    if(buffer->dataType() == DataBuffer::Float32) {
        const ColorAF& c = BufferReadAccess<ColorAF>(buffer)[index];
        return ColorA(c.r(), c.g(), c.b(), c.a());
    }
    if(buffer->dataType() == DataBuffer::Float64) {
        const ColorA& c = BufferReadAccess<ColorA>(buffer)[index];
        return c;
    }
    return fallback;
}

FloatType readScalar(const ConstDataBufferPtr& buffer, size_t index, FloatType fallback)
{
    if(!buffer || buffer->componentCount() != 1)
        return fallback;

    if(buffer->dataType() == DataBuffer::Float32)
        return BufferReadAccess<float>(buffer)[index];
    if(buffer->dataType() == DataBuffer::Float64)
        return BufferReadAccess<double>(buffer)[index];
    return fallback;
}

bool readSelection(const ConstDataBufferPtr& buffer, size_t index)
{
    return buffer && BufferReadAccess<SelectionIntType>(buffer)[index] != 0;
}

uint64_t textureKey(const ColorA& color)
{
    auto byte = [](FloatType v) -> uint64_t {
        return static_cast<uint64_t>(std::lround(clamp01(v) * 255.0));
    };
    return (byte(color.r()) << 24) | (byte(color.g()) << 16) | (byte(color.b()) << 8) | byte(color.a());
}

class TachyonSceneBuilder
{
public:

    TachyonSceneBuilder(SceneHandle scene, const TachyonRenderer& renderer, RenderBuffer& renderBuffer) :
        _scene(scene), _renderer(renderer), _renderBuffer(renderBuffer) {}

    void renderPrimitive(const FrameGraph::RenderingCommand& command)
    {
        if(command.skipInVisualPass() || !command.primitive())
            return;

        if(const ParticlePrimitive* primitive = dynamic_cast<const ParticlePrimitive*>(command.primitive()))
            renderParticles(*primitive, command.modelWorldTM());
        else if(const CylinderPrimitive* primitive = dynamic_cast<const CylinderPrimitive*>(command.primitive()))
            renderCylinders(*primitive, command.modelWorldTM());
        else if(const MeshPrimitive* primitive = dynamic_cast<const MeshPrimitive*>(command.primitive()))
            renderMesh(*primitive, command.modelWorldTM());
        else if(const LinePrimitive* primitive = dynamic_cast<const LinePrimitive*>(command.primitive()))
            renderLines(*primitive, command.modelWorldTM());
        else if(dynamic_cast<const VolumePrimitive*>(command.primitive()))
            reportOnce("Tachyon renderer does not currently support volume primitives.");
    }

private:

    void* textureFor(ColorA color)
    {
        color.a() = clamp01(color.a());
        const uint64_t key = textureKey(color);
        auto existing = _textures.find(key);
        if(existing != _textures.end())
            return existing->second;

        apitexture tex = {};
        tex.texturefunc = RT_TEXTURE_CONSTANT;
        tex.col = toApiColor(color);
        tex.shadowcast = _renderer.shadows() ? 1 : 0;
        tex.ambient = _renderer.materialAmbient();
        tex.diffuse = _renderer.materialDiffuse();
        tex.specular = 0.0; // Tachyon specular is mirror reflection, not an OpenGL-style highlight.
        tex.opacity = color.a();
        tex.scale = rt_vector(1, 1, 1);
        void* handle = rt_texture(_scene, &tex);
        rt_tex_phong(handle, _renderer.materialSpecular(), 40.0, 0);
        _textures.emplace(key, handle);
        return handle;
    }

    void renderParticles(const ParticlePrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.positions() || primitive.positions()->size() == 0)
            return;

        if(primitive.particleShape() != ParticlePrimitive::SphericalShape)
            reportOnce("Tachyon renderer currently renders non-spherical particle glyphs as spheres.");

        auto renderPositions = [&](auto _) {
            using T = decltype(_);
            BufferReadAccess<Point_3<T>> positions(primitive.positions());

            auto renderParticle = [&](size_t sourceIndex) {
                const Point3 pos = tm * positions[sourceIndex].template toDataType<FloatType>();
                const FloatType radius = std::max(FloatType(0), readScalar(primitive.radii(), sourceIndex, primitive.uniformRadius()));
                if(radius <= 0)
                    return;

                ColorA color = colorWithOpacity(primitive.uniformColor(), 1);
                color = readRgbColor(primitive.colors(), sourceIndex, color);
                if(primitive.selection() && readSelection(primitive.selection(), sourceIndex))
                    color = colorWithOpacity(primitive.selectionColor(), color.a());
                if(primitive.transparencies())
                    color.a() *= clamp01(FloatType(1) - readScalar(primitive.transparencies(), sourceIndex, 0));
                if(color.a() <= 0)
                    return;

                rt_sphere(_scene, textureFor(color), toApiVector(pos), radius);
            };

            if(primitive.indices()) {
                const BufferReadAccess<int32_t> indices(primitive.indices());
                for(int32_t sourceIndex : indices)
                    renderParticle(static_cast<size_t>(sourceIndex));
            }
            else {
                for(size_t i = 0; i < positions.size(); ++i)
                    renderParticle(i);
            }
        };

        primitive.positions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(renderPositions);
    }

    void renderCylinders(const CylinderPrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.basePositions() || !primitive.headPositions() || primitive.basePositions()->size() == 0)
            return;
        if(primitive.colors() && primitive.colors()->componentCount() == 1)
            reportOnce("Tachyon renderer currently ignores one-component pseudo-colors on cylinders.");

        auto renderPositions = [&](auto _) {
            using T = decltype(_);
            BufferReadAccess<Point_3<T>> bases(primitive.basePositions());
            BufferReadAccess<Point_3<T>> heads(primitive.headPositions());

            for(size_t i = 0; i < bases.size(); ++i) {
                Point3 base = tm * bases[i].template toDataType<FloatType>();
                Point3 head = tm * heads[i].template toDataType<FloatType>();
                Vector3 axis = head - base;
                const FloatType length = axis.length();
                if(length <= 0)
                    continue;

                ColorA color = colorWithOpacity(primitive.uniformColor(), 1);
                color = readRgbColor(primitive.colors(), i, color);
                if(primitive.selection() && readSelection(primitive.selection(), i))
                    color = colorWithOpacity(primitive.selectionColor(), color.a());
                if(primitive.transparencies())
                    color.a() *= clamp01(FloatType(1) - readScalar(primitive.transparencies(), i, 0));
                if(color.a() <= 0)
                    continue;

                const FloatType radius = std::max(FloatType(0), readScalar(primitive.widths(), i, primitive.uniformWidth()) / FloatType(2));
                if(radius <= 0)
                    continue;

                if(primitive.shape() == CylinderPrimitive::ArrowShape)
                    reportOnce("Tachyon renderer currently renders arrow glyphs as cylinders.");

                rt_fcylinder(_scene, textureFor(color), toApiVector(base), toApiVector(axis), radius);
            }
        };

        primitive.basePositions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(renderPositions);
    }

    void renderMesh(const MeshPrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.mesh() || primitive.faceCount() == 0)
            return;
        if(primitive.useInstancedRendering()) {
            reportOnce("Tachyon renderer currently renders only the base mesh for instanced mesh primitives.");
        }
        if(primitive.mesh()->hasVertexPseudoColors() || primitive.mesh()->hasFacePseudoColors())
            reportOnce("Tachyon renderer currently ignores mesh pseudo-color mappings.");

        std::vector<MeshPrimitive::RenderVertex> vertices(static_cast<size_t>(primitive.faceCount()) * 3);
        primitive.generateRenderableVertices(vertices, false, false);
        ColorA fallback = primitive.uniformColor();

        for(size_t i = 0; i + 2 < vertices.size(); i += 3) {
            const Point3 p0 = tm * vertices[i].position.template toDataType<FloatType>();
            const Point3 p1 = tm * vertices[i + 1].position.template toDataType<FloatType>();
            const Point3 p2 = tm * vertices[i + 2].position.template toDataType<FloatType>();

            Vector3 n0 = tm.linear() * vertices[i].normal.template toDataType<FloatType>();
            Vector3 n1 = tm.linear() * vertices[i + 1].normal.template toDataType<FloatType>();
            Vector3 n2 = tm.linear() * vertices[i + 2].normal.template toDataType<FloatType>();
            if(!n0.normalizeSafely() || !n1.normalizeSafely() || !n2.normalizeSafely()) {
                Vector3 faceNormal = (p1 - p0).cross(p2 - p0);
                if(!faceNormal.normalizeSafely())
                    continue;
                n0 = n1 = n2 = faceNormal;
            }

            ColorA c0 = vertices[i].color.template toDataType<FloatType>();
            ColorA c1 = vertices[i + 1].color.template toDataType<FloatType>();
            ColorA c2 = vertices[i + 2].color.template toDataType<FloatType>();
            if(c0.a() <= 0 && c1.a() <= 0 && c2.a() <= 0)
                continue;
            const FloatType opacity = clamp01((c0.a() + c1.a() + c2.a()) / FloatType(3));
            fallback.a() = opacity;

            const auto fp0 = toFloat3(p0);
            const auto fp1 = toFloat3(p1);
            const auto fp2 = toFloat3(p2);
            const auto fn0 = toFloat3(n0);
            const auto fn1 = toFloat3(n1);
            const auto fn2 = toFloat3(n2);
            const std::array<float, 3> fc0 = { static_cast<float>(clamp01(c0.r())), static_cast<float>(clamp01(c0.g())), static_cast<float>(clamp01(c0.b())) };
            const std::array<float, 3> fc1 = { static_cast<float>(clamp01(c1.r())), static_cast<float>(clamp01(c1.g())), static_cast<float>(clamp01(c1.b())) };
            const std::array<float, 3> fc2 = { static_cast<float>(clamp01(c2.r())), static_cast<float>(clamp01(c2.g())), static_cast<float>(clamp01(c2.b())) };
            rt_vcstri3fv(_scene, textureFor(fallback), fp0.data(), fp1.data(), fp2.data(), fn0.data(), fn1.data(), fn2.data(), fc0.data(), fc1.data(), fc2.data());
        }

        if(primitive.emphasizeEdges()) {
            ConstDataBufferPtr wireframe = primitive.generateWireframeLines();
            LinePrimitive linePrimitive;
            linePrimitive.setPositions(wireframe);
            linePrimitive.setUniformColor(primitive.wireframeColor());
            linePrimitive.setLineWidth(primitive.wireframeWidth());
            renderLines(linePrimitive, tm);
        }
    }

    void renderLines(const LinePrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.positions() || primitive.positions()->size() < 2)
            return;

        auto renderPositions = [&](auto _) {
            using T = decltype(_);
            BufferReadAccess<Point_3<T>> points(primitive.positions());
            const FloatType fallbackRadius = std::max(FloatType(0.01), primitive.lineWidth() * FloatType(0.25));

            for(size_t i = 0; i + 1 < points.size(); i += 2) {
                Point3 base = tm * points[i].template toDataType<FloatType>();
                Point3 head = tm * points[i + 1].template toDataType<FloatType>();
                Vector3 axis = head - base;
                if(axis.length() <= 0)
                    continue;

                ColorA color = primitive.uniformColor();
                color = readRgbaColor(primitive.colors(), i, color);
                if(color.a() <= 0)
                    continue;

                rt_fcylinder(_scene, textureFor(color), toApiVector(base), toApiVector(axis), fallbackRadius);
            }
        };

        primitive.positions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(renderPositions);
    }

    void reportOnce(const char* message)
    {
        if(_reportedIssues.insert(message).second)
            _renderBuffer.reportIssue(QString::fromUtf8(message));
    }

private:

    SceneHandle _scene;
    const TachyonRenderer& _renderer;
    RenderBuffer& _renderBuffer;
    std::unordered_map<uint64_t, void*> _textures;
    std::set<QByteArray> _reportedIssues;
};

void configureCamera(SceneHandle scene, const ViewProjectionParameters& projectionParams, const QSize& size)
{
    const AffineTransformation& inv = projectionParams.inverseViewMatrix;
    Point3 center = Point3::Origin() + inv.translation();
    Vector3 viewDir = inv.linear() * Vector3(0, 0, -1);
    Vector3 upDir = inv.linear() * Vector3(0, 1, 0);
    if(!viewDir.normalizeSafely())
        viewDir = Vector3(0, 0, -1);
    if(!upDir.normalizeSafely())
        upDir = Vector3(0, 1, 0);

    rt_camera_position(scene, toApiVector(center), toApiVector(viewDir), toApiVector(upDir));
    rt_aspectratio(scene, 1.0f);

    const FloatType widthOverHeight = size.height() > 0 ? FloatType(size.width()) / FloatType(size.height()) : FloatType(1);
    if(projectionParams.isPerspective) {
        rt_camera_projection(scene, RT_PROJECTION_PERSPECTIVE);
        const FloatType top = std::tan(projectionParams.fieldOfView * FloatType(0.5));
        const FloatType right = top * widthOverHeight;
        rt_camera_frustum(scene, -right, right, -top, top);
    }
    else {
        rt_camera_projection(scene, RT_PROJECTION_ORTHOGRAPHIC);
        const FloatType height = std::max(projectionParams.fieldOfView, FloatType(1e-6));
        const FloatType width = height * widthOverHeight;
        rt_camera_frustum(scene, -width / FloatType(2), width / FloatType(2), -height / FloatType(2), height / FloatType(2));
    }
}

void configureLighting(SceneHandle scene, const TachyonRenderer& renderer, const ViewProjectionParameters& projectionParams)
{
    if(renderer.shadows() && renderer.ambientOcclusion()) {
        const FloatType aoBrightness = std::max(FloatType(0), renderer.ambientOcclusionBrightness());
        rt_ambient_occlusion(scene, std::max(0, renderer.ambientOcclusionSamples()), rt_color(aoBrightness, aoBrightness, aoBrightness));
    }
    else
        rt_ambient_occlusion(scene, 0, rt_color(1, 1, 1));

    if(!renderer.directLight())
        return;

    const AffineTransformation& inv = projectionParams.inverseViewMatrix;
    Vector3 viewDir = inv.linear() * Vector3(0, 0, -1);
    Vector3 upDir = inv.linear() * Vector3(0, 1, 0);
    Vector3 rightDir = inv.linear() * Vector3(1, 0, 0);
    viewDir.normalizeSafely();
    upDir.normalizeSafely();
    rightDir.normalizeSafely();
    Vector3 lightDir = (viewDir - rightDir * FloatType(0.35) + upDir * FloatType(0.55));
    if(!lightDir.normalizeSafely())
        lightDir = Vector3(0, 0, -1);

    apitexture lightTex = {};
    lightTex.texturefunc = RT_TEXTURE_CONSTANT;
    lightTex.col = rt_color(1, 1, 1);
    lightTex.ambient = 1;
    lightTex.diffuse = 0;
    lightTex.specular = 0;
    lightTex.opacity = 1;
    lightTex.scale = rt_vector(1, 1, 1);
    rt_directional_light(scene, rt_texture(scene, &lightTex), toApiVector(lightDir));
    rt_rescale_lights(scene, std::max(FloatType(0), renderer.directLightBrightness()));
}

} // End anonymous namespace

IMPLEMENT_ABSTRACT_OVITO_CLASS(TachyonRenderingJob);

/******************************************************************************
* Constructor.
******************************************************************************/
void TachyonRenderingJob::initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, OORef<const TachyonRenderer> sceneRenderer)
{
    RenderingJob::initializeObject(flags);
    _visCache = std::move(visCache);
    _sceneRenderer = std::move(sceneRenderer);
}

/******************************************************************************
* Creates a new abstract target frame buffer for rendering into.
******************************************************************************/
OORef<RenderBuffer> TachyonRenderingJob::createOffscreenRenderBuffer(const QSize& deviceIndependentSize)
{
    return OORef<TachyonRenderBuffer>::create(deviceIndependentSize);
}

/******************************************************************************
* Renders an image of the given frame graph.
******************************************************************************/
SCFuture<void> TachyonRenderingJob::renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<RenderBuffer> renderBuffer, std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress)
{
    if(!frameBuffer)
        return SCFuture<void>::createImmediateEmpty();
    if(!_sceneRenderer)
        throw RendererException(tr("Cannot render scene: Tachyon renderer settings object is missing."));

    ensureTachyonInitialized();

    const QSize size = renderBuffer->size();
    if(size.width() <= 0 || size.height() <= 0)
        return SCFuture<void>::createImmediateEmpty();

    progress.setText(tr("Ray-tracing frame with Tachyon"));

    std::vector<unsigned char> rawImage(static_cast<size_t>(size.width()) * static_cast<size_t>(size.height()) * 3);

    std::lock_guard<std::mutex> guard(tachyonRenderMutex);
    SceneHandle scene = rt_newscene();
    try {
        rt_resolution(scene, size.width(), size.height());
        rt_rawimage_rgb24(scene, rawImage.data());
        rt_verbose(scene, 0);
        rt_image_clamp(scene);
        rt_background(scene, toApiColor(frameGraph->clearColor()));
        rt_background_mode(scene, RT_BACKGROUND_TEXTURE_SOLID);
        rt_boundmode(scene, RT_BOUNDING_ENABLED);
        rt_boundthresh(scene, 16);
        rt_aa_maxsamples(scene, _sceneRenderer->enableAntialiasing() ? std::max(0, _sceneRenderer->antialiasingSamples()) : 0);
        rt_camera_raydepth(scene, std::max(1, _sceneRenderer->maxRayRecursion()));
        rt_trans_max_surfaces(scene, std::max(1, _sceneRenderer->maxTransparentSurfaces()));
        rt_shadow_filtering(scene, _sceneRenderer->shadows() ? 1 : 0);
        // OVITO transparencies are alpha-style opacities. Tachyon's original
        // mode adds the full surface color even for semi-transparent objects,
        // which makes translucent meshes and particles look washed out.
        rt_trans_mode(scene, RT_TRANS_VMD);
        rt_normal_fixup_mode(scene, RT_NORMAL_FIXUP_GUESS);
        if(_sceneRenderer->maxThreads() > 0)
            rt_set_numthreads(scene, _sceneRenderer->maxThreads());

        configureCamera(scene, frameGraph->projectionParams(), size);
        if(_sceneRenderer->depthOfField() && frameGraph->projectionParams().isPerspective)
            rt_camera_dof(scene, std::max(FloatType(0), _sceneRenderer->focalLength()), std::max(FloatType(0), _sceneRenderer->aperture()));
        rt_phong_shader(scene, RT_SHADER_BLINN_FAST);
        configureLighting(scene, *_sceneRenderer, frameGraph->projectionParams());

        TachyonSceneBuilder builder(scene, *_sceneRenderer, *renderBuffer);
        for(const FrameGraph::RenderingCommandGroup& group : frameGraph->commandGroups()) {
            if(group.layerType() == FrameGraph::UnderLayer || group.layerType() == FrameGraph::OverLayer)
                continue;
            for(const FrameGraph::RenderingCommand& command : group.commands()) {
                this_task::throwIfCanceled();
                builder.renderPrimitive(command);
            }
        }

        rt_renderscene(scene);

        QImage renderedImage(size, QImage::Format_ARGB32_Premultiplied);
        for(int y = 0; y < size.height(); ++y) {
            QRgb* dst = reinterpret_cast<QRgb*>(renderedImage.scanLine(y));
            const unsigned char* src = rawImage.data() + static_cast<size_t>(size.height() - 1 - y) * static_cast<size_t>(size.width()) * 3;
            for(int x = 0; x < size.width(); ++x, src += 3) {
                dst[x] = qRgb(src[0], src[1], src[2]);
            }
        }

        if(!frameBuffer->image().isNull()) {
            QPainter painter(&frameBuffer->image());
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.drawImage(frameBuffer->viewportRect(), renderedImage);
        }
        else {
            frameBuffer->image() = std::move(renderedImage);
        }

        frameBuffer->renderPrimitives(FrameGraph::OverLayer, *frameGraph);
        frameBuffer->update(frameBuffer->viewportRect());
        frameBuffer->commitChanges();

        rt_deletescene(scene);
    }
    catch(...) {
        rt_deletescene(scene);
        throw;
    }

    return SCFuture<void>::createImmediateEmpty();
}

}   // End of namespace
