////////////////////////////////////////////////////////////////////////////////////////
//
//  VisRTX renderer plugin for OVITO.
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
#include "AnariRenderingJob.h"
#include "VisRTXRenderBuffer.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <set>

#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QLibrary>
#include <QPainter>
#include <QRect>
#include <QThread>

namespace Ovito {
namespace {

std::mutex anariRenderMutex;
constexpr int RayTraceTileSize = 2048;

using ANARIObject = void*;
using ANARILibrary = void*;
using ANARIDevice = void*;
using ANARIArray1D = void*;
using ANARICamera = void*;
using ANARIFrame = void*;
using ANARIGeometry = void*;
using ANARILight = void*;
using ANARIMaterial = void*;
using ANARIRenderer = void*;
using ANARISurface = void*;
using ANARIWorld = void*;
using ANARIDataType = int;
using ANARIStatusSeverity = int;
using ANARIStatusCode = int;
using ANARIWaitMask = unsigned;
using ANARIMemoryDeleter = void (*)(const void*, const void*);
using ANARIStatusCallback = void (*)(const void*, ANARIDevice, ANARIObject, ANARIDataType, ANARIStatusSeverity, ANARIStatusCode, const char*);

enum : ANARIDataType
{
    ANARI_DATA_TYPE = 100,
    ANARI_STRING = 101,
    ANARI_BOOL = 103,
    ANARI_ARRAY1D = 504,
    ANARI_CAMERA = 507,
    ANARI_FRAME = 508,
    ANARI_GEOMETRY = 509,
    ANARI_LIGHT = 512,
    ANARI_MATERIAL = 513,
    ANARI_RENDERER = 514,
    ANARI_SURFACE = 515,
    ANARI_WORLD = 519,
    ANARI_INT32 = 1016,
    ANARI_UINT32 = 1020,
    ANARI_UINT32_VEC2 = 1021,
    ANARI_UINT32_VEC3 = 1022,
    ANARI_FLOAT32 = 1068,
    ANARI_FLOAT32_VEC2 = 1069,
    ANARI_FLOAT32_VEC3 = 1070,
    ANARI_FLOAT32_VEC4 = 1071,
    ANARI_FLOAT32_BOX2 = 2009,
    ANARI_UFIXED8_RGBA_SRGB = 2003
};

enum : ANARIWaitMask
{
    ANARI_NO_WAIT = 0,
    ANARI_WAIT = 1
};

enum : ANARIStatusSeverity
{
    ANARI_SEVERITY_FATAL_ERROR = 1,
    ANARI_SEVERITY_ERROR = 2,
    ANARI_SEVERITY_WARNING = 3
};

struct Vec2ui
{
    uint32_t x;
    uint32_t y;
};

struct Vec2f
{
    float x;
    float y;
};

struct Vec3ui
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
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

struct Box2f
{
    Vec2f lower;
    Vec2f upper;
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

Box2f imageRegion(const QRect& tile, const QSize& fullSize)
{
    return Box2f{
        Vec2f{
            static_cast<float>(tile.x()) / static_cast<float>(fullSize.width()),
            1.0f - static_cast<float>(tile.y() + tile.height()) / static_cast<float>(fullSize.height())
        },
        Vec2f{
            static_cast<float>(tile.x() + tile.width()) / static_cast<float>(fullSize.width()),
            1.0f - static_cast<float>(tile.y()) / static_cast<float>(fullSize.height())
        }
    };
}

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

QVector<RuntimeLibraryCandidate> buildAnariRuntimeCandidates(const QString& runtimePath)
{
    QVector<RuntimeLibraryCandidate> candidates;
    const QString trimmedPath = runtimePath.trimmed();

    if(!trimmedPath.isEmpty()) {
        QFileInfo info(trimmedPath);
        if(info.isDir())
            addRuntimeDirectoryCandidate(candidates, info.absoluteFilePath(), QStringLiteral("anari"));
        else
            addRuntimeCandidate(candidates, trimmedPath, info.absolutePath());
        return candidates;
    }

    addRuntimeCandidate(candidates, QStringLiteral("anari"));
    addRuntimeDirectoryCandidate(candidates, QCoreApplication::applicationDirPath(), QStringLiteral("anari"));
    addRuntimeDirectoryCandidate(candidates, qEnvironmentVariable("ANARI_ROOT"), QStringLiteral("anari"));
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("ANARI_ROOT")).filePath(QStringLiteral("bin")), QStringLiteral("anari"));
    addRuntimeDirectoryCandidate(candidates, qEnvironmentVariable("VISRTX_ROOT"), QStringLiteral("anari"));
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("VISRTX_ROOT")).filePath(QStringLiteral("bin")), QStringLiteral("anari"));
#ifdef Q_OS_WIN
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("ProgramFiles")).filePath(QStringLiteral("OVITO Basic")), QStringLiteral("anari"));
    addRuntimeDirectoryCandidate(candidates, QDir(qEnvironmentVariable("ProgramFiles(x86)")).filePath(QStringLiteral("OVITO Basic")), QStringLiteral("anari"));
