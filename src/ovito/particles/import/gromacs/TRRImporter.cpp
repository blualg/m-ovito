////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include "TRRImporter.h"

#include <xdrfile/xdrfile.h>
#include <xdrfile/xdrfile_trr.h>

#include <algorithm>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(TRRImporter);
OVITO_CLASSINFO(TRRImporter, "DisplayName", "TRR");

class TRRFile
{
public:
    struct Frame {
        int step;
        float time;
        float lambda;
        int hasProp;
        Matrix_3<float> cell;
        std::vector<Point3F> xyz;
        std::vector<Vector3F> velocity;
        std::vector<Vector3F> force;

        // Adapted from #define flags in xrdfile_trr.h
        enum class Property : uint8_t
        {
            XYZ = 1,
            VELOCITY = 2,
            FORCE = 4
        };

        void resize(size_t n)
        {
            xyz.resize(n);
            velocity.resize(n);
            force.resize(n);
        }
    };

    ~TRRFile() { close(); }

    void open(const char* filename)
    {
        close();
        int returnCode = read_trr_natoms(filename, &_numAtoms);
        if(returnCode != exdrOK || _numAtoms <= 0)
            throw Exception(TRRImporter::tr("Error opening TRR file (error code %1).").arg(returnCode));
        _file = xdrfile_open(filename, "r");
        if(!_file) throw Exception(TRRImporter::tr("Error opening TRR file."));
        _eof = false;
    }

    void close()
    {
        if(_file) {
            if(xdrfile_close(_file) != exdrOK) qWarning() << "TRRImporter: Failure reported by xdrfile_close()";
            _file = nullptr;
        }
    }

    Frame read()
    {
        OVITO_ASSERT(_file);
        OVITO_ASSERT(!_eof);
        Frame frame;
        frame.resize(_numAtoms);
        int returnCode = read_trr(_file, _numAtoms, &frame.step, &frame.time, &frame.lambda, reinterpret_cast<matrix&>(frame.cell),
                                  reinterpret_cast<rvec*>(frame.xyz.data()), reinterpret_cast<rvec*>(frame.velocity.data()),
                                  reinterpret_cast<rvec*>(frame.force.data()), &frame.hasProp);

        if(returnCode != exdrOK && returnCode != exdrENDOFFILE)
            throw Exception(TRRImporter::tr("Error reading TRR file (code %1).").arg(returnCode));
        if(returnCode == exdrENDOFFILE) _eof = true;
        return frame;
    }

    void seek(qint64 offset)
    {
        int returnCode = xdr_seek(_file, offset, SEEK_SET);
        if(returnCode != exdrOK) throw Exception(TRRImporter::tr("Error seeking in TRR file (code %1).").arg(returnCode));
    }

    qint64 byteOffset() const
    {
        OVITO_ASSERT(_file);
        return xdr_tell(_file);
    }

    bool eof() const { return _eof; }

private:
    XDRFILE* _file = nullptr;
    int _numAtoms = 0;
    bool _eof = false;
};

/******************************************************************************
 * Checks if the given file has format that can be read by this importer.
 ******************************************************************************/
bool TRRImporter::OOMetaClass::checkFileFormat(const FileHandle& file) const
{
    return file.sourceUrl().fileName().endsWith(QStringLiteral(".trr"), Qt::CaseInsensitive);
}

/******************************************************************************
 * Scans the data file and builds a list of source frames.
 ******************************************************************************/
void TRRImporter::discoverFramesInFile(const FileHandle& fileHandle, QVector<FileSourceImporter::Frame>& frames) const
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Scanning file %1").arg(fileHandle.toString()));
    progress.setMaximum(QFileInfo(fileHandle.localFilePath()).size());

    // Open TRR file for reading.
    TRRFile file;
    file.open(QFile::encodeName(QDir::toNativeSeparators(fileHandle.localFilePath())).constData());

    Frame frame(fileHandle);
    while(!file.eof() && !this_task::isCanceled()) {
        frame.byteOffset = file.byteOffset();

        // Update progress bar and check for user cancellation.
        progress.setValue(frame.byteOffset);

        // Parse trajectory frame.
        TRRFile::Frame trrFrame = file.read();
        if(file.eof()) break;

        // Create a new record for the timestep.
        frame.label = tr("Timestep %1").arg(trrFrame.step);
        frames.push_back(frame);
    }
}

