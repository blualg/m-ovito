////////////////////////////////////////////////////////////////////////////////////////
//
//  OSPRay renderer plugin for OVITO.
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
#include "OSPRayRenderBuffer.h"
#include "OSPRayRenderingJob.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <set>

#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QLibrary>
#include <QPainter>
#include <QRect>
#include <QThread>

namespace Ovito {
namespace {

std::mutex osprayRenderMutex;
constexpr int RayTraceTileSize = 2048;

using OSPObject = void*;
using OSPDevice = void*;
using OSPData = void*;
using OSPCamera = void*;
using OSPFrameBuffer = void*;
using OSPFuture = void*;
using OSPGeometricModel = void*;
using OSPGeometry = void*;
using OSPGroup = void*;
using OSPInstance = void*;
using OSPLight = void*;
using OSPMaterial = void*;
using OSPRenderer = void*;
using OSPWorld = void*;
using OSPDeleterCallback = void (*)(const void*, const void*);

enum OSPError : uint32_t
{
    OSP_NO_ERROR = 0,
    OSP_UNKNOWN_ERROR = 1,
    OSP_INVALID_ARGUMENT = 2,
    OSP_INVALID_OPERATION = 3,
    OSP_OUT_OF_MEMORY = 4,
    OSP_UNSUPPORTED_CPU = 5,
    OSP_VERSION_MISMATCH = 6
};

enum OSPDataType : uint32_t
{
    OSP_BOOL = 250,
    OSP_STRING = 1500,
    OSP_DATA = 0x8000000 + 100,
    OSP_CAMERA,
    OSP_FRAMEBUFFER,
    OSP_FUTURE,
    OSP_GEOMETRIC_MODEL,
    OSP_GEOMETRY,
    OSP_GROUP,
    OSP_IMAGE_OPERATION,
    OSP_INSTANCE,
    OSP_LIGHT,
    OSP_MATERIAL,
    OSP_RENDERER,
    OSP_TEXTURE,
    OSP_TRANSFER_FUNCTION,
    OSP_VOLUME,
    OSP_VOLUMETRIC_MODEL,
    OSP_WORLD,
    OSP_INT = 4000,
    OSP_UINT = 4500,
    OSP_FLOAT = 6000,
    OSP_VEC2F,
    OSP_VEC3F,
    OSP_VEC4F
};

enum OSPFrameBufferFormat : uint32_t
{
    OSP_FB_NONE = 0,
    OSP_FB_RGBA8 = 1,
    OSP_FB_SRGBA = 2,
    OSP_FB_RGBA32F = 3
};

enum OSPFrameBufferChannel : uint32_t
{
    OSP_FB_COLOR = (1 << 0),
    OSP_FB_ACCUM = (1 << 2)
};

enum OSPSyncEvent : uint32_t
{
    OSP_TASK_FINISHED = 100000
};

enum OSPCurveType : uint32_t
{
    OSP_DISJOINT = 3
};

enum OSPCurveBasis : uint32_t
{
    OSP_LINEAR = 0
};

struct Vec2f
{
    float x;
    float y;
};

struct Vec3f
{
    float x;
    float y;
    float z;
};

struct Vec4f
{
    float x;
    float y;
    float z;
    float w;
};

std::vector<QRect> makeRenderTiles(const QSize& size)
{
    std::vector<QRect> tiles;
    const int tileSize = std::max(1, RayTraceTileSize);
    for(int y = 0; y < size.height(); y += tileSize) {
        const int tileHeight = std::min(tileSize, size.height() - y);
        for(int x = 0; x < size.width(); x += tileSize) {
            const int tileWidth = std::min(tileSize, size.width() - x);
            tiles.emplace_back(x, y, tileWidth, tileHeight);
        }
    }
    return tiles;
}

Vec2f imageRegionStart(const QRect& tile, const QSize& fullSize)
{
    return Vec2f{
        static_cast<float>(tile.x()) / static_cast<float>(fullSize.width()),
        1.0f - static_cast<float>(tile.y() + tile.height()) / static_cast<float>(fullSize.height())
    };
}

Vec2f imageRegionEnd(const QRect& tile, const QSize& fullSize)
{
    return Vec2f{
        static_cast<float>(tile.x() + tile.width()) / static_cast<float>(fullSize.width()),
        1.0f - static_cast<float>(tile.y()) / static_cast<float>(fullSize.height())
    };
}

struct Vec3ui
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
};

FloatType clamp01(FloatType value)
{
    return std::clamp(value, FloatType(0), FloatType(1));
}

Vec3f toVec3f(const Point3& p)
{
    return Vec3f{ static_cast<float>(p.x()), static_cast<float>(p.y()), static_cast<float>(p.z()) };
}

Vec3f toVec3f(const Vector3& v)
{
    return Vec3f{ static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()) };
}