#endif

    return candidates;
}

void anariStatusCallback(const void* userPtr, ANARIDevice, ANARIObject, ANARIDataType, ANARIStatusSeverity severity, ANARIStatusCode, const char* message)
{
    if(!userPtr || !message)
        return;
    if(severity <= ANARI_SEVERITY_WARNING) {
        QStringList* messages = static_cast<QStringList*>(const_cast<void*>(userPtr));
        messages->append(QString::fromUtf8(message));
    }
}

class AnariApi
{
public:

    explicit AnariApi(const QString& runtimePath)
    {
        const QVector<RuntimeLibraryCandidate> candidates = buildAnariRuntimeCandidates(runtimePath);
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
                "Failed to load the ANARI runtime library. Install the ANARI SDK/VisRTX runtime, copy anari.dll and anari_library_visrtx.dll next to ovito.exe, or set the ANARI runtime path to the folder containing anari.dll.\n\nTried:\n%1")
                .arg(loadErrors.join(QLatin1Char('\n'))));
        }

        resolveAll();
    }

    ANARILibrary (*anariLoadLibrary)(const char*, ANARIStatusCallback, const void*) = nullptr;
    void (*anariUnloadLibrary)(ANARILibrary) = nullptr;
    ANARIDevice (*anariNewDevice)(ANARILibrary, const char*) = nullptr;
    ANARIArray1D (*anariNewArray1D)(ANARIDevice, const void*, ANARIMemoryDeleter, const void*, ANARIDataType, uint64_t) = nullptr;
    ANARILight (*anariNewLight)(ANARIDevice, const char*) = nullptr;
    ANARICamera (*anariNewCamera)(ANARIDevice, const char*) = nullptr;
    ANARIGeometry (*anariNewGeometry)(ANARIDevice, const char*) = nullptr;
    ANARISurface (*anariNewSurface)(ANARIDevice) = nullptr;
    ANARIMaterial (*anariNewMaterial)(ANARIDevice, const char*) = nullptr;
    ANARIWorld (*anariNewWorld)(ANARIDevice) = nullptr;
    ANARIRenderer (*anariNewRenderer)(ANARIDevice, const char*) = nullptr;
    ANARIFrame (*anariNewFrame)(ANARIDevice) = nullptr;
    void (*anariSetParameter)(ANARIDevice, ANARIObject, const char*, ANARIDataType, const void*) = nullptr;
    void (*anariCommitParameters)(ANARIDevice, ANARIObject) = nullptr;
    void (*anariRelease)(ANARIDevice, ANARIObject) = nullptr;
    void (*anariRenderFrame)(ANARIDevice, ANARIFrame) = nullptr;
    int (*anariFrameReady)(ANARIDevice, ANARIFrame, ANARIWaitMask) = nullptr;
    const void* (*anariMapFrame)(ANARIDevice, ANARIFrame, const char*, uint32_t*, uint32_t*, ANARIDataType*) = nullptr;
    void (*anariUnmapFrame)(ANARIDevice, ANARIFrame, const char*) = nullptr;
    void (*anariDiscardFrame)(ANARIDevice, ANARIFrame) = nullptr;

