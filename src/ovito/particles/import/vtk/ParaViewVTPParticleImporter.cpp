////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/mesh/surface/SurfaceMeshReadAccess.h>
#include <ovito/mesh/io/ParaViewVTPMeshImporter.h>
#include <ovito/core/dataset/io/FileSource.h>
#include "ParaViewVTPParticleImporter.h"
#include <ovito/stdobj/table/DataTable.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(ParaViewVTPParticleImporter);
OVITO_CLASSINFO(ParaViewVTPParticleImporter, "DisplayName", "VTP");
IMPLEMENT_CREATABLE_OVITO_CLASS(ParticlesParaViewVTMFileFilter);

/******************************************************************************
* Checks if the given file has format that can be read by this importer.
******************************************************************************/
bool ParaViewVTPParticleImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(device.get());

    // Parse XML. First element must be <VTKFile type="PolyData">.
    if(xml.readNext() != QXmlStreamReader::StartDocument)
        return false;
    if(xml.readNext() != QXmlStreamReader::StartElement)
        return false;
    if(xml.name().compare(QLatin1String("VTKFile")) != 0)
        return false;
    if(xml.attributes().value("type").compare(QLatin1String("PolyData")) != 0)
        return false;

    // Continue until we reach the <Piece> element.
    while(xml.readNextStartElement()) {
        if(xml.name().compare(QLatin1String("Piece")) == 0) {
            // Number of lines, triangle strips, and polygons must be zero.
            if(xml.attributes().value("NumberOfLines").toULongLong() == 0 && xml.attributes().value("NumberOfStrips").toULongLong() == 0 && xml.attributes().value("NumberOfPolys").toULongLong() == 0) {
                // Number of vertices must match number of points.
                if(xml.attributes().value("NumberOfPoints") == xml.attributes().value("NumberOfVerts")) {
                    return !xml.hasError();
                }
            }
            break;
        }
    }

    return false;
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void ParaViewVTPParticleImporter::FrameLoader::loadFile()
{
    setProgressText(tr("Reading ParaView VTP particles file %1").arg(fileHandle().toString()));

    // Initialize XML reader and open input file.
    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly | QIODevice::Text))
        throw Exception(tr("Failed to open VTP file: %1").arg(device->errorString()));
    QXmlStreamReader xml(device.get());

    // Determine the target container for the per-particle or per-body data read from the VTP file.
    PropertyContainer* container;
    if(!_isBodiesFile) {
        container = particles();
    }
    else {
        container = state().getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("bodies"));
        if(!container)
            container = state().createObject<DataTable>(QStringLiteral("bodies"), pipelineNode(), DataTable::None, tr("Non-convex bodies"));
    }

    // Append particles to existing container when requested by the caller.
    // This may be the case when loading a multi-block dataset specified in a VTM file.
    size_t baseParticleIndex = 0;
    bool preserveExistingData = false;
    if(loadRequest().appendData) {
        baseParticleIndex = container->elementCount();
        preserveExistingData = (baseParticleIndex != 0);
    }

    // Aspherix stores bonds in a separate VTK file, which gets loaded alongside with this particles files.
    // To preserve the bonds loaded by ParaViewVTPBondsImporter, we have to explicitly tell the ParticleImporter base class here
    // to NOT reset the bonds list (which it would otherwise do, because ParaViewVTPParticleImporter doesn't create any bonds).
    setKeepExistingTopology(true);

    PropertyPtr roundnessProperty;
    PropertyPtr typeProperty;
    PropertyPtr tensorProperty;

    // Parse the elements of the XML file.
    while(xml.readNextStartElement()) {
        this_task::throwIfCanceled();

        if(xml.name().compare(QLatin1String("VTKFile")) == 0) {
            if(xml.attributes().value("type").compare(QLatin1String("PolyData")) != 0)
                xml.raiseError(tr("VTK file is not of type PolyData."));
            else if(xml.attributes().value("byte_order").compare(QLatin1String("LittleEndian")) != 0)
                xml.raiseError(tr("Byte order must be 'LittleEndian'. Please contact the OVITO developers to request an extension of the file parser."));
            else if(xml.attributes().value("compressor").compare(QLatin1String("")) != 0)
                xml.raiseError(tr("The parser does not support compressed data arrays. Please contact the OVITO developers to request an extension of the file parser."));
        }
        else if(xml.name().compare(QLatin1String("PolyData")) == 0) {
            // Do nothing. Parse child elements.
        }
        else if(xml.name().compare(QLatin1String("Piece")) == 0) {

            // Parse number of lines, triangle strips and polygons.
            if(xml.attributes().value("NumberOfLines").toULongLong() != 0
                    || xml.attributes().value("NumberOfStrips").toULongLong() != 0
                    || xml.attributes().value("NumberOfPolys").toULongLong() != 0) {
                xml.raiseError(tr("Number of lines, strips and polys are nonzero. This particle file parser can only read PolyData datasets containing vertices only."));
                break;
            }

            // Parse number of points.
            size_t numParticles = xml.attributes().value("NumberOfPoints").toULongLong();
            // Parse number of vertices.
            size_t numVertices = xml.attributes().value("NumberOfVerts").toULongLong();
            if(numVertices != numParticles) {
                xml.raiseError(tr("Number of vertices does not match number of points. This file parser can only read datasets consisting of vertices only."));
                break;
            }
            OVITO_ASSERT(baseParticleIndex + numParticles != 0);
            container->setElementCount(baseParticleIndex + numParticles);
        }
        else if(xml.name().compare(QLatin1String("PointData")) == 0 || xml.name().compare(QLatin1String("Points")) == 0 || xml.name().compare(QLatin1String("Verts")) == 0) {
            // Parse child elements.
            while(xml.readNextStartElement() && !isCanceled()) {
                if(xml.name().compare(QLatin1String("DataArray")) == 0) {
                    int vectorComponent = -1;
                    if(Property* property = createPropertyForDataArray(xml, container, vectorComponent, preserveExistingData)) {
                        if(!ParaViewVTPMeshImporter::parseVTKDataArray(property, xml, vectorComponent, baseParticleIndex))
                            break;
                        if(xml.hasError() || isCanceled())
                            break;

                        if(!_isBodiesFile) {
                            if(property->typeId() == Particles::SuperquadricRoundnessProperty)
                                roundnessProperty = property;
                            if(property->typeId() == Particles::TypeProperty)
                                typeProperty = property;
                            else if(property->name() == QStringLiteral("Tensor"))
                                tensorProperty = property;

                            // Create particle types if this is a typed property.
                            OvitoClassPtr elementTypeClass = Particles::OOClass().typedPropertyElementClass(property->typeId());
                            if(!elementTypeClass && property->name() == QStringLiteral("Material Type"))
                                elementTypeClass = &ElementType::OOClass();
                            if(elementTypeClass) {
                                for(int t : BufferReadAccess<int32_t>(property).subrange(baseParticleIndex)) {
                                    if(!property->elementType(t)) {
                                        DataOORef<ElementType> elementType =
                                            static_object_cast<ElementType>(elementTypeClass->createInstance());
                                        elementType->setNumericId(t);
                                        elementType->initializeType(OwnerPropertyRef(&Particles::OOClass(), property),
                                                                    this_task::isInteractive());
                                        if(elementTypeClass == &ParticleType::OOClass()) {
                                            // Load mesh-based shape of the particle type as specified in the VTM container file.
                                            loadParticleShape(static_object_cast<ParticleType>(elementType.get()));
                                        }
                                        property->addElementType(std::move(elementType));
                                    }
                                }
                            }
                        }
                    }
                    if(xml.tokenType() != QXmlStreamReader::EndElement)
                        xml.skipCurrentElement();
                }
                else {
                    xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
                }
            }
        }
        else if(xml.name().compare(QStringLiteral("FieldData")) == 0 || xml.name().compare(QLatin1String("CellData")) == 0 || xml.name().compare(QLatin1String("Lines")) == 0 || xml.name().compare(QLatin1String("Strips")) == 0 || xml.name().compare(QLatin1String("Polys")) == 0) {
            // Do nothing. Ignore element contents.
            xml.skipCurrentElement();
        }
        else {
            xml.raiseError(tr("Unexpected XML element <%1>.").arg(xml.name().toString()));
        }
    }

    // Handle XML parsing errors.
    if(xml.hasError()) {
        throw Exception(tr("VTP file parsing error on line %1, column %2: %3")
            .arg(xml.lineNumber()).arg(xml.columnNumber()).arg(xml.errorString()));
    }
    this_task::throwIfCanceled();

    // Convert superquadric 'Blockiness' values from the Aspherix simulation to 'Roundness' values used by OVITO particle visualization.
    bool transposeOrientations = false;
    if(roundnessProperty) {
        for(auto& v :
            BufferWriteAccess<Vector_2<GraphicsFloatType>, access_mode::read_write>(roundnessProperty).subrange(baseParticleIndex)) {
            // Blockiness1: "north-south" blockiness
            // Blockiness2: "east-west" blockiness
            // Roundness.x: "east-west" roundness
            // Roundness.y: "north-south" roundness
            std::swap(v.x(), v.y());
            // Roundness = 2.0 / Blockiness:
            if(v.x() != 0) v.x() = GraphicsFloatType(2) / v.x();
            if(v.y() != 0) v.y() = GraphicsFloatType(2) / v.y();
        }
        transposeOrientations = true;
        this_task::throwIfCanceled();
    }

    // Convert 3x3 'Tensor' property into particle orientation.
    if(tensorProperty && tensorProperty->dataType() == Property::FloatDefault && tensorProperty->componentCount() == 9) {
        BufferWriteAccess<QuaternionG, access_mode::write> orientations(
            container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::OrientationProperty),
            preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized);
        auto* q = orientations.begin() + baseParticleIndex;
        for(const Matrix3& tensor : BufferReadAccess<Matrix3>(tensorProperty).subrange(baseParticleIndex)) {
            if(!tensor.isZero())
                *q++ =
                    Quaternion(transposeOrientations ? tensor.transposed() : tensor, FloatType(1e-6)).toDataType<GraphicsFloatType>();
            else
                *q++ = QuaternionG::Identity();
        }
        this_task::throwIfCanceled();
    }

    // Reset "Radius" property of particles with a mesh-based shape to zero to get correct scaling.
    if(typeProperty) {
        std::vector<int> typesWithMeshShape;
        for(const ElementType* type : typeProperty->elementTypes()) {
            if(const ParticleType* particleType = dynamic_object_cast<ParticleType>(type))
                if(particleType->shape() == ParticlesVis::ParticleShape::Mesh)
                    typesWithMeshShape.push_back(particleType->numericId());
        }
        if(typesWithMeshShape.size() == typeProperty->elementTypes().size()) {
            // If all particle shapes are mesh-based, simply remove the "Radius" property, which is not used in this case anyway.
            if(const Property* radiusProperty = container->getProperty(Particles::RadiusProperty))
                container->removeProperty(radiusProperty);
        }
        else if(!typesWithMeshShape.empty()) {
            if(BufferWriteAccess<GraphicsFloatType, access_mode::write> radiusArray = container->getMutableProperty(Particles::RadiusProperty)) {
                auto* radius = radiusArray.begin() + baseParticleIndex;
                for(auto t : BufferReadAccess<int32_t>(typeProperty).subrange(baseParticleIndex)) {
                    if(std::find(typesWithMeshShape.cbegin(), typesWithMeshShape.cend(), t) != typesWithMeshShape.cend()) *radius = 0;
                    ++radius;
                }
            }
        }
    }

    // Report number of particles to the user.
    QString statusString = (!_isBodiesFile ? tr("Particles: %1") : tr("Bodies: %1")).arg(container->elementCount());
    state().setStatus(std::move(statusString));

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

