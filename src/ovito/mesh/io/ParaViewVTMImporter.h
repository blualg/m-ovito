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


#include <ovito/mesh/Mesh.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

class ParaViewVTMImporter; // defined below

/**
 * \brief Describes a single data file referenced by a VTM file.
 */
struct OVITO_MESH_EXPORT ParaViewVTMBlockInfo
{
    /// The named path to the block in the hierarchy of nested data blocks within the VTM file.
    QStringList blockPath;

    /// The URL of the referenced data file (may be empty).
    QUrl location;

    /// The index of this partial dataset if it is part of a piece-wise (parallel) dataset structure; otherwise -1.
    int pieceIndex = -1;

    /// The total number of partial datasets that are part of the same parallel dataset.
    int pieceCount = 0;
};

/**
 * \brief Abstract base class for filters that can customize the loading of VTM files.
 */
class OVITO_MESH_EXPORT ParaViewVTMFileFilter : public OvitoObject
{
    OVITO_CLASS(ParaViewVTMFileFilter)

public:

    /// \brief Is called once before the datasets referenced in a multi-block VTM file will be loaded.
    virtual void preprocessDatasets(std::vector<ParaViewVTMBlockInfo>& blockDatasets, FileSourceImporter::LoadOperationRequest& request, const ParaViewVTMImporter& vtmImporter) {}

    /// \brief Is called for every dataset referenced in a multi-block VTM file.
    virtual Future<> loadDataset(const ParaViewVTMBlockInfo& blockInfo, const FileHandle& referencedFile, const FileSourceImporter::LoadOperationRequest& loadRequest) { return {}; }

    /// \brief Is called before parsing of a dataset referenced in a multi-block VTM file begins.
    virtual void configureImporter(const ParaViewVTMBlockInfo& blockInfo, FileSourceImporter::LoadOperationRequest& loadRequest, FileSourceImporter* importer) {}

    /// \brief Is called after all datasets referenced in a multi-block VTM file have been loaded.
    virtual void postprocessDatasets(FileSourceImporter::LoadOperationRequest& request) {}
};

/**
 * \brief File parser for ParaView Multi-Block files (VTM).
 */
class OVITO_MESH_EXPORT ParaViewVTMImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:

        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.vtm"), tr("ParaView Multi-Block Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewVTMImporter, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "VTM");

public:

    /// Constructor.
    using FileSourceImporter::FileSourceImporter;

    /// Loads the data for the given frame from the external file.
    virtual Future<PipelineFlowState> loadFrame(const LoadOperationRequest& request) override;

protected:

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

private:

    /// Parses the given VTM file and returns the list of referenced data files.
    static std::vector<ParaViewVTMBlockInfo> loadVTMFile(const FileHandle& fileHandle);

private:

    /// Controls whether all surface meshes are merged into a single mesh during import.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool, uniteMeshes, setUniteMeshes, PROPERTY_FIELD_MEMORIZE);
};

}   // End of namespace
