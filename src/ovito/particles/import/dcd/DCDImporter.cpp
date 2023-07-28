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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/ParticlesObject.h>
#include <ovito/particles/objects/ParticleType.h>
#include <ovito/stdobj/simcell/SimulationCellObject.h>
#include "DCDImporter.h"

namespace Ovito::Particles {

IMPLEMENT_OVITO_CLASS(DCDImporter);

static void swap_endianess(qint32* data, size_t ndata = 1)
{
    for(size_t i = 0; i < ndata; i++) {
        qint32* N = data + i;
        *N = (((*N >> 24) & 0xff) | ((*N & 0xff) << 24) | ((*N >> 8) & 0xff00) | ((*N & 0xff00) << 8));
    }
}

static void swap_endianess(qint64* data, size_t ndata = 1)
{
    for(size_t i = 0; i < ndata; i++) {
        int* N = reinterpret_cast<qint32*>(data + i);
        int t0 = N[0];
        t0 = (((t0 >> 24) & 0xff) | ((t0 & 0xff) << 24) | ((t0 >> 8) & 0xff00) | ((t0 & 0xff00) << 8));
        int t1 = N[1];
        t1 = (((t1 >> 24) & 0xff) | ((t1 & 0xff) << 24) | ((t1 >> 8) & 0xff00) | ((t1 & 0xff00) << 8));
        N[0] = t1;
        N[1] = t0;
    }
}

static void swap_endianess(float* data, size_t ndata = 1)
{
    OVITO_STATIC_ASSERT(sizeof(float) == sizeof(qint32));
    swap_endianess(reinterpret_cast<qint32*>(data), ndata);
}

static void swap_endianess(double* data, size_t ndata = 1)
{
    OVITO_STATIC_ASSERT(sizeof(double) == sizeof(qint64));
    swap_endianess(reinterpret_cast<qint64*>(data), ndata);
}

static void read_int(QIODevice& device, int& value, bool reverseEndianess)
{
    if(device.read(reinterpret_cast<char *>(&value), sizeof(value)) != sizeof(value)) {
        if(device.atEnd())
            throw Exception(DCDImporter::tr("File I/O error: unexpected end of DCD file"));
        else
            throw Exception(DCDImporter::tr("File I/O error: %1").arg(device.errorString()));
    }
    if(reverseEndianess)
        swap_endianess(&value);
}

struct DCDHeader
{
    enum CharmmFlags {
        DCD_IS_CHARMM = 0x01,
        DCD_HAS_4DIMS = 0x02,
        DCD_HAS_EXTRA_BLOCK = 0x04
    };

    int natoms = 0;
    int nsets = 0;
    int istart = 0;
    int nsavc = 0;
    double delta = 0.0;
    int nfixed = 0;
    bool reverseEndian = false;
    int charmm = 0;
    QByteArray remarks;
    std::vector<int> freeindices;