/******************************************************************************
* Creates the right kind of OVITO property object that will receive the data
* read from a <DataArray> element.
******************************************************************************/
Property* ParaViewVTPParticleImporter::FrameLoader::createPropertyForDataArray(QXmlStreamReader& xml, PropertyContainer* container, int& vectorComponent, bool preserveExistingData)
{
    int numComponents = std::max(1, xml.attributes().value("NumberOfComponents").toInt());
    auto name = xml.attributes().value("Name");

    if(!_isBodiesFile) {
        if(name.compare(QLatin1String("connectivity"), Qt::CaseInsensitive) == 0 || name.compare(QLatin1String("offsets"), Qt::CaseInsensitive) == 0) {
            return nullptr;
        }
        else if(name.compare(QLatin1String("points"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::PositionProperty);
        }
        else if(name.compare(QLatin1String("id"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::IdentifierProperty);
        }
        else if(name.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            Property* property = container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Material Type"), Property::Int32);
            property->setTitle(tr("Material types"));
            return property;
        }
        else if(name.compare(QLatin1String("shapetype"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::TypeProperty);
        }
        else if(name.compare(QLatin1String("body"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("body"), Property::Int32, numComponents);
        }
        else if(name.compare(QLatin1String("mass"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::MassProperty);
        }
        else if(name.compare(QLatin1String("radius"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::RadiusProperty);
        }
        else if(name.compare(QLatin1String("v"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::VelocityProperty);
        }
        else if(name.compare(QLatin1String("omega"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::AngularVelocityProperty);
        }
        else if(name.compare(QLatin1String("tq"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::TorqueProperty);
        }
        else if(name.compare(QLatin1String("f"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Particles::ForceProperty);
        }
        else if(name.compare(QLatin1String("density"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Density"), Property::FloatDefault);
        }
        else if(name.compare(QLatin1String("tensor"), Qt::CaseInsensitive) == 0 && numComponents == 9) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Tensor"), Property::FloatDefault, 9);
        }
        else if(name.compare(QLatin1String("shapex"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            vectorComponent = 0;
            return container->createProperty(DataBuffer::Initialized, Particles::AsphericalShapeProperty);
        }
        else if(name.compare(QLatin1String("shapey"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            vectorComponent = 1;
            return container->createProperty(DataBuffer::Initialized, Particles::AsphericalShapeProperty);
        }
        else if(name.compare(QLatin1String("shapez"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            vectorComponent = 2;
            return container->createProperty(DataBuffer::Initialized, Particles::AsphericalShapeProperty);
        }
        else if(name.compare(QLatin1String("blockiness1"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            vectorComponent = 0;
            return container->createProperty(DataBuffer::Initialized, Particles::SuperquadricRoundnessProperty);
        }
        else if(name.compare(QLatin1String("blockiness2"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            vectorComponent = 1;
            return container->createProperty(DataBuffer::Initialized, Particles::SuperquadricRoundnessProperty);
        }
    }
    else {
        if(name.compare(QLatin1String("connectivity"), Qt::CaseInsensitive) == 0 || name.compare(QLatin1String("offsets"), Qt::CaseInsensitive) == 0) {
            return nullptr;
        }
        else if(name.compare(QLatin1String("points"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Position"), Property::FloatDefault, numComponents);
        }
        else if(name.compare(QLatin1String("f"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Force"), Property::FloatDefault, numComponents);
        }
        else if(name.compare(QLatin1String("v"), Qt::CaseInsensitive) == 0 && numComponents == 3) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Velocity"), Property::FloatDefault, numComponents);
        }
        else if(name.compare(QLatin1String("atomtype"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Atom Type"), Property::Int32, numComponents);
        }
        else if(name.compare(QLatin1String("nspheres"), Qt::CaseInsensitive) == 0 || name.compare(QLatin1String("type"), Qt::CaseInsensitive) == 0) {
            return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, name, Property::Int32, numComponents);
        }
        else if(name.compare(QLatin1String("id"), Qt::CaseInsensitive) == 0 && numComponents == 1) {
            Property* idProperty = container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, QStringLiteral("Identifier"), Property::IntIdentifier, numComponents);
            if(DataTable* bodiesTable = dynamic_object_cast<DataTable>(container))
                bodiesTable->setX(idProperty);
            return idProperty;
        }
    }

    return container->createProperty(preserveExistingData ? DataBuffer::Initialized : DataBuffer::Uninitialized, Property::makePropertyNameValid(name.toString()), Property::FloatDefault, numComponents);
}

/******************************************************************************
* Helper method that loads the shape of a particle type from an external geometry file.
******************************************************************************/
void ParaViewVTPParticleImporter::FrameLoader::loadParticleShape(ParticleType* particleType)
{
    OVITO_ASSERT(!isUndoRecording());

    // According to Aspherix convention, particle type -1 has no shape.
    if(particleType->numericId() < 0)
        return;

    // Determine the VTM block(s) that contain(s) the type's mesh geometry.
    if(particleType->numericId() >= _particleShapeFiles.size())
        return;

    // Adopt the particle type name from the VTM file.
    particleType->setName(_particleShapeFiles[particleType->numericId()].blockPath[1]);

    // Set radius of particle type to 1.0 to always get correct scaling of shape geometry.
    particleType->setRadius(1.0);

    // Fetch the shape geometry file, then continue in main thread.
    // Note: Invoking a file importer is currently only allowed from the main thread. This may change in the future.
    const QUrl& geometryFileUrl = _particleShapeFiles[particleType->numericId()].location;
    Future<PipelineFlowState> stateFuture =
        Application::instance()
            ->fileManager()
            .fetchUrl(geometryFileUrl)
            .then(ObjectExecutor(particleType), [pipelineNode = pipelineNode()](const FileHandle& fileHandle) {
                // Detect geometry file format and create an importer for it.
                // Note: For loading particle shape geometries we only accept FileSourceImporters.
                OORef<FileSourceImporter> importer =
                    dynamic_object_cast<FileSourceImporter>(FileImporter::autodetectFileFormat(fileHandle));
                if(!importer) return Future<PipelineFlowState>::createImmediateEmpty();

                // Set up a file load request to be passed to the importer.
                LoadOperationRequest loadRequest;
                loadRequest.pipelineNode = pipelineNode;
                loadRequest.fileHandle = fileHandle;
                loadRequest.frame = Frame(fileHandle);
                loadRequest.state = PipelineFlowState(DataOORef<const DataCollection>::create(), PipelineStatus::Success);

                // Let the importer parse the geometry file.
                return importer->loadFrame(loadRequest);
            });

    // Check if the importer has loaded any data.
    PipelineFlowState state = stateFuture.result();
    if(!state || state.status().type() == PipelineStatus::Error)
        return;

    // Look for a triangle mesh or a surface mesh.
    DataOORef<const TriangleMesh> meshObj = state.getObject<TriangleMesh>();
    if(!meshObj) {
        if(const SurfaceMesh* surfaceMesh = state.getObject<SurfaceMesh>()) {
            // Convert surface mesh to triangle mesh.
            DataOORef<TriangleMesh> triMesh = DataOORef<TriangleMesh>::create(ObjectInitializationFlag::DontCreateVisElement);
            SurfaceMeshReadAccess(surfaceMesh).convertToTriMesh(*triMesh, false);
            meshObj = std::move(triMesh);
        }
        else return;
    }
    // Release pipeline state.
    state.reset();

    // Show sharp edges of the mesh.
    DataOORef<TriangleMesh> mutableMeshObj = std::move(meshObj).makeMutable();
    mutableMeshObj->determineEdgeVisibility();

    particleType->setShapeMesh(std::move(mutableMeshObj));
    particleType->setShape(ParticlesVis::Mesh);

    // Aspherix particle geometries seem not to have a consistent face winding order.
    // Need to turn edge highlighting and back-face culling off by default.
    particleType->setShapeBackfaceCullingEnabled(false);
    particleType->setHighlightShapeEdges(false);

    particleType->freezeInitialParameterValues({
        SHADOW_PROPERTY_FIELD(ElementType::name),
        SHADOW_PROPERTY_FIELD(ParticleType::radius),
        SHADOW_PROPERTY_FIELD(ParticleType::shape),
        SHADOW_PROPERTY_FIELD(ParticleType::highlightShapeEdges),
        SHADOW_PROPERTY_FIELD(ParticleType::shapeBackfaceCullingEnabled)});
}

/******************************************************************************
 * Is called once before the datasets referenced in a multi-block VTM file will be loaded.
 ******************************************************************************/
void ParticlesParaViewVTMFileFilter::preprocessDatasets(std::vector<ParaViewVTMBlockInfo>& blockDatasets,
                                                        FileSourceImporter::LoadOperationRequest& request,
                                                        const ParaViewVTMImporter& vtmImporter)
{
    // Resize particles object to zero elements in the existing pipeline state.
    // This is mainly done to remove any existing particles in those trajectory frames where the VTM file is made of empty data blocks.
    for(const DataObject* obj : request.state.data()->objects()) {
        if(const Particles* particles = dynamic_object_cast<Particles>(obj)) {
            Particles* mutableParticles = request.state.mutableData()->makeMutable(particles);
            mutableParticles->setElementCount(0);
            if(mutableParticles->bonds()) mutableParticles->makeBondsMutable()->setElementCount(0);
        }
    }

    // Resize bodies data table to zero elements (clearing its content) in the existing pipeline state.
    // This is mainly done to remove any existing data in those trajectory frames where the VTM file is made of empty data blocks.
    if(DataTable* bodiesTable = request.state.getMutableLeafObject<DataTable>(DataTable::OOClass(), QStringLiteral("bodies"))) {
        bodiesTable->setElementCount(0);
    }

    // The following is specific to VTM files written by the Aspherix code.

    // Remove those datasets from the multi-block structure that represent Aspherix particle shapes (group block "Convex shapes").
    // Keep a list of these removed datasets for later to load them together with the particles dataset.
    // Furthermore, process the "Bodies" section, which stores the per-body data from non-convex particle simulations.
    std::erase_if(blockDatasets, [this](const auto& block) {
        if(block.blockPath.size() == 2 && block.blockPath[0] == QStringLiteral("Convex shapes") && block.pieceIndex == -1) {
            // Store the particle type name and the URL of the type's shape file in the internal list.
            _particleShapeFiles.emplace_back(std::move(block));
            return true;
        }
        else if(block.blockPath.size() == 1 && block.blockPath[0] == QStringLiteral("Bodies")) {
            // Keep track of which .vtp files listed in the VTM file contain per-body data instead of per-particle data.
            _bodiesFiles.push_back(block.location);
        }
        return false;
    });
}

/******************************************************************************
 * Is called before parsing of a dataset reference in a multi-block VTM file begins.
 ******************************************************************************/
void ParticlesParaViewVTMFileFilter::configureImporter(const ParaViewVTMBlockInfo& blockInfo,
                                                       FileSourceImporter::LoadOperationRequest& loadRequest, FileSourceImporter* importer)
{
    if(ParaViewVTPParticleImporter* particleImporter = dynamic_object_cast<ParaViewVTPParticleImporter>(importer)) {

        // Determine whether the VTP file to be loaded by the ParaViewVTPParticleImporter contains per-particle or per-body data.
        particleImporter->setIsBodiesFile(std::any_of(_bodiesFiles.cbegin(), _bodiesFiles.cend(),
            [&](const QUrl& url) { return blockInfo.location == url; }));

        // Pass the list of particle shape files to be loaded to the VTP particle importer, which will take care
        // of loading the geometry file for each particle type.
        if(particleImporter->isBodiesFile() == false)
            particleImporter->setParticleShapeFileList(std::move(_particleShapeFiles));
    }
}

}  // namespace Ovito