/******************************************************************************
 * Parses the given input file.
 ******************************************************************************/
void TRRImporter::FrameLoader::loadFile()
{
    TaskProgress progress(this_task::ui());
    progress.setText(tr("Reading TRR file %1").arg(fileHandle().toString()));

    // Open TRR file for reading.
    TRRFile file;
    file.open(QFile::encodeName(QDir::toNativeSeparators(fileHandle().localFilePath())).constData());

    // Seek to byte offset of requested trajectory frame.
    if(frame().byteOffset != 0) file.seek(frame().byteOffset);

    // Read trajectory frame data.
    TRRFile::Frame trrFrame = file.read();

    // Transfer atomic coordinates to property storage. Also convert from nanometer units to angstroms.
    size_t numParticles = trrFrame.xyz.size();
    setParticleCount(numParticles);

    // Check if property has been read from the file
    if(((uint8_t)trrFrame.hasProp & (uint8_t)TRRFile::Frame::Property::XYZ) == (uint8_t)TRRFile::Frame::Property::XYZ) {
        BufferWriteAccess<Point3, access_mode::discard_write> posProperty = particles()->createProperty(Particles::PositionProperty);
        // Convert nm to angstrom
        std::ranges::transform(trrFrame.xyz, posProperty.begin(), [](const Point3F& p) { return (p * 10.0f).toDataType<FloatType>(); });
        posProperty.reset();
    }

    if(((uint8_t)trrFrame.hasProp & (uint8_t)TRRFile::Frame::Property::VELOCITY) == (uint8_t)TRRFile::Frame::Property::VELOCITY) {
        BufferWriteAccess<Vector3, access_mode::discard_write> velProperty = particles()->createProperty(Particles::VelocityProperty);
        // convert nm/ps to angstrom/ps
        std::ranges::transform(trrFrame.velocity, velProperty.begin(),
                               [](const Vector3F& p) { return (p * 10.0f).toDataType<FloatType>(); });
        velProperty.reset();
    }

    if(((uint8_t)trrFrame.hasProp & (uint8_t)TRRFile::Frame::Property::FORCE) == (uint8_t)TRRFile::Frame::Property::FORCE) {
        BufferWriteAccess<Vector3, access_mode::discard_write> forceProperty = particles()->createProperty(Particles::ForceProperty);
        // 1 kJ/mol = 1.03642697E-02 eV (from https://wild.life.nctu.edu.tw/class/common/energy-conversion-table-in-E-format.html)
        constexpr float kJ_Mol_eV = 1.03642697E-02;
        constexpr float kJ_Mol_nm_to_eV_A = kJ_Mol_eV * 0.1f;
        // convert kJ/mol/nm to eV/A
        std::ranges::transform(trrFrame.force, forceProperty.begin(),
                               [](const Vector3F& p) { return (p * kJ_Mol_nm_to_eV_A).toDataType<FloatType>(); });
        forceProperty.reset();
    }

    // Convert cell vectors from nanometers to angstroms.
    simulationCell()->setCellMatrix(AffineTransformation((trrFrame.cell * 10.0f).toDataType<FloatType>()));

    state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(trrFrame.step), pipelineNode());
    state().setAttribute(QStringLiteral("Time"), QVariant::fromValue((FloatType)trrFrame.time), pipelineNode());
    state().setAttribute(QStringLiteral("Lambda"), QVariant::fromValue((FloatType)trrFrame.lambda), pipelineNode());

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}  // namespace Ovito