Vec4f toVec4f(const ColorA& c)
{
    return Vec4f{
        static_cast<float>(clamp01(c.r())),
        static_cast<float>(clamp01(c.g())),
        static_cast<float>(clamp01(c.b())),
        static_cast<float>(clamp01(c.a()))
    };
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

QString runtimeDllName(const QString& baseName)
{
#ifdef Q_OS_WIN
    return baseName + QStringLiteral(".dll");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("lib") + baseName + QStringLiteral(".dylib");
#else
    return QStringLiteral("lib") + baseName + QStringLiteral(".so");
#endif
}

void prependRuntimeDirectoryToPath(const QString& directory)
{
#ifdef Q_OS_WIN
    if(directory.trimmed().isEmpty())
        return;

    const QByteArray nativeDirectory = QDir::toNativeSeparators(directory).toLocal8Bit();
    QByteArray path = qgetenv("PATH");
    const QList<QByteArray> pathEntries = path.split(';');
    if(!pathEntries.contains(nativeDirectory)) {
        if(!path.isEmpty())
            path.prepend(';');
        path.prepend(nativeDirectory);
        qputenv("PATH", path);
    }
#else
    Q_UNUSED(directory);
#endif
}

struct RuntimeLibraryCandidate
{
    QString fileName;
    QString dependencyDirectory;
};

void addRuntimeCandidate(QVector<RuntimeLibraryCandidate>& candidates, const QString& fileName, const QString& dependencyDirectory = QString())
{
    if(fileName.trimmed().isEmpty())
        return;

    const QString normalizedFileName = QDir::toNativeSeparators(fileName);
    const QString normalizedDependencyDirectory = dependencyDirectory.isEmpty() ? QString() : QDir::toNativeSeparators(dependencyDirectory);
    for(const RuntimeLibraryCandidate& candidate : candidates) {
        if(QDir::toNativeSeparators(candidate.fileName) == normalizedFileName)
            return;
    }

    candidates.push_back({ fileName, normalizedDependencyDirectory });
}

void addRuntimeDirectoryCandidate(QVector<RuntimeLibraryCandidate>& candidates, const QString& directory, const QString& baseName)
{
    if(directory.trimmed().isEmpty())
        return;

    QDir dir(directory);
    if(!dir.exists())
        return;

    addRuntimeCandidate(candidates, dir.filePath(runtimeDllName(baseName)), dir.absolutePath());
    addRuntimeCandidate(candidates, dir.filePath(baseName), dir.absolutePath());
}

QVector<RuntimeLibraryCandidate> buildOSPRayRuntimeCandidates(const QString& libraryPath)
{
    QVector<RuntimeLibraryCandidate> candidates;
    const QString trimmedPath = libraryPath.trimmed();

    if(!trimmedPath.isEmpty()) {
        QFileInfo info(trimmedPath);
        if(info.isDir())
            addRuntimeDirectoryCandidate(candidates, info.absoluteFilePath(), QStringLiteral("ospray"));
        else
            addRuntimeCandidate(candidates, trimmedPath, info.absolutePath());
        return candidates;
    }

    addRuntimeCandidate(candidates, QStringLiteral("ospray"));
    addRuntimeDirectoryCandidate(candidates, QCoreApplication::applicationDirPath(), QStringLiteral("ospray"));
    addRuntimeDirectoryCandidate(candidates, qEnvironmentVariable("OSPRAY_ROOT"), QStringLiteral("ospray"));
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("OSPRAY_ROOT")).filePath(QStringLiteral("bin")), QStringLiteral("ospray"));
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("ONEAPI_ROOT")).filePath(QStringLiteral("ospray/latest/bin")), QStringLiteral("ospray"));
#ifdef Q_OS_WIN
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("ProgramFiles")).filePath(QStringLiteral("OVITO Basic")), QStringLiteral("ospray"));
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("ProgramFiles(x86)")).filePath(QStringLiteral("OVITO Basic")), QStringLiteral("ospray"));
#endif

    return candidates;
}

class OSPRayApi
{
public:

    explicit OSPRayApi(const QString& libraryPath)
    {
        const QVector<RuntimeLibraryCandidate> candidates = buildOSPRayRuntimeCandidates(libraryPath);
        QStringList loadErrors;
        for(const RuntimeLibraryCandidate& candidate : candidates) {
            if(!candidate.dependencyDirectory.isEmpty())
                prependRuntimeDirectoryToPath(candidate.dependencyDirectory);

            _library.setFileName(candidate.fileName);
            _library.setLoadHints(QLibrary::ResolveAllSymbolsHint);
            if(_library.load())
                break;

            loadErrors.push_back(QObject::tr("%1: %2").arg(candidate.fileName, _library.errorString()));
        }

        if(!_library.isLoaded()) {
            throw RendererException(QObject::tr(
                "Failed to load the OSPRay runtime library. Install Intel OSPRay/oneAPI Rendering Toolkit, copy ospray.dll and its dependent DLLs next to ovito.exe, or set the OSPRay library path to the folder containing ospray.dll.\n\nTried:\n%1")
                .arg(loadErrors.join(QLatin1Char('\n'))));
        }

        resolveAll();
    }

