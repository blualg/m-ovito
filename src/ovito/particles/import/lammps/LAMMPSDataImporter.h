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
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/DataSetContainer.h>

namespace Ovito {

/**
 * \brief File parser for LAMMPS data files.
 */
class OVITO_PARTICLES_EXPORT LAMMPSDataImporter : public ParticleImporter
{
    /// Defines a metaclass specialization for this importer type.
    class OOMetaClass : public ParticleImporter::OOMetaClass
    {
    public:
        /// Inherit standard constructor from base meta class.
        using ParticleImporter::OOMetaClass::OOMetaClass;

        /// Returns the list of file formats that can be read by this importer class.
        virtual std::span<const SupportedFormat> supportedFormats() const override {
            static const SupportedFormat formats[] = {{ QStringLiteral("*"), tr("LAMMPS Data Files") }};
            return formats;
        }

        /// Checks if the given file has format that can be read by this importer.
        virtual bool checkFileFormat(const FileHandle& file) const override;
    };

    OVITO_CLASS_META(LAMMPSDataImporter, OOMetaClass)

public:

    /// \brief The LAMMPS atom_style used by the data file.
    enum LAMMPSAtomStyle {
        AtomStyle_Unknown,  //< Special value indicating that the atom_style could not be automatically detected and needs to be specified by the user.
        AtomStyle_Angle,
        AtomStyle_Atomic,
        AtomStyle_Body,
        AtomStyle_Bond,
        AtomStyle_Charge,
        AtomStyle_Dipole,
        AtomStyle_DPD,
        AtomStyle_EDPD,
        AtomStyle_MDPD,
        AtomStyle_Electron,
        AtomStyle_Ellipsoid,
        AtomStyle_Full,
        AtomStyle_Line,
        AtomStyle_Meso,
        AtomStyle_Molecular,
        AtomStyle_Peri,
        AtomStyle_SMD,
        AtomStyle_Sphere,
        AtomStyle_Template,
        AtomStyle_Tri,
        AtomStyle_Wavepacket,
        AtomStyle_Hybrid,

        AtomStyle_COUNT, // End-of-list marker
    };
    Q_ENUM(LAMMPSAtomStyle);

public:

    /// \brief Constructs a new instance of this class.
    explicit LAMMPSDataImporter(ObjectInitializationFlags flags) : ParticleImporter(flags), _atomStyle(AtomStyle_Unknown) {}

    /// Creates an asynchronous loader object that loads the data for the given frame from the external file.
    virtual FileSourceImporter::FrameLoaderPtr createFrameLoader(const LoadOperationRequest& request) override {
        activateCLocale();
        return std::make_shared<FrameLoader>(request, sortParticles(), atomStyle(), atomSubStyles());
    }

    struct LAMMPSAtomStyleHints {
        LAMMPSAtomStyle atomStyle = AtomStyle_Unknown;
        std::vector<LAMMPSAtomStyle> atomSubStyles;
        int atomDataColumnCount = 0;
    };

    /// Inspects the header of the given file and returns the detected LAMMPS atom style.
    Future<LAMMPSAtomStyleHints> inspectFileHeader(const Frame& frame);

    /// Returns the name string of the given LAMMPS atom style.
    static QString atomStyleName(LAMMPSAtomStyle atomStyle);

    /// Parses a hint string for the LAMMPS atom style.
    static LAMMPSAtomStyle parseAtomStyleHint(const QString& atomStyleHint);

    /// Sets up the mapping of data file columns in the 'Atoms' section to internal particle properties based on the selected LAMMPS atom style.
    static ParticleInputColumnMapping createAtomsColumnMapping(LAMMPSAtomStyle atomStyle, const std::vector<LAMMPSAtomStyle>& atomSubStyles, int dataColumnCount = 0);

    /// Sets up the mapping of data file columns in the 'Velocities' section to internal particle properties based on the selected LAMMPS atom style.
    static ParticleInputColumnMapping createVelocitiesColumnMapping(LAMMPSAtomStyle atomStyle, const std::vector<LAMMPSAtomStyle>& atomSubStyles);

private:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class FrameLoader : public ParticleImporter::FrameLoader
    {
    public:

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool sortParticles, LAMMPSAtomStyle atomStyle, std::vector<LAMMPSAtomStyle> atomSubStyles)
            : ParticleImporter::FrameLoader(request),
                _atomStyleHints{atomStyle, std::move(atomSubStyles)},
                _sortParticles(sortParticles) {}

    private:

        /// Reads the frame data from the external file.
        virtual void loadFile() override;

        /// The LAMMPS atom style of the file.
        LAMMPSAtomStyleHints _atomStyleHints;

        bool _sortParticles;
    };

    /// Detects or verifies the LAMMPS atom style used by the data file.
    static void detectAtomStyle(const char* firstLine, const QByteArray& keywordLine, LAMMPSAtomStyleHints& atomStyleHints);

    /// The LAMMPS atom style used by the data format.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(LAMMPSAtomStyle, atomStyle, setAtomStyle);

    /// The list of sub-styles if the hybrid atom style is used.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(std::vector<LAMMPSAtomStyle>, atomSubStyles, setAtomSubStyles);
};

}   // End of namespace