    void parse(QIODevice& device)
    {
        // First thing in the file should be an 84
        int input_integer;
        read_int(device, input_integer, false);

        // Check magic number in file header and determine byte order
        if(input_integer != 84) {
            // check to see if its merely reversed endianism
            // rather than a totally incorrect file magic number
            swap_endianess(&input_integer);

            if(input_integer == 84) {
                reverseEndian = true;
            }
            else {
                // not simply reversed endianism, but something else.
                throw Exception(DCDImporter::tr("File I/O error: not a DCD file"));
            }
        }

        // Read entire header for random access.
        char hdrbuf[84];
        if(device.read(hdrbuf, sizeof(hdrbuf)) != sizeof(hdrbuf)) {
            if(device.atEnd())
                throw Exception(DCDImporter::tr("File I/O error: unexpected end of DCD file"));
            else
                throw Exception(DCDImporter::tr("File I/O error: %1").arg(device.errorString()));
        }

        // Check for the ID string "COORD"
        if(hdrbuf[0] != 'C' || hdrbuf[1] != 'O' || hdrbuf[2] != 'R' || hdrbuf[3] != 'D')
            throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file"));

        // CHARMm-genereate DCD files set the last integer in the
        // header, which is unused by X-PLOR, to its version number.
        // Checking if this is nonzero tells us this is a CHARMm file
        // and to look for other CHARMm flags.
        if(*reinterpret_cast<const int*>(hdrbuf + 80) != 0) {
            charmm = DCD_IS_CHARMM;
            if(*reinterpret_cast<const int*>(hdrbuf + 44) != 0)
                charmm |= DCD_HAS_EXTRA_BLOCK;
            if(*reinterpret_cast<const int*>(hdrbuf + 48) == 1)
                charmm |= DCD_HAS_4DIMS;
        }

        // Store the number of sets of coordinates
        nsets = *reinterpret_cast<const int*>(hdrbuf + 4);
        if(reverseEndian) swap_endianess(&nsets);

        // Store the starting timestep
        istart = *reinterpret_cast<const int*>(hdrbuf + 8);
        if(reverseEndian) swap_endianess(&istart);

        // Store the number of timesteps between dcd saves
        nsavc = *reinterpret_cast<const int*>(hdrbuf + 12);
        if(reverseEndian) swap_endianess(&nsavc);

        // Store the number of fixed atoms
        nfixed = *reinterpret_cast<const int*>(hdrbuf + 36);
        if(reverseEndian) swap_endianess(&nfixed);

        // Read in the timestep, DELTA
        // Note: DELTA is stored as a double with X-PLOR but as a float with CHARMm
        if(charmm & DCD_IS_CHARMM) {
            float ftmp = *reinterpret_cast<const float*>(hdrbuf + 40);
            if(reverseEndian) swap_endianess(&ftmp);
            delta = ftmp;
        }
        else {
            delta = *reinterpret_cast<const double*>(hdrbuf + 40);
            if(reverseEndian) swap_endianess(&delta);
        }

        // Get the end size of the first block
        read_int(device, input_integer, reverseEndian);
        if(input_integer != 84)
            throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file"));

        // Read in the size of the next block
        read_int(device, input_integer, reverseEndian);
        if(((input_integer - 4) % 80) != 0)
            throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file"));

        // Read NTITLE, the number of 80 character title strings there are
        int NTITLE;
        read_int(device, NTITLE, reverseEndian);
        remarks = device.read(NTITLE * 80);
        if(remarks.size() != NTITLE * 80)
            throw Exception(DCDImporter::tr("File I/O error: %1").arg(device.errorString()));

        // Get the ending size for this block
        read_int(device, input_integer, reverseEndian);

        // Read in an integer '4'
        read_int(device, input_integer, reverseEndian);
        if(input_integer != 4)
            throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file"));

        // Read in the number of atoms
        read_int(device, natoms, reverseEndian);

        // Read in an integer '4'
        read_int(device, input_integer, reverseEndian);
        if(input_integer != 4)
            throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file"));

        if(nfixed != 0) {
            freeindices.resize(natoms - nfixed);

            // Read in index array size
            read_int(device, input_integer, reverseEndian);
            if(input_integer != freeindices.size() * 4)
                throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file"));

            if(device.read(reinterpret_cast<char*>(freeindices.data()), freeindices.size() * sizeof(int)) != freeindices.size() * sizeof(int))
                throw Exception(DCDImporter::tr("File I/O error: %1").arg(device.errorString()));
            if(reverseEndian)
                swap_endianess(freeindices.data(), freeindices.size());

            read_int(device, input_integer, reverseEndian);
            if(input_integer != freeindices.size() * 4)
                throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file"));
        }
    }

