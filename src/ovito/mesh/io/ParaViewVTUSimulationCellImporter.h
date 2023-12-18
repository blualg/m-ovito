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
#include <ovito/stdobj/io/StandardFrameLoader.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

#include <QXmlStreamReader>

namespace Ovito {

/**
 * \brief File parser for reading the simulation cell geometry from a ParaView VTU (UnstructuredGrid) file as written by the Aspherix simulation code.
 */
class OVITO_MESH_EXPORT ParaViewVTUSimulationCellImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:

        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewVTUSimulationCellImporter, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "VTU");

public:

    /// Constructor.
    using FileSourceImporter::FileSourceImporter;

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        return std::make_shared<FrameLoader>(request);
    }

private:

    /// The format-specific task object that is responsible for reading an input file in a separate thread.
    class FrameLoader : public StandardFrameLoader
    {
    public:

        /// Inherit constructor from base class.
        using StandardFrameLoader::StandardFrameLoader;

    protected:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;
    };
};

}   // End of namespace