    OSPError (*ospInit)(int*, const char**) = nullptr;
    void (*ospShutdown)() = nullptr;
    OSPData (*ospNewSharedData)(const void*, OSPDataType, uint64_t, int64_t, uint64_t, int64_t, uint64_t, int64_t, OSPDeleterCallback, const void*) = nullptr;
    OSPLight (*ospNewLight)(const char*) = nullptr;
    OSPCamera (*ospNewCamera)(const char*) = nullptr;
    OSPGeometry (*ospNewGeometry)(const char*) = nullptr;
    OSPGeometricModel (*ospNewGeometricModel)(OSPGeometry) = nullptr;
    OSPMaterial (*ospNewMaterial)(const char*) = nullptr;
    OSPGroup (*ospNewGroup)() = nullptr;
    OSPInstance (*ospNewInstance)(OSPGroup) = nullptr;
    OSPWorld (*ospNewWorld)() = nullptr;
    void (*ospSetParam)(OSPObject, const char*, OSPDataType, const void*) = nullptr;
    void (*ospCommit)(OSPObject) = nullptr;
    void (*ospRelease)(OSPObject) = nullptr;
    OSPFrameBuffer (*ospNewFrameBuffer)(int, int, OSPFrameBufferFormat, uint32_t) = nullptr;
    const void* (*ospMapFrameBuffer)(OSPFrameBuffer, OSPFrameBufferChannel) = nullptr;
    void (*ospUnmapFrameBuffer)(const void*, OSPFrameBuffer) = nullptr;
    void (*ospResetAccumulation)(OSPFrameBuffer) = nullptr;
    OSPRenderer (*ospNewRenderer)(const char*) = nullptr;
    OSPFuture (*ospRenderFrame)(OSPFrameBuffer, OSPRenderer, OSPCamera, OSPWorld) = nullptr;
    int (*ospIsReady)(OSPFuture, OSPSyncEvent) = nullptr;
    void (*ospWait)(OSPFuture, OSPSyncEvent) = nullptr;
    void (*ospCancel)(OSPFuture) = nullptr;
    float (*ospGetProgress)(OSPFuture) = nullptr;

private:

    void resolveAll()
    {
        resolve(ospInit, "ospInit");
        resolve(ospShutdown, "ospShutdown");
        resolve(ospNewSharedData, "ospNewSharedData");
        resolve(ospNewLight, "ospNewLight");
        resolve(ospNewCamera, "ospNewCamera");
        resolve(ospNewGeometry, "ospNewGeometry");
        resolve(ospNewGeometricModel, "ospNewGeometricModel");
        resolve(ospNewMaterial, "ospNewMaterial");
        resolve(ospNewGroup, "ospNewGroup");
        resolve(ospNewInstance, "ospNewInstance");
        resolve(ospNewWorld, "ospNewWorld");
        resolve(ospSetParam, "ospSetParam");
        resolve(ospCommit, "ospCommit");
        resolve(ospRelease, "ospRelease");
        resolve(ospNewFrameBuffer, "ospNewFrameBuffer");
        resolve(ospMapFrameBuffer, "ospMapFrameBuffer");
        resolve(ospUnmapFrameBuffer, "ospUnmapFrameBuffer");
        resolve(ospResetAccumulation, "ospResetAccumulation");
        resolve(ospNewRenderer, "ospNewRenderer");
        resolve(ospRenderFrame, "ospRenderFrame");
        resolve(ospIsReady, "ospIsReady");
        resolve(ospWait, "ospWait");
        resolve(ospCancel, "ospCancel");
        resolve(ospGetProgress, "ospGetProgress");
    }

    template<typename FunctionPointer>
    void resolve(FunctionPointer& fn, const char* symbolName)
    {
        fn = reinterpret_cast<FunctionPointer>(_library.resolve(symbolName));
        if(!fn) {
            throw RendererException(QObject::tr("The loaded OSPRay runtime is missing required symbol '%1'.").arg(QString::fromLatin1(symbolName)));
        }
    }

    QLibrary _library;
};

class OSPRaySession
{
public:

    explicit OSPRaySession(OSPRayApi& api) : _api(api)
    {
        int argc = 1;
        const char* argv[] = { "ovito-ospray", nullptr };
        const OSPError err = _api.ospInit(&argc, argv);
        if(err != OSP_NO_ERROR)
            throw RendererException(QObject::tr("Failed to initialize OSPRay. Error code: %1").arg(static_cast<uint32_t>(err)));
        _initialized = true;
    }

    ~OSPRaySession()
    {
        if(_initialized)
            _api.ospShutdown();
    }

private:

    OSPRayApi& _api;
    bool _initialized = false;
};

class OSPRayOwnedObjects
{
public:

    explicit OSPRayOwnedObjects(OSPRayApi& api) : _api(api) {}

    template<typename T>
    T keep(T handle)
    {
        if(handle)
            _handles.push_back(static_cast<OSPObject>(handle));
        return handle;
    }

    ~OSPRayOwnedObjects()
    {
        releaseAll();
    }

    void releaseAll()
    {
        for(auto it = _handles.rbegin(); it != _handles.rend(); ++it) {
            if(*it)
                _api.ospRelease(*it);
        }
        _handles.clear();
    }

private:

    OSPRayApi& _api;
    std::vector<OSPObject> _handles;
};

OSPData makeSharedData(OSPRayApi& api, OSPRayOwnedObjects& owned, const void* data, OSPDataType type, size_t count)
{
    OSPData handle = owned.keep(api.ospNewSharedData(data, type, static_cast<uint64_t>(count), 0, 1, 0, 1, 0, nullptr, nullptr));
    if(!handle)
        throw RendererException(QObject::tr("OSPRay failed to create shared scene data."));
    api.ospCommit(handle);
    return handle;
}

template<typename T>
void setParam(OSPRayApi& api, OSPObject object, const char* name, OSPDataType type, const T& value)
{
    api.ospSetParam(object, name, type, &value);
}