    void read_charmm_extrablock(QIODevice& device, std::array<double,6>& unitcell)
    {
        if((charmm & DCD_IS_CHARMM) && (charmm & DCD_HAS_EXTRA_BLOCK)) {
            // Leading integer must be 48
            int input_integer;
            read_int(device, input_integer, reverseEndian);
            if(input_integer == 48) {
                if(device.read(reinterpret_cast<char*>(unitcell.data()), 6 * sizeof(double)) != 6 * sizeof(double)) {
                    if(device.atEnd())
                        throw Exception(DCDImporter::tr("File I/O error: unexpected end of DCD file"));
                    else
                        throw Exception(DCDImporter::tr("File I/O error: %1").arg(device.errorString()));
                }
                if(reverseEndian)
                    swap_endianess(unitcell.data(), unitcell.size());
            }
            else {
                // Unrecognized block, just skip it
                if(device.skip(input_integer) != input_integer)
                    throw Exception(DCDImporter::tr("File I/O error: %1").arg(device.errorString()));
            }
            read_int(device, input_integer, reverseEndian);
        }
    }
};

/******************************************************************************
 * Checks if the given file has format that can be read by this importer.
 ******************************************************************************/
bool DCDImporter::OOMetaClass::checkFileFormat(const FileHandle &file) const
{
    std::unique_ptr<QIODevice> device = file.createIODevice();
    if(!device->open(QIODevice::ReadOnly))
        throw Exception(tr("Failed to open file: %1").arg(device->errorString()));

    DCDHeader().parse(*device);

    return true;
}

/******************************************************************************
 * Scans the data file and builds a list of source frames.
 ******************************************************************************/
void DCDImporter::FrameFinder::discoverFramesInFile(QVector<FileSourceImporter::Frame> &frames)
{
    setProgressText(tr("Scanning file %1").arg(fileHandle().toString()));

    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly))
        throw Exception(tr("Failed to open file: %1").arg(device->errorString()));

    DCDHeader header;
    header.parse(*device);

    qint64 extrablocksize = header.charmm & DCDHeader::DCD_HAS_EXTRA_BLOCK ? 48 + 8 : 0;
    qint64 ndims = header.charmm & DCDHeader::DCD_HAS_4DIMS ? 4 : 3;
    qint64 firstframesize = (header.natoms + 2) * ndims * sizeof(float) + extrablocksize;
    qint64 framesize = (header.natoms - header.nfixed + 2) * ndims * sizeof(float) + extrablocksize;
    qint64 headersize = device->pos();

    // Compute number of trajectory frames based on file size.
    qint64 filesize = device->size() - headersize - firstframesize;
    int nframes = 0;
    if(filesize >= 0)
        nframes = std::min(header.nsets, 1 + int(filesize / framesize));

    Frame frame(fileHandle());
    for(int i = 0; i < nframes; i++) {
        frame.byteOffset = i;
        frame.label = tr("Timestep %1").arg(header.istart + i * header.nsavc);
        frames.push_back(frame);
    }
}