private:

    void resolveAll()
    {
        resolve(anariLoadLibrary, "anariLoadLibrary");
        resolve(anariUnloadLibrary, "anariUnloadLibrary");
        resolve(anariNewDevice, "anariNewDevice");
        resolve(anariNewArray1D, "anariNewArray1D");
        resolve(anariNewLight, "anariNewLight");
        resolve(anariNewCamera, "anariNewCamera");
        resolve(anariNewGeometry, "anariNewGeometry");
        resolve(anariNewSurface, "anariNewSurface");
        resolve(anariNewMaterial, "anariNewMaterial");
        resolve(anariNewWorld, "anariNewWorld");
        resolve(anariNewRenderer, "anariNewRenderer");
        resolve(anariNewFrame, "anariNewFrame");
        resolve(anariSetParameter, "anariSetParameter");
        resolve(anariCommitParameters, "anariCommitParameters");
        resolve(anariRelease, "anariRelease");
        resolve(anariRenderFrame, "anariRenderFrame");
        resolve(anariFrameReady, "anariFrameReady");
        resolve(anariMapFrame, "anariMapFrame");
        resolve(anariUnmapFrame, "anariUnmapFrame");
        resolve(anariDiscardFrame, "anariDiscardFrame");
    }

    template<typename FunctionPointer>
    void resolve(FunctionPointer& fn, const char* symbolName)
    {
        fn = reinterpret_cast<FunctionPointer>(_library.resolve(symbolName));
        if(!fn)
            throw RendererException(QObject::tr("The loaded ANARI runtime is missing required symbol '%1'.").arg(QString::fromLatin1(symbolName)));
    }

    QLibrary _library;
};

class AnariSession
{
public:

    AnariSession(AnariApi& api, const OffscreenAnariRenderer& renderer) : _api(api)
    {
        _libraryName = renderer.anariLibraryName().trimmed().toLatin1();
        if(_libraryName.isEmpty())
            _libraryName = "visrtx";

        _library = _api.anariLoadLibrary(_libraryName.constData(), anariStatusCallback, &_statusMessages);
        if(!_library) {
            QString details = _statusMessages.isEmpty() ? QString() : QStringLiteral("\n\n%1").arg(_statusMessages.join(QLatin1Char('\n')));
            throw RendererException(QObject::tr("Failed to load ANARI library '%1'. Make sure the VisRTX runtime is installed and anari_library_%1.dll can be found.%2").arg(QString::fromLatin1(_libraryName), details));
        }

        _deviceSubtype = renderer.deviceSubtype().trimmed().toLatin1();
        if(_deviceSubtype.isEmpty())
            _deviceSubtype = "default";

        _device = _api.anariNewDevice(_library, _deviceSubtype.constData());
        if(!_device) {
            QString details = _statusMessages.isEmpty() ? QString() : QStringLiteral("\n\n%1").arg(_statusMessages.join(QLatin1Char('\n')));
            throw RendererException(QObject::tr("Failed to create ANARI device subtype '%1' from library '%2'.%3").arg(QString::fromLatin1(_deviceSubtype), QString::fromLatin1(_libraryName), details));
        }
        _api.anariCommitParameters(_device, _device);
    }

    ~AnariSession()
    {
        if(_device)
            _api.anariRelease(_device, _device);
        if(_library)
            _api.anariUnloadLibrary(_library);
    }

    ANARIDevice device() const { return _device; }

private:

    AnariApi& _api;
    QByteArray _libraryName;
    QByteArray _deviceSubtype;
    QStringList _statusMessages;
    ANARILibrary _library = nullptr;
    ANARIDevice _device = nullptr;
};

class AnariOwnedObjects
{
public:

    AnariOwnedObjects(AnariApi& api, ANARIDevice device) : _api(api), _device(device) {}

    template<typename T>
    T keep(T handle)
    {
        if(handle)
            _handles.push_back(static_cast<ANARIObject>(handle));
        return handle;
    }

    ~AnariOwnedObjects()
    {
        releaseAll();
    }

    void releaseAll()
    {
        for(auto it = _handles.rbegin(); it != _handles.rend(); ++it) {
            if(*it)
                _api.anariRelease(_device, *it);
        }
        _handles.clear();
    }

private:

    AnariApi& _api;
    ANARIDevice _device;
    std::vector<ANARIObject> _handles;
};

ANARIArray1D makeArray(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device, const void* data, ANARIDataType type, size_t count)
{
    ANARIArray1D array = owned.keep(api.anariNewArray1D(device, data, nullptr, nullptr, type, static_cast<uint64_t>(count)));
    if(!array)
        throw RendererException(QObject::tr("VisRTX failed to create shared scene data."));
    return array;
}