OSPMaterial createDefaultMaterial(OSPRayApi& api, OSPRayOwnedObjects& owned)
{
    OSPMaterial material = owned.keep(api.ospNewMaterial("obj"));
    if(!material)
        throw RendererException(QObject::tr("OSPRay failed to create the default material."));
    Vec3f kd{ 0.8f, 0.8f, 0.8f };
    Vec3f ks{ 0.04f, 0.04f, 0.04f };
    float ns = 16.0f;
    setParam(api, material, "kd", OSP_VEC3F, kd);
    setParam(api, material, "ks", OSP_VEC3F, ks);
    setParam(api, material, "ns", OSP_FLOAT, ns);
    api.ospCommit(material);
    return material;
}

class OSPRaySceneBuilder
{
public:

    OSPRaySceneBuilder(OSPRayApi& api, OSPRayOwnedObjects& owned, OSPMaterial material, const OSPRayRenderer& renderer, RenderBuffer& renderBuffer) :
        _api(api), _owned(owned), _material(material), _renderer(renderer), _renderBuffer(renderBuffer) {}

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
            reportOnce("OSPRay renderer does not currently convert OVITO volume primitives.");
    }

    const std::vector<OSPGeometricModel>& models() const { return _models; }

private:

    void renderParticles(const ParticlePrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.positions() || primitive.positions()->size() == 0)
            return;

        if(primitive.particleShape() != ParticlePrimitive::SphericalShape)
            reportOnce("OSPRay renderer currently renders non-spherical particle glyphs as spheres.");

        std::vector<Vec3f> positions;
        std::vector<float> radii;
        std::vector<Vec4f> colors;

        auto gatherPositions = [&](auto _) {
            using T = decltype(_);
            BufferReadAccess<Point_3<T>> sourcePositions(primitive.positions());

            auto gatherParticle = [&](size_t sourceIndex) {
                const Point3 pos = tm * sourcePositions[sourceIndex].template toDataType<FloatType>();
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

                positions.push_back(toVec3f(pos));
                radii.push_back(static_cast<float>(radius));
                colors.push_back(toVec4f(color));
            };

            if(primitive.indices()) {
                const BufferReadAccess<int32_t> indices(primitive.indices());
                for(int32_t sourceIndex : indices)
                    gatherParticle(static_cast<size_t>(sourceIndex));
            }
            else {
                for(size_t i = 0; i < sourcePositions.size(); ++i)
                    gatherParticle(i);
            }
        };

        primitive.positions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(gatherPositions);
        if(positions.empty())
            return;

        OSPGeometry geometry = _owned.keep(_api.ospNewGeometry("sphere"));
        if(!geometry) {
            reportOnce("OSPRay failed to create sphere geometry.");
            return;
        }
        OSPData positionData = makeSharedData(_api, _owned, positions.data(), OSP_VEC3F, positions.size());
        OSPData radiusData = makeSharedData(_api, _owned, radii.data(), OSP_FLOAT, radii.size());
        OSPData colorData = makeSharedData(_api, _owned, colors.data(), OSP_VEC4F, colors.size());
        setParam(_api, geometry, "sphere.position", OSP_DATA, positionData);
        setParam(_api, geometry, "sphere.radius", OSP_DATA, radiusData);
        _api.ospCommit(geometry);

        OSPGeometricModel model = _owned.keep(_api.ospNewGeometricModel(geometry));
        if(!model) {
            reportOnce("OSPRay failed to create a sphere geometric model.");
            return;
        }
        setParam(_api, model, "material", OSP_MATERIAL, _material);
        setParam(_api, model, "color", OSP_DATA, colorData);
        _api.ospCommit(model);
        _models.push_back(model);

        _ownedVectors.emplace_back(std::move(positions), std::move(radii), std::move(colors));
    }

    void renderCylinders(const CylinderPrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.basePositions() || !primitive.headPositions() || primitive.basePositions()->size() == 0)
            return;
        if(primitive.colors() && primitive.colors()->componentCount() == 1)
            reportOnce("OSPRay renderer currently ignores one-component pseudo-colors on cylinders.");

        std::vector<Vec4f> vertices;
        std::vector<Vec4f> colors;
        std::vector<uint32_t> indices;

        auto gatherPositions = [&](auto _) {
            using T = decltype(_);
            BufferReadAccess<Point_3<T>> bases(primitive.basePositions());
            BufferReadAccess<Point_3<T>> heads(primitive.headPositions());

            for(size_t i = 0; i < bases.size(); ++i) {
                Point3 base = tm * bases[i].template toDataType<FloatType>();
                Point3 head = tm * heads[i].template toDataType<FloatType>();
                Vector3 axis = head - base;
                if(axis.length() <= 0)
                    continue;

                ColorA color = colorWithOpacity(primitive.uniformColor(), 1);
                color = readRgbColor(primitive.colors(), i, color);
                if(primitive.selection() && readSelection(primitive.selection(), i))
                    color = colorWithOpacity(primitive.selectionColor(), color.a());
                if(primitive.transparencies())
                    color.a() *= clamp01(FloatType(1) - readScalar(primitive.transparencies(), i, 0));
                if(color.a() <= 0)
                    continue;

                const float radius = static_cast<float>(std::max(FloatType(0), readScalar(primitive.widths(), i, primitive.uniformWidth()) / FloatType(2)));
                if(radius <= 0)
                    continue;

                const uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
                vertices.push_back(Vec4f{ static_cast<float>(base.x()), static_cast<float>(base.y()), static_cast<float>(base.z()), radius });
                vertices.push_back(Vec4f{ static_cast<float>(head.x()), static_cast<float>(head.y()), static_cast<float>(head.z()), radius });
                colors.push_back(toVec4f(color));
                colors.push_back(toVec4f(color));
                indices.push_back(firstVertex);
            }
        };

        primitive.basePositions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(gatherPositions);
        addCurveGeometry(std::move(vertices), std::move(colors), std::move(indices));
    }

    void renderMesh(const MeshPrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.mesh() || primitive.faceCount() == 0)
            return;
        if(primitive.useInstancedRendering())
            reportOnce("OSPRay renderer currently renders only the base mesh for instanced mesh primitives.");
        if(primitive.mesh()->hasVertexPseudoColors() || primitive.mesh()->hasFacePseudoColors())
            reportOnce("OSPRay renderer currently ignores mesh pseudo-color mappings.");

        std::vector<MeshPrimitive::RenderVertex> renderVertices(static_cast<size_t>(primitive.faceCount()) * 3);
        primitive.generateRenderableVertices(renderVertices, false, false);

        std::vector<Vec3f> positions;
        std::vector<Vec3f> normals;
        std::vector<Vec4f> colors;
        std::vector<Vec3ui> indices;
        positions.reserve(renderVertices.size());
        normals.reserve(renderVertices.size());
        colors.reserve(renderVertices.size());
        indices.reserve(renderVertices.size() / 3);

        for(size_t i = 0; i + 2 < renderVertices.size(); i += 3) {
            const Point3 p0 = tm * renderVertices[i].position.template toDataType<FloatType>();
            const Point3 p1 = tm * renderVertices[i + 1].position.template toDataType<FloatType>();
            const Point3 p2 = tm * renderVertices[i + 2].position.template toDataType<FloatType>();

            Vector3 n0 = tm.linear() * renderVertices[i].normal.template toDataType<FloatType>();
            Vector3 n1 = tm.linear() * renderVertices[i + 1].normal.template toDataType<FloatType>();
            Vector3 n2 = tm.linear() * renderVertices[i + 2].normal.template toDataType<FloatType>();
            if(!n0.normalizeSafely() || !n1.normalizeSafely() || !n2.normalizeSafely()) {
                Vector3 faceNormal = (p1 - p0).cross(p2 - p0);
                if(!faceNormal.normalizeSafely())
                    continue;
                n0 = n1 = n2 = faceNormal;
            }

            ColorA c0 = renderVertices[i].color.template toDataType<FloatType>();
            ColorA c1 = renderVertices[i + 1].color.template toDataType<FloatType>();
            ColorA c2 = renderVertices[i + 2].color.template toDataType<FloatType>();
            if(c0.a() <= 0 && c1.a() <= 0 && c2.a() <= 0)
                continue;

            const uint32_t baseIndex = static_cast<uint32_t>(positions.size());
            positions.push_back(toVec3f(p0));
            positions.push_back(toVec3f(p1));
            positions.push_back(toVec3f(p2));
            normals.push_back(toVec3f(n0));
            normals.push_back(toVec3f(n1));
            normals.push_back(toVec3f(n2));
            colors.push_back(toVec4f(c0));
            colors.push_back(toVec4f(c1));
            colors.push_back(toVec4f(c2));
            indices.push_back(Vec3ui{ baseIndex, baseIndex + 1, baseIndex + 2 });
        }

        if(positions.empty())
            return;

        OSPGeometry geometry = _owned.keep(_api.ospNewGeometry("mesh"));
        if(!geometry) {
            reportOnce("OSPRay failed to create mesh geometry.");
            return;
        }
        OSPData positionData = makeSharedData(_api, _owned, positions.data(), OSP_VEC3F, positions.size());
        OSPData normalData = makeSharedData(_api, _owned, normals.data(), OSP_VEC3F, normals.size());
        OSPData colorData = makeSharedData(_api, _owned, colors.data(), OSP_VEC4F, colors.size());
        OSPData indexData = makeSharedData(_api, _owned, indices.data(), static_cast<OSPDataType>(OSP_UINT + 2), indices.size());
        setParam(_api, geometry, "vertex.position", OSP_DATA, positionData);
        setParam(_api, geometry, "vertex.normal", OSP_DATA, normalData);
        setParam(_api, geometry, "vertex.color", OSP_DATA, colorData);
        setParam(_api, geometry, "index", OSP_DATA, indexData);
        _api.ospCommit(geometry);

        OSPGeometricModel model = _owned.keep(_api.ospNewGeometricModel(geometry));
        if(!model) {
            reportOnce("OSPRay failed to create a mesh geometric model.");
            return;
        }
        setParam(_api, model, "material", OSP_MATERIAL, _material);
        _api.ospCommit(model);
        _models.push_back(model);

        _ownedVectors.emplace_back(std::move(positions), std::move(normals), std::move(colors), std::move(indices));

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

        std::vector<Vec4f> vertices;
        std::vector<Vec4f> colors;
        std::vector<uint32_t> indices;
        const FloatType fallbackRadius = std::max(FloatType(0.01), primitive.lineWidth() * FloatType(0.25));

        auto gatherPositions = [&](auto _) {
            using T = decltype(_);
            BufferReadAccess<Point_3<T>> points(primitive.positions());

            for(size_t i = 0; i + 1 < points.size(); i += 2) {
                Point3 base = tm * points[i].template toDataType<FloatType>();
                Point3 head = tm * points[i + 1].template toDataType<FloatType>();
                if((head - base).length() <= 0)
                    continue;

                ColorA color = primitive.uniformColor();
                color = readRgbaColor(primitive.colors(), i, color);
                if(color.a() <= 0)
                    continue;

                const uint32_t firstVertex = static_cast<uint32_t>(vertices.size());
                const float radius = static_cast<float>(fallbackRadius);
                vertices.push_back(Vec4f{ static_cast<float>(base.x()), static_cast<float>(base.y()), static_cast<float>(base.z()), radius });
                vertices.push_back(Vec4f{ static_cast<float>(head.x()), static_cast<float>(head.y()), static_cast<float>(head.z()), radius });
                colors.push_back(toVec4f(color));
                colors.push_back(toVec4f(color));
                indices.push_back(firstVertex);
            }
        };

        primitive.positions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(gatherPositions);
        addCurveGeometry(std::move(vertices), std::move(colors), std::move(indices));
    }

    void addCurveGeometry(std::vector<Vec4f> vertices, std::vector<Vec4f> colors, std::vector<uint32_t> indices)
    {
        if(vertices.empty() || indices.empty())
            return;

        OSPGeometry geometry = _owned.keep(_api.ospNewGeometry("curve"));
        if(!geometry) {
            reportOnce("OSPRay failed to create curve geometry.");
            return;
        }
        OSPData vertexData = makeSharedData(_api, _owned, vertices.data(), OSP_VEC4F, vertices.size());
        OSPData colorData = makeSharedData(_api, _owned, colors.data(), OSP_VEC4F, colors.size());
        OSPData indexData = makeSharedData(_api, _owned, indices.data(), OSP_UINT, indices.size());
        setParam(_api, geometry, "vertex.position_radius", OSP_DATA, vertexData);
        setParam(_api, geometry, "vertex.color", OSP_DATA, colorData);
        setParam(_api, geometry, "index", OSP_DATA, indexData);
        OSPCurveType curveType = OSP_DISJOINT;
        OSPCurveBasis curveBasis = OSP_LINEAR;
        setParam(_api, geometry, "type", OSP_UINT, curveType);
        setParam(_api, geometry, "basis", OSP_UINT, curveBasis);
        _api.ospCommit(geometry);

        OSPGeometricModel model = _owned.keep(_api.ospNewGeometricModel(geometry));
        if(!model) {
            reportOnce("OSPRay failed to create a curve geometric model.");
            return;
        }
        setParam(_api, model, "material", OSP_MATERIAL, _material);
        _api.ospCommit(model);
        _models.push_back(model);

        _ownedVectors.emplace_back(std::move(vertices), std::move(colors), std::move(indices));
    }

    void reportOnce(const char* message)
    {
        if(_reportedIssues.insert(message).second)
            _renderBuffer.reportIssue(QString::fromUtf8(message));
    }

    struct OwnedVectorStorage
    {
        std::vector<Vec3f> vec3a;
        std::vector<Vec3f> vec3b;
        std::vector<Vec4f> vec4a;
        std::vector<Vec4f> vec4b;
        std::vector<float> floats;
        std::vector<uint32_t> uints;
        std::vector<Vec3ui> vec3uis;

        OwnedVectorStorage(std::vector<Vec3f>&& positions, std::vector<float>&& radii, std::vector<Vec4f>&& colors) :
            vec3a(std::move(positions)), vec4a(std::move(colors)), floats(std::move(radii)) {}

        OwnedVectorStorage(std::vector<Vec3f>&& positions, std::vector<Vec3f>&& normals, std::vector<Vec4f>&& colors, std::vector<Vec3ui>&& indices) :
            vec3a(std::move(positions)), vec3b(std::move(normals)), vec4a(std::move(colors)), vec3uis(std::move(indices)) {}

        OwnedVectorStorage(std::vector<Vec4f>&& vertices, std::vector<Vec4f>&& colors, std::vector<uint32_t>&& indices) :
            vec4a(std::move(vertices)), vec4b(std::move(colors)), uints(std::move(indices)) {}
    };