/******************************************************************************
* Parses the given input file.
******************************************************************************/
void DCDImporter::FrameLoader::loadFile()
{
    // Open file for reading.
    setProgressText(tr("Reading DCD file %1").arg(fileHandle().toString()));

    std::unique_ptr<QIODevice> device = fileHandle().createIODevice();
    if(!device->open(QIODevice::ReadOnly))
        throw Exception(tr("Failed to open DCD file: %1").arg(device->errorString()));

    DCDHeader header;
    header.parse(*device);
    qint64 headersize = device->pos();

    // Simulation cell info.
    std::array<double,6> unitcell;
    unitcell[0] = unitcell[2] = unitcell[5] = 0.0;
    unitcell[1] = unitcell[3] = unitcell[4] = 90.0;

    // Coordinates array.
    std::vector<float> coords[3] = {
        std::vector<float>(header.natoms),
        std::vector<float>(header.natoms),
        std::vector<float>(header.natoms)
    };

    // Compute byte offset at which the requested frame starts.
    qint64 extrablocksize = header.charmm & DCDHeader::DCD_HAS_EXTRA_BLOCK ? 48 + 8 : 0;
    qint64 ndims = header.charmm & DCDHeader::DCD_HAS_4DIMS ? 4 : 3;
    qint64 firstframesize = (header.natoms + 2) * ndims * sizeof(float) + extrablocksize;
    qint64 framesize = (header.natoms - header.nfixed + 2) * ndims * sizeof(float) + extrablocksize;
    qint64 byteoffset = headersize + firstframesize + framesize * (frame().byteOffset - 1);

    // If there are no fixed atoms, directly read the requested frame.
    // Otherwise, first read the first frame, which includes the coordinates of fixed atoms.
    if(header.nfixed == 0) {
        if(!device->seek(byteoffset))
            throw Exception(tr("Failed to read DCD file: jumping to byte offset of frame %1 failed.").arg(frame().byteOffset));
    }

    // Read the charmm periodic cell information.
    header.read_charmm_extrablock(*device, unitcell);

    // Read xyz coordinates of all atoms (including fixed ones).
    for(int dim = 0; dim < 3; dim++) {
        // Read format integer.
        int byte_count;
        read_int(*device, byte_count, header.reverseEndian);
        if(byte_count != sizeof(float) * header.natoms)
            throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file (byte_count != 4 * natoms)"));

        if(device->read(reinterpret_cast<char*>(coords[dim].data()), coords[dim].size() * sizeof(float)) != coords[dim].size() * sizeof(float)) {
            if(device->atEnd())
                throw Exception(DCDImporter::tr("File I/O error: unexpected end of DCD file"));
            else
                throw Exception(DCDImporter::tr("File I/O error: %1").arg(device->errorString()));
        }
        if(header.reverseEndian)
            swap_endianess(coords[dim].data(), coords[dim].size());

        read_int(*device, byte_count, header.reverseEndian);
    }

    // Read free atom coordinates of requested frame (unless it's the first one or there are no fixed atoms).
    if(frame().byteOffset != 0 && header.nfixed != 0) {

        // Seek to byte offset at which the requested frame starts.
        if(!device->seek(byteoffset))
            throw Exception(tr("Failed to read DCD file: jumping to byte offset of frame %1 failed.").arg(frame().byteOffset));

        // Read the charmm periodic cell information.
        header.read_charmm_extrablock(*device, unitcell);

        // Coordinates array for free atoms.
        std::vector<float> free_coords(header.natoms - header.nfixed);

        // Read xyz coordinates of free atoms.
        for(int dim = 0; dim < 3; dim++) {
            // Read format integer.
            int byte_count;
            read_int(*device, byte_count, header.reverseEndian);
            if(byte_count != free_coords.size() * sizeof(float))
                throw Exception(DCDImporter::tr("File I/O error: not a valid DCD file (byte_count mismatch)"));

            if(device->read(reinterpret_cast<char*>(free_coords.data()), free_coords.size() * sizeof(float)) != free_coords.size() * sizeof(float)) {
                if(device->atEnd())
                    throw Exception(DCDImporter::tr("File I/O error: unexpected end of DCD file"));
                else
                    throw Exception(DCDImporter::tr("File I/O error: %1").arg(device->errorString()));
            }
            if(header.reverseEndian)
                swap_endianess(free_coords.data(), free_coords.size());

            read_int(*device, byte_count, header.reverseEndian);

            float* pos = coords[dim].data();
            auto iter = header.freeindices.cbegin();
            for(float c : free_coords)
                pos[(*iter++) - 1] = c;
        }
    }

    // Allocate position particle property.
    setParticleCount(header.natoms);
    BufferWriteAccess<Point3, access_mode::discard_write> posProperty = particles()->createProperty(ParticlesObject::PositionProperty);

    // Copy coordinates to property array.
    auto iter_x = coords[0].cbegin();
    auto iter_y = coords[1].cbegin();
    auto iter_z = coords[2].cbegin();
    for(Point3& p : posProperty) {
        p = Point3(*iter_x++, *iter_y++, *iter_z++);
    }

    if((header.charmm & DCDHeader::DCD_IS_CHARMM) && (header.charmm & DCDHeader::DCD_HAS_EXTRA_BLOCK)) {
        AffineTransformation cell = AffineTransformation::Identity();
        double cos_alpha, cos_beta, cos_gamma;
        double sin_alpha, sin_beta, sin_gamma;
        if(unitcell[1] >= -1.0 && unitcell[1] <= 1.0 && unitcell[3] >= -1.0 && unitcell[3] <= 1.0 && unitcell[4] >= -1.0 && unitcell[4] <= 1.0) {
            // The file was generated by Charmm, or by NAMD > 2.5, with the angle
            // cosines of the periodic cell angles written to the DCD file.
            // This formulation improves rounding behavior for orthogonal cells
            // so that the angles end up at precisely 90 degrees, unlike acos().
            // unitcell = [A, gamma, B, beta, alpha, C]
            cos_alpha = unitcell[4];
            cos_beta = unitcell[3];
            cos_gamma = unitcell[1];
            sin_gamma = std::sqrt(1.0 - cos_gamma * cos_gamma);
        }
        else if(unitcell[0] < 0.0 || unitcell[1] < 0.0 || unitcell[2] < 0.0 || unitcell[3] < 0.0 || unitcell[4] < 0.0 || unitcell[5] < 0.0 || unitcell[3] > 180.0 || unitcell[4] > 180.0 || unitcell[5] > 180.0) {
            // Might be new CHARMM format: cell matrix vectors
            cell.column(0) = Vector3(unitcell[0], unitcell[1], unitcell[3]);
            cell.column(1) = Vector3(unitcell[1], unitcell[2], unitcell[4]);
            cell.column(2) = Vector3(unitcell[3], unitcell[4], unitcell[5]);
        }
        else {
            // This file was likely generated by NAMD 2.5 and the periodic cell
            // angles are specified in degrees rather than angle cosines.
            cos_alpha = std::cos(qDegreesToRadians(unitcell[4]));
            cos_beta = std::cos(qDegreesToRadians(unitcell[3]));
            cos_gamma = std::cos(qDegreesToRadians(unitcell[1]));
            sin_gamma = std::sin(qDegreesToRadians(unitcell[1]));
        }
        double a = unitcell[0];
        double b = unitcell[2];
        double c = unitcell[5];
        if(cos_alpha == 0.0 && cos_beta == 0.0 && cos_gamma == 0.0) {
            cell(0,0) = a;
            cell(1,1) = b;
            cell(2,2) = c;
        }
        else if(cos_alpha == 0.0 && cos_beta == 0.0) {
            cell(0,0) = a;
            cell(0,1) = b * cos_gamma;
            cell(1,1) = b * sin_gamma;
            cell(2,2) = c;
        }
        else {
            cell(0,0) = a;
            cell(0,1) = b * cos_gamma;
            cell(1,1) = b * sin_gamma;
            cell(0,2) = c * cos_beta;
            cell(1,2) = c * (cos_alpha - cos_beta * cos_gamma) / sin_gamma;
            cell(2,2) = std::sqrt(c * c - cell(0,2) * cell(0,2) - cell(1,2) * cell(1,2));
        }
        simulationCell()->setCellMatrix(cell);
        simulationCell()->setPbcFlags(true, true, true);
    }
    else {
        Box3 boundingBox;
        boundingBox.addPoints(posProperty);
        // If the input file does not contain simulation cell info,
        // use bounding box of particles as simulation cell.
        simulationCell()->setCellMatrix(AffineTransformation(
                Vector3(boundingBox.sizeX(), 0, 0),
                Vector3(0, boundingBox.sizeY(), 0),
                Vector3(0, 0, boundingBox.sizeZ()),
                boundingBox.minc - Point3::Origin()));
        simulationCell()->setPbcFlags(false, false, false);
    }
    posProperty.reset();

    int timestep = header.istart + header.nsavc * frame().byteOffset;
    state().setAttribute(QStringLiteral("Timestep"), QVariant::fromValue(timestep), dataSource());
    state().setAttribute(QStringLiteral("Time"), QVariant::fromValue((FloatType)(header.delta * timestep)), dataSource());

    // Call base implementation to finalize the loaded particle data.
    ParticleImporter::FrameLoader::loadFile();
}

}   // End of namespace