template<typename T>
void setParam(AnariApi& api, ANARIDevice device, ANARIObject object, const char* name, ANARIDataType type, const T& value)
{
    api.anariSetParameter(device, object, name, type, &value);
}

template<typename T>
void setObjectParam(AnariApi& api, ANARIDevice device, ANARIObject object, const char* name, ANARIDataType type, T value)
{
    api.anariSetParameter(device, object, name, type, &value);
}

void setStringParam(AnariApi& api, ANARIDevice device, ANARIObject object, const char* name, const char* value)
{
    api.anariSetParameter(device, object, name, ANARI_STRING, value);
}

ANARIMaterial createDefaultMaterial(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device)
{
    ANARIMaterial material = owned.keep(api.anariNewMaterial(device, "matte"));
    if(!material)
        throw RendererException(QObject::tr("VisRTX failed to create the default material."));
    setStringParam(api, device, material, "color", "color");
    api.anariCommitParameters(device, material);
    return material;
}

class AnariSceneBuilder
{
public:

    AnariSceneBuilder(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device, ANARIMaterial material, RenderBuffer& renderBuffer) :
        _api(api), _owned(owned), _device(device), _material(material), _renderBuffer(renderBuffer) {}

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
            reportOnce("VisRTX renderer does not currently convert OVITO volume primitives.");
    }

    const std::vector<ANARISurface>& surfaces() const { return _surfaces; }