private:

    OSPRayApi& _api;
    OSPRayOwnedObjects& _owned;
    OSPMaterial _material;
    const OSPRayRenderer& _renderer;
    RenderBuffer& _renderBuffer;
    std::vector<OSPGeometricModel> _models;
    std::vector<OwnedVectorStorage> _ownedVectors;
    std::set<QByteArray> _reportedIssues;
};

OSPCamera createCamera(OSPRayApi& api, OSPRayOwnedObjects& owned, const ViewProjectionParameters& projectionParams, const QSize& tileSize, const QRect& tile, const QSize& fullSize)
{
    const bool perspective = projectionParams.isPerspective;
    OSPCamera camera = owned.keep(api.ospNewCamera(perspective ? "perspective" : "orthographic"));
    if(!camera)
        throw RendererException(QObject::tr("OSPRay failed to create the camera."));

    const AffineTransformation& inv = projectionParams.inverseViewMatrix;
    Point3 cameraPosition = Point3::Origin() + inv.translation();
    Vector3 viewDir = inv.linear() * Vector3(0, 0, -1);
    Vector3 upDir = inv.linear() * Vector3(0, 1, 0);
    if(!viewDir.normalizeSafely())
        viewDir = Vector3(0, 0, -1);
    if(!upDir.normalizeSafely())
        upDir = Vector3(0, 1, 0);

    Vec3f position = toVec3f(cameraPosition);
    Vec3f direction = toVec3f(viewDir);
    Vec3f up = toVec3f(upDir);
    const float aspect = tileSize.height() > 0 ? static_cast<float>(tileSize.width()) / static_cast<float>(tileSize.height()) : 1.0f;
    setParam(api, camera, "position", OSP_VEC3F, position);
    setParam(api, camera, "direction", OSP_VEC3F, direction);
    setParam(api, camera, "up", OSP_VEC3F, up);
    setParam(api, camera, "aspect", OSP_FLOAT, aspect);
    if(tile.size() != fullSize) {
        const Vec2f imageStart = imageRegionStart(tile, fullSize);
        const Vec2f imageEnd = imageRegionEnd(tile, fullSize);
        setParam(api, camera, "imageStart", OSP_VEC2F, imageStart);
        setParam(api, camera, "imageEnd", OSP_VEC2F, imageEnd);
    }

    if(perspective) {
        const float fovy = static_cast<float>(projectionParams.fieldOfView * FloatType(180.0 / M_PI));
        setParam(api, camera, "fovy", OSP_FLOAT, fovy);
    }
    else {
        const float height = static_cast<float>(std::max(projectionParams.fieldOfView, FloatType(1e-6)));
        setParam(api, camera, "height", OSP_FLOAT, height);
    }

    api.ospCommit(camera);
    return camera;
}

