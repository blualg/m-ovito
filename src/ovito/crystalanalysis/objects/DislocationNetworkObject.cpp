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

#include <ovito/crystalanalysis/CrystalAnalysis.h>
#include "DislocationNetworkObject.h"
#include "DislocationVis.h"

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(DislocationNetworkObject);
DEFINE_RUNTIME_PROPERTY_FIELD(DislocationNetworkObject, storage);
DEFINE_VECTOR_REFERENCE_FIELD(DislocationNetworkObject, crystalStructures);
SET_PROPERTY_FIELD_LABEL(DislocationNetworkObject, crystalStructures, "Crystal structures");

/// Holds a shared, empty instance of the DislocationNetwork class,
/// which is used in places where a default storage is needed.
/// This singleton instance is never modified.
static const std::shared_ptr<DislocationNetwork> defaultStorage = std::make_shared<DislocationNetwork>(std::make_shared<ClusterGraph>());

/******************************************************************************
* Constructor.
******************************************************************************/
DislocationNetworkObject::DislocationNetworkObject(ObjectInitializationFlags flags) : PeriodicDomainObject(flags), _storage(defaultStorage)
{
    if(!flags.testFlag(ObjectInitializationFlag::DontInitializeObject)) {
        if(!flags.testFlag(ObjectInitializationFlag::DontCreateVisElement)) {
            // Attach a visualization element for rendering the dislocation lines.
            setVisElement(OORef<DislocationVis>::create(flags));
        }

        // Create the "unidentified" structure.
        if(crystalStructures().empty()) {
            DataOORef<MicrostructurePhase> defaultStructure = DataOORef<MicrostructurePhase>::create(flags);
            defaultStructure->setName(tr("Unidentified structure"));
            defaultStructure->setColor(Color(1,1,1));
            defaultStructure->addBurgersVectorFamily(DataOORef<BurgersVectorFamily>::create(flags));
            addCrystalStructure(std::move(defaultStructure));
        }
    }
}

/******************************************************************************
* Returns the data encapsulated by this object after making sure it is not
* shared with other owners.
******************************************************************************/
const std::shared_ptr<DislocationNetwork>& DislocationNetworkObject::modifiableStorage()
{
    // Copy data storage on write if there is more than one reference to the storage.
    OVITO_ASSERT(storage());
    OVITO_ASSERT(storage().use_count() >= 1);
    if(storage().use_count() > 1)
        _storage.mutableValue() = std::make_shared<DislocationNetwork>(*storage());
    OVITO_ASSERT(storage().use_count() == 1);
    return storage();
}

/******************************************************************************
* Creates an editable proxy object for this DataObject and synchronizes its parameters.
******************************************************************************/
void DislocationNetworkObject::updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath) const
{
    PeriodicDomainObject::updateEditableProxies(state, dataPath);

    // Note: 'this' may no longer exist at this point, because the base method implementation may
    // have already replaced it with a mutable copy.
    const DislocationNetworkObject* self = static_object_cast<DislocationNetworkObject>(dataPath.back());

    if(DislocationNetworkObject* proxy = static_object_cast<DislocationNetworkObject>(self->editableProxy())) {
        // Synchronize the actual data object with the editable proxy object.

        // Add the proxies of newly created microstructure phases to the proxy object.
        for(const MicrostructurePhase* phase : self->crystalStructures()) {
            MicrostructurePhase* proxyPhase = static_object_cast<MicrostructurePhase>(phase->editableProxy());
            OVITO_ASSERT(proxyPhase != nullptr);
            if(!proxy->crystalStructures().contains(proxyPhase))
                proxy->addCrystalStructure(proxyPhase);
        }

        // Add microstructure phases that are non-existing in the actual data object.
        // Note: Currently this should never happen, because file parser never
        // remove element types.
        for(const MicrostructurePhase* proxyPhase : proxy->crystalStructures()) {
            OVITO_ASSERT(std::any_of(self->crystalStructures().begin(), self->crystalStructures().end(), [proxyPhase](const MicrostructurePhase* phase) { return phase->editableProxy() == proxyPhase; }));
        }
    }
    else {
        // Create and initialize a new proxy object.
        // Note: We avoid copying the actual dislocation data here by constructing the proxy DislocationNetworkObject from scratch instead of cloning the original data object.
        OORef<DislocationNetworkObject> newProxy = OORef<DislocationNetworkObject>::create(ObjectInitializationFlag::DontCreateVisElement);
        newProxy->setTitle(self->title());
        while(!newProxy->crystalStructures().empty())
            newProxy->removeCrystalStructure(0);

        // Adopt the proxy objects for the microstructure phase types, which have already been created by
        // the recursive method.
        for(const MicrostructurePhase* phase : self->crystalStructures()) {
            OVITO_ASSERT(phase->editableProxy() != nullptr);
            newProxy->addCrystalStructure(static_object_cast<MicrostructurePhase>(phase->editableProxy()));
        }

        // Make this data object mutable and attach the proxy object to it.
        state.makeMutableInplace(dataPath)->setEditableProxy(std::move(newProxy));
    }
}

}   // End of namespace