private:

    void renderParticles(const ParticlePrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.positions() || primitive.positions()->size() == 0)
            return;

        if(primitive.particleShape() != ParticlePrimitive::SphericalShape)
            reportOnce("VisRTX renderer currently renders non-spherical particle glyphs as spheres.");

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

        ANARIGeometry geometry = _owned.keep(_api.anariNewGeometry(_device, "sphere"));
        if(!geometry) {
            reportOnce("VisRTX failed to create sphere geometry.");
            return;
        }
        ANARIArray1D positionArray = makeArray(_api, _owned, _device, positions.data(), ANARI_FLOAT32_VEC3, positions.size());
        ANARIArray1D radiusArray = makeArray(_api, _owned, _device, radii.data(), ANARI_FLOAT32, radii.size());
        ANARIArray1D colorArray = makeArray(_api, _owned, _device, colors.data(), ANARI_FLOAT32_VEC4, colors.size());
        setObjectParam(_api, _device, geometry, "vertex.position", ANARI_ARRAY1D, positionArray);
        setObjectParam(_api, _device, geometry, "vertex.radius", ANARI_ARRAY1D, radiusArray);
        setObjectParam(_api, _device, geometry, "vertex.color", ANARI_ARRAY1D, colorArray);
        _api.anariCommitParameters(_device, geometry);

        addSurface(geometry);
        _ownedVectors.emplace_back(std::move(positions), std::move(radii), std::move(colors));
    }

    void renderCylinders(const CylinderPrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.basePositions() || !primitive.headPositions() || primitive.basePositions()->size() == 0)
            return;
        if(primitive.colors() && primitive.colors()->componentCount() == 1)
            reportOnce("VisRTX renderer currently ignores one-component pseudo-colors on cylinders.");

        std::vector<Vec3f> vertices;
        std::vector<Vec4f> colors;
        std::vector<float> radii;

        auto gatherPositions = [&](auto _) {
            using T = decltype(_);
            BufferReadAccess<Point_3<T>> bases(primitive.basePositions());
            BufferReadAccess<Point_3<T>> heads(primitive.headPositions());
            const size_t count = std::min(bases.size(), heads.size());

            for(size_t i = 0; i < count; ++i) {
                Point3 base = tm * bases[i].template toDataType<FloatType>();
                Point3 head = tm * heads[i].template toDataType<FloatType>();
                if((head - base).length() <= 0)
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

                vertices.push_back(toVec3f(base));
                vertices.push_back(toVec3f(head));
                colors.push_back(toVec4f(color));
                colors.push_back(toVec4f(color));
                radii.push_back(radius);
            }
        };

        primitive.basePositions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(gatherPositions);
        addCylinderGeometry(std::move(vertices), std::move(colors), std::move(radii));
    }

    void renderMesh(const MeshPrimitive& primitive, const AffineTransformation& tm)
    {
        if(!primitive.mesh() || primitive.faceCount() == 0)
            return;
        if(primitive.useInstancedRendering())
            reportOnce("VisRTX renderer currently renders only the base mesh for instanced mesh primitives.");
        if(primitive.mesh()->hasVertexPseudoColors() || primitive.mesh()->hasFacePseudoColors())
            reportOnce("VisRTX renderer currently ignores mesh pseudo-color mappings.");

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

        ANARIGeometry geometry = _owned.keep(_api.anariNewGeometry(_device, "triangle"));
        if(!geometry) {
            reportOnce("VisRTX failed to create triangle geometry.");
            return;
        }
        ANARIArray1D positionArray = makeArray(_api, _owned, _device, positions.data(), ANARI_FLOAT32_VEC3, positions.size());
        ANARIArray1D normalArray = makeArray(_api, _owned, _device, normals.data(), ANARI_FLOAT32_VEC3, normals.size());
        ANARIArray1D colorArray = makeArray(_api, _owned, _device, colors.data(), ANARI_FLOAT32_VEC4, colors.size());
        ANARIArray1D indexArray = makeArray(_api, _owned, _device, indices.data(), ANARI_UINT32_VEC3, indices.size());
        setObjectParam(_api, _device, geometry, "vertex.position", ANARI_ARRAY1D, positionArray);
        setObjectParam(_api, _device, geometry, "vertex.normal", ANARI_ARRAY1D, normalArray);
        setObjectParam(_api, _device, geometry, "vertex.color", ANARI_ARRAY1D, colorArray);
        setObjectParam(_api, _device, geometry, "primitive.index", ANARI_ARRAY1D, indexArray);
        _api.anariCommitParameters(_device, geometry);

        addSurface(geometry);
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

        std::vector<Vec3f> vertices;
        std::vector<Vec4f> colors;
        std::vector<float> radii;
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

                vertices.push_back(toVec3f(base));
                vertices.push_back(toVec3f(head));
                colors.push_back(toVec4f(color));
                colors.push_back(toVec4f(color));
                radii.push_back(static_cast<float>(fallbackRadius));
            }
        };

        primitive.positions()->forTypes<DataBuffer::Float32, DataBuffer::Float64>(gatherPositions);
        addCylinderGeometry(std::move(vertices), std::move(colors), std::move(radii));
    }

    void addCylinderGeometry(std::vector<Vec3f> vertices, std::vector<Vec4f> colors, std::vector<float> radii)
    {
        if(vertices.empty() || radii.empty())
            return;

        ANARIGeometry geometry = _owned.keep(_api.anariNewGeometry(_device, "cylinder"));
        if(!geometry) {
            reportOnce("VisRTX failed to create cylinder geometry.");
            return;
        }
        ANARIArray1D vertexArray = makeArray(_api, _owned, _device, vertices.data(), ANARI_FLOAT32_VEC3, vertices.size());
        ANARIArray1D colorArray = makeArray(_api, _owned, _device, colors.data(), ANARI_FLOAT32_VEC4, colors.size());
        ANARIArray1D radiusArray = makeArray(_api, _owned, _device, radii.data(), ANARI_FLOAT32, radii.size());
        setObjectParam(_api, _device, geometry, "vertex.position", ANARI_ARRAY1D, vertexArray);
        setObjectParam(_api, _device, geometry, "vertex.color", ANARI_ARRAY1D, colorArray);
        setObjectParam(_api, _device, geometry, "primitive.radius", ANARI_ARRAY1D, radiusArray);
        setStringParam(_api, _device, geometry, "caps", "both");
        _api.anariCommitParameters(_device, geometry);

        addSurface(geometry);
        _ownedVectors.emplace_back(std::move(vertices), std::move(colors), std::move(radii));
    }

    void addSurface(ANARIGeometry geometry)
    {
        ANARISurface surface = _owned.keep(_api.anariNewSurface(_device));
        if(!surface) {
            reportOnce("VisRTX failed to create a surface object.");
            return;
        }
        setObjectParam(_api, _device, surface, "geometry", ANARI_GEOMETRY, geometry);
        setObjectParam(_api, _device, surface, "material", ANARI_MATERIAL, _material);
        _api.anariCommitParameters(_device, surface);
        _surfaces.push_back(surface);
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
        std::vector<Vec3ui> vec3uis;

        OwnedVectorStorage(std::vector<Vec3f>&& positions, std::vector<float>&& radii, std::vector<Vec4f>&& colors) :
            vec3a(std::move(positions)), vec4a(std::move(colors)), floats(std::move(radii)) {}

        OwnedVectorStorage(std::vector<Vec3f>&& positions, std::vector<Vec3f>&& normals, std::vector<Vec4f>&& colors, std::vector<Vec3ui>&& indices) :
            vec3a(std::move(positions)), vec3b(std::move(normals)), vec4a(std::move(colors)), vec3uis(std::move(indices)) {}

        OwnedVectorStorage(std::vector<Vec3f>&& vertices, std::vector<Vec4f>&& colors, std::vector<float>&& radii) :
            vec3a(std::move(vertices)), vec4a(std::move(colors)), floats(std::move(radii)) {}
    };

