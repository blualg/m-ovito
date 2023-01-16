////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
#include <ovito/stdobj/properties/PropertyObject.h>
#include <ovito/stdobj/io/StandardFrameLoader.h>
#include <ovito/core/dataset/io/FileSourceImporter.h>

namespace Ovito::Particles {

/**
 * \brief Base class for file parsers that read particle datasets.
 */
class OVITO_PARTICLES_EXPORT ParticleImporter : public FileSourceImporter
{
    OVITO_CLASS(ParticleImporter)

public:

    /// \brief Constructs a new instance of this class.
    ParticleImporter(ObjectCreationParams params) : FileSourceImporter(params), 
        _sortParticles(false), _generateBonds(false), _recenterCell(false) {}

    /// Indicates whether this file importer type loads particle trajectories.
    virtual bool isTrajectoryFormat() const { return false; } 

    /// \brief Returns the priority level of this importer, which is used to order multiple files that are imported simultaneously.
    virtual int importerPriority() const override {
        // When importing multiple files at once, make sure trajectory importers are called after 
        // non-trajectory (i.e. topology) importers by giving them a lower priority. 
        // The topology importer's importFurtherFiles() method will then be called first and can insert a "Load Trajectory" modifier
        // into the pipeline for loading the trajectory data file(s).
        return isTrajectoryFormat() ? -1 : FileSourceImporter::importerPriority(); 
    }

protected:

    /// The format-specific task object that is responsible for reading an input file in the background.
    class OVITO_PARTICLES_EXPORT FrameLoader : public StandardFrameLoader
    {
    public:

        /// Constructor.
        using StandardFrameLoader::StandardFrameLoader;

        /// Constructor.
        FrameLoader(const LoadOperationRequest& request, bool recenterCell) : StandardFrameLoader::StandardFrameLoader(request), _recenterCell(recenterCell) {}

        /// Returns the particles container object, newly creating it first if necessary.
        ParticlesObject* particles();

        /// Returns the bonds container object, newly creating it first if necessary.
        BondsObject* bonds();

        /// Returns the angles container object, newly creating it first if necessary.
        AnglesObject* angles();

        /// Returns the dihedrals container object, newly creating it first if necessary.
        DihedralsObject* dihedrals();

        /// Returns the impropers container object, newly creating it first if necessary.
        ImpropersObject* impropers();

        /// Creates a particles container object (if the particle count is non-zero) and adjusts the number of elements of the property container.
        void setParticleCount(size_t count);

        /// Creates a bonds container object (if the bond count is non-zero) and adjusts the number of elements of the property container.
        void setBondCount(size_t count);

        /// Creates an angles container object (if the bond count is non-zero) and adjusts the number of elements of the property container.
        void setAngleCount(size_t count);

        /// Creates a dihedrals container object (if the bond count is non-zero) and adjusts the number of elements of the property container.
        void setDihedralCount(size_t count);

        /// Creates an impropers container object (if the bond count is non-zero) and adjusts the number of elements of the property container.
        void setImproperCount(size_t count);

        /// Determines the PBC shift vectors for bonds based on the minimum image convention.
        void generateBondPeriodicImageProperty();

        /// Generates ad-hoc bonds between atoms based on their van der Waals radii.
        void generateBonds();

        /// If the particles are centered on the coordinate origin but the current simulation cell corner is positioned at (0,0,0), 
        /// the this method centers the cell at (0,0,0), leaving the particle coordinates unchanged.
        void correctOffcenterCell();

        /// Indicates that the particles data object was newly created by this file reader.
        bool areParticlesNewlyCreated() const { return _areParticlesNewlyCreated; }

        /// Indicates that the bonds data object was newly created by this file reader.
        bool areBondsNewlyCreated() const { return _areBondsNewlyCreated; }

        /// Indicates that the angles data object was newly created by this file reader.
        bool areAnglesNewlyCreated() const { return _areAnglesNewlyCreated; }

        /// Indicates that the dihedrals data object was newly created by this file reader.
        bool areDihedralsNewlyCreated() const { return _areDihedralsNewlyCreated; }

        /// Indicates that the impropers data object was newly created by this file reader.
        bool areImpropersNewlyCreated() const { return _areImpropersNewlyCreated; }

        /// Controls whether file loader should clear bonds, angles, etc. at the end of the loading process.
        void setKeepExistingTopology(bool enable) { _keepExistingTopology = enable; }

    protected:

        /// Finalizes the particle data loaded by a sub-class.
        virtual void loadFile() override;

    private:

        /// If the 'Velocity' vector particle property is present, then this method computes the 'Velocity Magnitude' scalar property.
        void computeVelocityMagnitude();

        /// Translates the simulation cell (and the particles) such that it is centered at the coordinate origin.
        void recenterSimulationCell();

    private:

        /// The particles container object.
        ParticlesObject* _particles = nullptr;

        /// The bonds container object.
        BondsObject* _bonds = nullptr;

        /// The angles container object.
        AnglesObject* _angles = nullptr;

        /// The dihedrals container object.
        DihedralsObject* _dihedrals = nullptr;

        /// The impropers container object.
        ImpropersObject* _impropers = nullptr;

        /// Controls the dynamic centering of the simulation cell during import.
        bool _recenterCell = false;

        /// Indicates that the particles data object was newly created by this file reader.
        bool _areParticlesNewlyCreated = false;

        /// Indicates that the bonds data object was newly created by this file reader.
        bool _areBondsNewlyCreated = false;

        /// Indicates that the angles data object was newly created by this file reader.
        bool _areAnglesNewlyCreated = false;

        /// Indicates that the dihedrals data object was newly created by this file reader.
        bool _areDihedralsNewlyCreated = false;

        /// Indicates that the impropers data object was newly created by this file reader.
        bool _areImpropersNewlyCreated = false;

        /// Indicates that the file loader should not clear bonds, angles, etc. at the end of the loading process.
        /// It will be set to true if the file loader loads some bonds, angles, etc.
        bool _keepExistingTopology = false;
    };

    /// Is called when the value of a property of this object has changed.
    virtual void propertyChanged(const PropertyFieldDescriptor* field) override;

    /// Is called when importing multiple files of different formats.
    virtual bool importFurtherFiles(Scene* scene, std::vector<std::pair<QUrl, OORef<FileImporter>>> sourceUrlsAndImporters, ImportMode importMode, bool autodetectFileSequences, MultiFileImportMode multiFileImportMode, PipelineSceneNode* pipeline) override;

private:

    /// Controls sorting of the input particle with respect to IDs.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, sortParticles, setSortParticles);

    /// Controls the generation of atomic ad-hoc bonds during data import.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, generateBonds, setGenerateBonds);

    /// Controls the dynamic recentering of simulation cell to the coordinate origin.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool, recenterCell, setRecenterCell);
};

}   // End of namespace