std::vector<OSPLight> createLights(OSPRayApi& api, OSPRayOwnedObjects& owned, const OSPRayRenderer& renderer, const ViewProjectionParameters& projectionParams)
{
    std::vector<OSPLight> lights;

    if(renderer.ambientLight()) {
        OSPLight ambient = owned.keep(api.ospNewLight("ambient"));
        float intensity = static_cast<float>(std::max(FloatType(0), renderer.ambientLightIntensity()));
        setParam(api, ambient, "intensity", OSP_FLOAT, intensity);
        api.ospCommit(ambient);
        lights.push_back(ambient);
    }

    if(renderer.directLight()) {
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

        OSPLight distant = owned.keep(api.ospNewLight("distant"));
        Vec3f direction = toVec3f(lightDir);
        float intensity = static_cast<float>(std::max(FloatType(0), renderer.directLightIntensity()));
        setParam(api, distant, "direction", OSP_VEC3F, direction);
        setParam(api, distant, "intensity", OSP_FLOAT, intensity);
        api.ospCommit(distant);
        lights.push_back(distant);
    }

    return lights;
}

OSPRenderer createRenderer(OSPRayApi& api, OSPRayOwnedObjects& owned, const OSPRayRenderer& renderer, const ColorA& clearColor)
{
    QByteArray rendererType = renderer.rendererType().trimmed().toLatin1();
    if(rendererType.isEmpty())
        rendererType = "scivis";

    OSPRenderer ospRenderer = owned.keep(api.ospNewRenderer(rendererType.constData()));
    if(!ospRenderer)
        throw RendererException(QObject::tr("OSPRay failed to create renderer type '%1'.").arg(QString::fromLatin1(rendererType)));
    int pixelSamples = std::max(1, renderer.samplesPerPixel());
    int aoSamples = renderer.ambientOcclusion() ? std::max(0, renderer.ambientOcclusionSamples()) : 0;
    int shadows = renderer.shadows() ? 1 : 0;
    Vec4f background{
        static_cast<float>(clamp01(clearColor.r())),
        static_cast<float>(clamp01(clearColor.g())),
        static_cast<float>(clamp01(clearColor.b())),
        1.0f
    };
    setParam(api, ospRenderer, "pixelSamples", OSP_INT, pixelSamples);
    setParam(api, ospRenderer, "aoSamples", OSP_INT, aoSamples);
    setParam(api, ospRenderer, "shadows", OSP_BOOL, shadows);
    setParam(api, ospRenderer, "backgroundColor", OSP_VEC4F, background);
    api.ospCommit(ospRenderer);
    return ospRenderer;
}