private:

    AnariApi& _api;
    AnariOwnedObjects& _owned;
    ANARIDevice _device;
    ANARIMaterial _material;
    RenderBuffer& _renderBuffer;
    std::vector<ANARISurface> _surfaces;
    std::vector<OwnedVectorStorage> _ownedVectors;
    std::set<QByteArray> _reportedIssues;
};

ANARICamera createCamera(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device, const ViewProjectionParameters& projectionParams, const QSize& tileSize, const QRect& tile, const QSize& fullSize)
{
    const bool perspective = projectionParams.isPerspective;
    ANARICamera camera = owned.keep(api.anariNewCamera(device, perspective ? "perspective" : "orthographic"));
    if(!camera)
        throw RendererException(QObject::tr("VisRTX failed to create the camera."));

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
    setParam(api, device, camera, "position", ANARI_FLOAT32_VEC3, position);
    setParam(api, device, camera, "direction", ANARI_FLOAT32_VEC3, direction);
    setParam(api, device, camera, "up", ANARI_FLOAT32_VEC3, up);
    setParam(api, device, camera, "aspect", ANARI_FLOAT32, aspect);
    if(tile.size() != fullSize) {
        const Box2f region = imageRegion(tile, fullSize);
        setParam(api, device, camera, "imageRegion", ANARI_FLOAT32_BOX2, region);
    }

    if(perspective) {
        const float fovy = static_cast<float>(projectionParams.fieldOfView);
        setParam(api, device, camera, "fovy", ANARI_FLOAT32, fovy);
    }
    else {
        const float height = static_cast<float>(std::max(projectionParams.fieldOfView, FloatType(1e-6)));
        setParam(api, device, camera, "height", ANARI_FLOAT32, height);
    }

    api.anariCommitParameters(device, camera);
    return camera;
}

std::vector<ANARILight> createLights(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device, const OffscreenAnariRenderer& renderer, const ViewProjectionParameters& projectionParams)
{
    std::vector<ANARILight> lights;

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

        ANARILight directional = owned.keep(api.anariNewLight(device, "directional"));
        if(!directional)
            throw RendererException(QObject::tr("VisRTX failed to create a directional light."));
        Vec3f direction = toVec3f(lightDir);
        float irradiance = static_cast<float>(std::max(FloatType(0), renderer.directLightIrradiance()));
        setParam(api, device, directional, "direction", ANARI_FLOAT32_VEC3, direction);
        setParam(api, device, directional, "irradiance", ANARI_FLOAT32, irradiance);
        api.anariCommitParameters(device, directional);
        lights.push_back(directional);
    }

    return lights;
}

