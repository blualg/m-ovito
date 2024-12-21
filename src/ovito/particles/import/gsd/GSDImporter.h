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

#pragma once


#include <ovito/particles/Particles.h>
#include <ovito/particles/import/ParticleImporter.h>
#include <ovito/particles/objects/ParticlesVis.h>
#include <ovito/core/dataset/data/mesh/TriangleMesh.h>
#include <ovito/core/dataset/DataSetContainer.h>

#include <QReadWriteLock>

namespace Ovito {

class GSDFile;  // Defined in GSDFile.h

/**
 * \brief File parser for GSD (General Simulation Data) files written by the HOOMD simulation code.
 */
class OVITO_PARTICLES_EXPORT GSDImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("GSD/HOOMD Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(GSDImporter, OOMetaClass)

public:

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags) {
        ParticleImporter::initializeObject(flags);
        setMultiTimestepFile(true);
    }

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_shared<FrameLoader>(request, this, std::max(roundingResolution(), 1));
    }

    /// Creates an asynchronous frame discovery object that scans the input file for contained animation frames.
    virtual std::shared_ptr<FileSourceImporter::FrameFinder> createFrameFinder(const FileHandle& file) override {
        return std::make_shared<FrameFinder>(file);
    }

    /// Stores the particle shape geometry generated from a JSON string in the internal cache.
    void storeParticleShapeInCache(const QByteArray& jsonString, const DataOORef<const TriangleMesh>& mesh);

    /// Looks up a particle shape geometry in the internal cache that was previously
    /// generated from a JSON string.
    DataOORef<const TriangleMesh> lookupParticleShapeInCache(const QByteArray& jsonString) const;

protected:

    /// \brief Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// A lookup map that holds geometries that have been generated from JSON strings.
    QHash<QByteArray, DataOORef<const TriangleMesh>> _particleShapeCache;

    /// Synchronization object for multi-threaded access to the particle shape cache.
    mutable QReadWriteLock _cacheSynchronization;

    /// Controls the tessellation resolution for rounded corners and edges.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{4}, roundingResolution, setRoundingResolution, PROPERTY_FIELD_MEMORIZE);

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, GSDImporter* importer, int roundingResolution)
            : ParticleImporter::FrameLoader(request), _importer(importer), _roundingResolution(roundingResolution) {}

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// Reads the values of a particle or bond property from the GSD file.
        Property* readOptionalProperty(GSDFile& gsd, const char* chunkName, uint64_t frameNumber, int propertyType, PropertyContainer* container, const void* defaultValue, size_t defaultValueSize);

        /// Parse the JSON string containing a particle shape definition.
        void parseParticleShape(int typeId, const QByteArray& shapeSpecString);

        /// Parsing routine for 'Sphere' particle shape definitions.
        void parseSphereShape(int typeId, QJsonObject definition);

        /// Parsing routine for 'Ellipsoid' particle shape definitions.
        void parseEllipsoidShape(int typeId, QJsonObject definition);

        /// Parsing routine for 'Polygon' particle shape definitions.
        void parsePolygonShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString);

        /// Parsing routine for 'ConvexPolyhedron' particle shape definitions.
        void parseConvexPolyhedronShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString);

        /// Parsing routine for 'Mesh' particle shape definitions.
        void parseMeshShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString);

        /// Parsing routine for 'SphereUnion' particle shape definitions.
        void parseSphereUnionShape(int typeId, QJsonObject definition, const QByteArray& shapeSpecString);

        /// Sets the shape of a particle type.
        void setParticleTypeShape(int typeId, ParticlesVis::ParticleShape shape, DataOORef<const TriangleMesh> shapeMesh = {}, FloatType radius = 1);

    private:

        OORef<GSDImporter> _importer;
        int _roundingResolution;
    };

    /// The format-specific task object that is responsible for scanning the input file for animation frames.
    class FrameFinder : public FileSourceImporter::FrameFinder
    {
    public:

        /// Inherit constructor from base class.
        using FileSourceImporter::FrameFinder::FrameFinder;

    protected:

        /// Scans the data file and builds a list of source frames.
        virtual void discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames) override;
    };
};

}   // End of namespace