OSPWorld createWorld(OSPRayApi& api, OSPRayOwnedObjects& owned, const std::vector<OSPGeometricModel>& models, std::vector<OSPLight>& lights, std::vector<OSPInstance>& instanceStorage)
{
    OSPGroup group = owned.keep(api.ospNewGroup());
    if(!group)
        throw RendererException(QObject::tr("OSPRay failed to create the geometry group."));
    if(!models.empty()) {
        OSPData geometryData = makeSharedData(api, owned, models.data(), OSP_GEOMETRIC_MODEL, models.size());
        setParam(api, group, "geometry", OSP_DATA, geometryData);
    }
    api.ospCommit(group);

    OSPInstance instance = owned.keep(api.ospNewInstance(group));
    if(!instance)
        throw RendererException(QObject::tr("OSPRay failed to create the geometry instance."));
    api.ospCommit(instance);

    OSPWorld world = owned.keep(api.ospNewWorld());
    if(!world)
        throw RendererException(QObject::tr("OSPRay failed to create the world."));
    instanceStorage.assign(1, instance);
    OSPData instanceData = makeSharedData(api, owned, instanceStorage.data(), OSP_INSTANCE, instanceStorage.size());
    setParam(api, world, "instance", OSP_DATA, instanceData);
    if(!lights.empty()) {
        OSPData lightData = makeSharedData(api, owned, lights.data(), OSP_LIGHT, lights.size());
        setParam(api, world, "light", OSP_DATA, lightData);
    }
    api.ospCommit(world);
    return world;
}

} // End anonymous namespace