ANARIRenderer createRenderer(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device, const OffscreenAnariRenderer& renderer, const ColorA& clearColor)
{
    QByteArray rendererSubtype = renderer.rendererSubtype().trimmed().toLatin1();
    if(rendererSubtype.isEmpty())
        rendererSubtype = "default";

    ANARIRenderer anariRenderer = owned.keep(api.anariNewRenderer(device, rendererSubtype.constData()));
    if(!anariRenderer)
        throw RendererException(QObject::tr("VisRTX failed to create renderer subtype '%1'.").arg(QString::fromLatin1(rendererSubtype)));
    int pixelSamples = std::max(1, renderer.samplesPerPixel());
    int ambientSamples = renderer.ambientOcclusion() ? std::max(0, renderer.ambientSamples()) : 0;
    int maxRayDepth = std::max(1, renderer.maxRayDepth());
    bool denoise = renderer.denoise();
    float ambientRadiance = static_cast<float>(std::max(FloatType(0), renderer.ambientRadiance()));
    Vec3f ambientColor{ 1.0f, 1.0f, 1.0f };
    Vec4f background{
        static_cast<float>(clamp01(clearColor.r())),
        static_cast<float>(clamp01(clearColor.g())),
        static_cast<float>(clamp01(clearColor.b())),
        1.0f
    };
    setParam(api, device, anariRenderer, "pixelSamples", ANARI_INT32, pixelSamples);
    setParam(api, device, anariRenderer, "ambientRadiance", ANARI_FLOAT32, ambientRadiance);
    setParam(api, device, anariRenderer, "ambientColor", ANARI_FLOAT32_VEC3, ambientColor);
    setParam(api, device, anariRenderer, "ambientSamples", ANARI_INT32, ambientSamples);
    setParam(api, device, anariRenderer, "maxRayDepth", ANARI_INT32, maxRayDepth);
    setParam(api, device, anariRenderer, "denoise", ANARI_BOOL, denoise);
    setParam(api, device, anariRenderer, "background", ANARI_FLOAT32_VEC4, background);
    api.anariCommitParameters(device, anariRenderer);
    return anariRenderer;
}

ANARIWorld createWorld(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device, const std::vector<ANARISurface>& surfaces, std::vector<ANARILight>& lights)
{
    ANARIWorld world = owned.keep(api.anariNewWorld(device));
    if(!world)
        throw RendererException(QObject::tr("VisRTX failed to create the world."));
    if(!surfaces.empty()) {
        ANARIArray1D surfaceArray = makeArray(api, owned, device, surfaces.data(), ANARI_SURFACE, surfaces.size());
        setObjectParam(api, device, world, "surface", ANARI_ARRAY1D, surfaceArray);
    }
    if(!lights.empty()) {
        ANARIArray1D lightArray = makeArray(api, owned, device, lights.data(), ANARI_LIGHT, lights.size());
        setObjectParam(api, device, world, "light", ANARI_ARRAY1D, lightArray);
    }
    api.anariCommitParameters(device, world);
    return world;
}

ANARIFrame createFrame(AnariApi& api, AnariOwnedObjects& owned, ANARIDevice device, const QSize& size, ANARIRenderer renderer, ANARICamera camera, ANARIWorld world)
{
    ANARIFrame frame = owned.keep(api.anariNewFrame(device));
    if(!frame)
        throw RendererException(QObject::tr("VisRTX failed to create the frame."));
    Vec2ui frameSize{ static_cast<uint32_t>(size.width()), static_cast<uint32_t>(size.height()) };
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    setParam(api, device, frame, "size", ANARI_UINT32_VEC2, frameSize);
    setParam(api, device, frame, "channel.color", ANARI_DATA_TYPE, colorType);
    setObjectParam(api, device, frame, "renderer", ANARI_RENDERER, renderer);
    setObjectParam(api, device, frame, "camera", ANARI_CAMERA, camera);
    setObjectParam(api, device, frame, "world", ANARI_WORLD, world);
    api.anariCommitParameters(device, frame);
    return frame;
}

} // End anonymous namespace

IMPLEMENT_ABSTRACT_OVITO_CLASS(AnariRenderingJob);

/******************************************************************************
* Constructor.
******************************************************************************/
void AnariRenderingJob::initializeObject(ObjectInitializationFlags flags, std::shared_ptr<RendererResourceCache> visCache, OORef<const OffscreenAnariRenderer> sceneRenderer)
{
    RenderingJob::initializeObject(flags);
    _visCache = std::move(visCache);
    _sceneRenderer = std::move(sceneRenderer);
}

/******************************************************************************
* Creates a new abstract target frame buffer for rendering into.
******************************************************************************/
OORef<RenderBuffer> AnariRenderingJob::createOffscreenRenderBuffer(const QSize& deviceIndependentSize)
{
    return OORef<VisRTXRenderBuffer>::create(deviceIndependentSize);
}

