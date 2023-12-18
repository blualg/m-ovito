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

/**
 * \brief File parser for ParaView data files (PVD).
 *
 * See https://www.paraview.org/Wiki/ParaView/Data_formats
 */
class OVITO_MESH_EXPORT ParaViewPVDImporter : public FileSourceImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public FileSourceImporter::OOMetaClass
    {
    public:

        /// Inherit standard constructor from base meta class.
        using FileSourceImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual Ovito::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*.pvd"), tr("ParaView PVD Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(ParaViewPVDImporter, OOMetaClass)
    OVITO_CLASSINFO("DisplayName", "PVD");

public:

    /// \brief Constructor.
    explicit ParaViewPVDImporter(ObjectInitializationFlags flags) : FileSourceImporter(flags) {
        setMultiTimestepFile(true);
    }

    /// Loads the data for the given frame from the external file.
    virtual Future<PipelineFlowState> loadFrame(const LoadOperationRequest& request) override;

    /// Creates an asynchronous task for scanning the input file for animation frames.
    virtual std::shared_ptr<FileSourceImporter::FrameFinder> createFrameFinder(const FileHandle& file) override {
        return std::make_shared<FrameFinder>(file);
    }

private:

    /// The asynchronous task that scans the input file for animation frames.
    class FrameFinder : public FileSourceImporter::FrameFinder
    {
    public:

        /// Inherit constructor from base class.
        using FileSourceImporter::FrameFinder::FrameFinder;

    protected:

        /// Scans the data file and builds a list of source frames.
        virtual void discoverFramesInFile(QVector<FileSourceImporter::Frame>& frames) override;
    };

private:

    /// The delegate importer responsible for parsing the datasets referenced in the PVD file.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<FileSourceImporter>, childImporter, PROPERTY_FIELD_NO_UNDO | PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_OPEN_SUBEDITOR);
};

}   // End of namespace