IMPLEMENT_ABSTRACT_OVITO_CLASS(OSPRayRenderingJob);

/******************************************************************************
* Constructor.
******************************************************************************/
void OSPRayRenderingJob::initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, OORef<const OSPRayRenderer> sceneRenderer)
{
    RenderingJob::initializeObject(flags);
    _visCache = std::move(visCache);
    _sceneRenderer = std::move(sceneRenderer);
}

/******************************************************************************
* Creates a new abstract target frame buffer for rendering into.
******************************************************************************/
OORef<RenderBuffer> OSPRayRenderingJob::createOffscreenRenderBuffer(const QSize& deviceIndependentSize)
{
    return OORef<OSPRayRenderBuffer>::create(deviceIndependentSize);
}

/******************************************************************************
* Renders an image of the given frame graph.
******************************************************************************/
SCFuture<void> OSPRayRenderingJob::renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<RenderBuffer> renderBuffer, std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress)
{
    if(!frameBuffer)
        return SCFuture<void>::createImmediateEmpty();
    if(!_sceneRenderer)
        throw RendererException(tr("Cannot render scene: OSPRay renderer settings object is missing."));

    const QSize size = renderBuffer->size();
    if(size.width() <= 0 || size.height() <= 0)
        return SCFuture<void>::createImmediateEmpty();

    progress.setText(tr("Ray-tracing frame with OSPRay"));

    std::lock_guard<std::mutex> guard(osprayRenderMutex);
    OSPRayApi api(_sceneRenderer->libraryPath());
    OSPRaySession session(api);
    OSPRayOwnedObjects owned(api);

    OSPMaterial material = createDefaultMaterial(api, owned);
    OSPRaySceneBuilder builder(api, owned, material, *_sceneRenderer, *renderBuffer);
    for(const FrameGraph::RenderingCommandGroup& group : frameGraph->commandGroups()) {
        if(group.layerType() == FrameGraph::UnderLayer || group.layerType() == FrameGraph::OverLayer)
            continue;
        for(const FrameGraph::RenderingCommand& command : group.commands()) {
            this_task::throwIfCanceled();
            builder.renderPrimitive(command);
        }
    }

    OSPRenderer renderer = createRenderer(api, owned, *_sceneRenderer, frameGraph->clearColor());
    std::vector<OSPLight> lights = createLights(api, owned, *_sceneRenderer, frameGraph->projectionParams());
    std::vector<OSPInstance> instanceStorage;
    OSPWorld world = createWorld(api, owned, builder.models(), lights, instanceStorage);

    QImage renderedImage(size, QImage::Format_ARGB32_Premultiplied);
    if(renderedImage.isNull())
        throw RendererException(tr("OSPRay could not allocate the final %1 x %2 output image.").arg(size.width()).arg(size.height()));
    const std::vector<QRect> tiles = makeRenderTiles(size);
    for(size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex) {
        this_task::throwIfCanceled();
        const QRect& tile = tiles[tileIndex];
        if(tiles.size() > 1)
            progress.setText(tr("Ray-tracing frame with OSPRay (%1/%2 tiles)").arg(static_cast<int>(tileIndex + 1)).arg(static_cast<int>(tiles.size())));

        OSPRayOwnedObjects tileOwned(api);
        OSPCamera camera = createCamera(api, tileOwned, frameGraph->projectionParams(), tile.size(), tile, size);
        OSPFrameBuffer osprayFrameBuffer = tileOwned.keep(api.ospNewFrameBuffer(tile.width(), tile.height(), OSP_FB_SRGBA, OSP_FB_COLOR | OSP_FB_ACCUM));
        if(!osprayFrameBuffer)
            throw RendererException(tr("OSPRay failed to create the frame buffer."));
        api.ospResetAccumulation(osprayFrameBuffer);
        OSPFuture future = tileOwned.keep(api.ospRenderFrame(osprayFrameBuffer, renderer, camera, world));
        if(!future)
            throw RendererException(tr("OSPRay failed to start rendering the frame."));

        while(!api.ospIsReady(future, OSP_TASK_FINISHED)) {
            this_task::throwIfCanceled();
            QThread::msleep(20);
        }
        api.ospWait(future, OSP_TASK_FINISHED);

        const auto* mappedPixels = static_cast<const unsigned char*>(api.ospMapFrameBuffer(osprayFrameBuffer, OSP_FB_COLOR));
        if(!mappedPixels)
            throw RendererException(tr("OSPRay returned an empty frame buffer."));

        for(int y = 0; y < tile.height(); ++y) {
            QRgb* dst = reinterpret_cast<QRgb*>(renderedImage.scanLine(tile.y() + y)) + tile.x();
            const unsigned char* src = mappedPixels + static_cast<size_t>(tile.height() - 1 - y) * static_cast<size_t>(tile.width()) * 4;
            for(int x = 0; x < tile.width(); ++x, src += 4)
                dst[x] = qRgba(src[0], src[1], src[2], src[3]);
        }
        api.ospUnmapFrameBuffer(mappedPixels, osprayFrameBuffer);
        tileOwned.releaseAll();
    }

    // Release OSPRay objects before the C++ vectors backing shared OSPRay data
    // go out of scope. OSPRay shared data does not own application memory.
    owned.releaseAll();

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

    return SCFuture<void>::createImmediateEmpty();
}

}   // End of namespace