/******************************************************************************
* Renders an image of the given frame graph.
******************************************************************************/
SCFuture<void> AnariRenderingJob::renderFrame(std::shared_ptr<const FrameGraph> frameGraph, OORef<RenderBuffer> renderBuffer, std::shared_ptr<FrameBuffer> frameBuffer, TaskProgress& progress)
{
    if(!frameBuffer)
        return SCFuture<void>::createImmediateEmpty();
    if(!_sceneRenderer)
        throw RendererException(tr("Cannot render scene: VisRTX renderer settings object is missing."));

    const QSize size = renderBuffer->size();
    if(size.width() <= 0 || size.height() <= 0)
        return SCFuture<void>::createImmediateEmpty();

    progress.setText(tr("Ray-tracing frame with VisRTX"));

    std::lock_guard<std::mutex> guard(anariRenderMutex);
    AnariApi api(_sceneRenderer->anariRuntimePath());
    AnariSession session(api, *_sceneRenderer);
    ANARIDevice device = session.device();
    AnariOwnedObjects owned(api, device);

    ANARIMaterial material = createDefaultMaterial(api, owned, device);
    AnariSceneBuilder builder(api, owned, device, material, *renderBuffer);
    for(const FrameGraph::RenderingCommandGroup& group : frameGraph->commandGroups()) {
        if(group.layerType() == FrameGraph::UnderLayer || group.layerType() == FrameGraph::OverLayer)
            continue;
        for(const FrameGraph::RenderingCommand& command : group.commands()) {
            this_task::throwIfCanceled();
            builder.renderPrimitive(command);
        }
    }

    ANARIRenderer renderer = createRenderer(api, owned, device, *_sceneRenderer, frameGraph->clearColor());
    std::vector<ANARILight> lights = createLights(api, owned, device, *_sceneRenderer, frameGraph->projectionParams());
    ANARIWorld world = createWorld(api, owned, device, builder.surfaces(), lights);

    QImage renderedImage(size, QImage::Format_ARGB32_Premultiplied);
    if(renderedImage.isNull())
        throw RendererException(tr("VisRTX could not allocate the final %1 x %2 output image.").arg(size.width()).arg(size.height()));
    const std::vector<QRect> tiles = makeRenderTiles(size);
    for(size_t tileIndex = 0; tileIndex < tiles.size(); ++tileIndex) {
        this_task::throwIfCanceled();
        const QRect& tile = tiles[tileIndex];
        if(tiles.size() > 1)
            progress.setText(tr("Ray-tracing frame with VisRTX (%1/%2 tiles)").arg(static_cast<int>(tileIndex + 1)).arg(static_cast<int>(tiles.size())));

        AnariOwnedObjects tileOwned(api, device);
        ANARICamera camera = createCamera(api, tileOwned, device, frameGraph->projectionParams(), tile.size(), tile, size);
        ANARIFrame frame = createFrame(api, tileOwned, device, tile.size(), renderer, camera, world);

        api.anariRenderFrame(device, frame);
        while(!api.anariFrameReady(device, frame, ANARI_NO_WAIT)) {
            this_task::throwIfCanceled();
            QThread::msleep(20);
        }
        api.anariFrameReady(device, frame, ANARI_WAIT);

        uint32_t width = 0;
        uint32_t height = 0;
        ANARIDataType pixelType = 0;
        const auto* mappedPixels = static_cast<const unsigned char*>(api.anariMapFrame(device, frame, "channel.color", &width, &height, &pixelType));
        if(!mappedPixels)
            throw RendererException(tr("VisRTX returned an empty frame buffer."));
        if(width != static_cast<uint32_t>(tile.width()) || height != static_cast<uint32_t>(tile.height()) || pixelType != ANARI_UFIXED8_RGBA_SRGB) {
            api.anariUnmapFrame(device, frame, "channel.color");
            throw RendererException(tr("VisRTX returned an unexpected frame buffer format."));
        }

        for(int y = 0; y < tile.height(); ++y) {
            QRgb* dst = reinterpret_cast<QRgb*>(renderedImage.scanLine(tile.y() + y)) + tile.x();
            const unsigned char* src = mappedPixels + static_cast<size_t>(tile.height() - 1 - y) * static_cast<size_t>(tile.width()) * 4;
            for(int x = 0; x < tile.width(); ++x, src += 4)
                dst[x] = qRgba(src[0], src[1], src[2], src[3]);
        }
        api.anariUnmapFrame(device, frame, "channel.color");
        tileOwned.releaseAll();
    }

    // Release ANARI objects before the C++ vectors backing shared ANARI arrays
    // go out of scope. ANARI arrays created from application memory are shared.
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
