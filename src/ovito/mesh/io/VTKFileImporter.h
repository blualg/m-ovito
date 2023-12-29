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


#include <ovito/mesh/Mesh.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for VTK files containing triangle mesh data.
 */
class OVITO_MESH_EXPORT VTKFileImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.vtk"), tr("VTK Legacy Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;

        /// Returns whether this importer class supports importing data of the given type.
        virtual bool importsDataType(const DataObject::OOMetaClass& dataObjectType) const override;
    };

    OVITO_CLASS_META(VTKFileImporter, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "VTK");

public:

    /// Constructor.
    using FileSourceImporter::FileSourceImporter;

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        activateCLocale();
        return std::make_shared<FrameLoader>(request);
    }

protected:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public FileSourceImporter::FrameLoader
    {
    public:

        /// Inherit constructor from base class.
        using FileSourceImporter::FrameLoader::FrameLoader;

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// Reads from the input stream and throws an exception if the given keyword is not present.
        static void expectKeyword(CompressedTextReader& stream, const char* keyword);
    };
};

}   // End of namespace
