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


#include <ovito/core/Core.h>
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/oo/PropertyField.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/rendering/RenderSettings.h>
#include <ovito/core/viewport/ViewportConfiguration.h>

namespace Ovito {

/**
 * \brief Stores the current program state including the list of viewports, the scene, viewport configuration,
 *        render settings etc.
 *
 * A DataSet represents the state of the current program session.
 * It can be saved to a file (.ovito suffix) and loaded again at a later time.
 */
class OVITO_CORE_EXPORT DataSet final : public RefTarget
{
    /// Give this class its own metaclass.
    class DataSetClass : public RefTarget::OOMetaClass
    {
    public:

        /// Inherit constructor from base class.
        using RefTarget::OOMetaClass::OOMetaClass;

        /// Provides a custom function that takes are of the deserialization of a serialized property field that has been removed from the class.
        /// This is needed for backward compatibility with OVITO 3.7.
        virtual SerializedClassInfo::PropertyFieldInfo::CustomDeserializationFunctionPtr overrideFieldDeserialization(const SerializedClassInfo::PropertyFieldInfo& field) const override;
    };

    OVITO_CLASS_META(DataSet, DataSetClass)

public:

    /// \brief Constructs an empty dataset.
    Q_INVOKABLE DataSet(ObjectInitializationFlags flags);

    /// \brief Destructor.
    virtual ~DataSet();

    /// \brief Returns the path where this dataset is stored on disk.
    /// \return The location where the dataset is stored or will be stored on disk.
    const QString& filePath() const { return _filePath; }

    /// \brief Sets the path where this dataset is stored.
    /// \param path The new path (should be absolute) where the dataset will be stored.
    void setFilePath(const QString& path) {
        if(path != _filePath) {
            _filePath = path;
            Q_EMIT filePathChanged(_filePath);
        }
    }

    /// \brief Returns the container this dataset belongs to.
    DataSetContainer* container() const;

    /// \brief Rescales the animation keys of all controllers in the scene.
    /// \param oldAnimationInterval The old animation interval, which will be mapped to the new animation interval.
    /// \param newAnimationInterval The new animation interval.
    ///
    /// This method calls RefTarget::rescaleTime() for all objects (including animation controllers) in the scene.
    /// For keyed controllers this will rescale the key times of all keys from the
    /// old animation interval to the new interval using a linear mapping.
    ///
    /// Keys that lie outside of the old active animation interval will also be rescaled
    /// according to a linear extrapolation.
    ///
    /// \undoable
    virtual void rescaleTime(const TimeInterval& oldAnimationInterval, const TimeInterval& newAnimationInterval) override;

    /// \brief Saves the dataset to a session state file.
    /// \throw Exception on error.
    ///
    /// Note that this method does NOT invoke setFilePath().
    void saveToFile(const QString& filePath) const;

    /// \brief Loads the dataset contents from a session state file.
    /// \throw Exception on error.
    ///
    /// Note that this method does NOT invoke setFilePath().
    void loadFromFile(const QString& filePath);

    /// \brief Appends an object to this dataset's list of global objects.
    void addGlobalObject(const RefTarget* target)
    {
        if(!_globalObjects.contains(target)) _globalObjects.push_back(this, PROPERTY_FIELD(globalObjects), target);
    }

    /// \brief Removes an object from this dataset's list of global objects.
    void removeGlobalObject(int index) { _globalObjects.remove(this, PROPERTY_FIELD(globalObjects), index); }

    /// \brief Looks for a global object of the given type.
    template <class T>
    T* findGlobalObject() const
    {
        for(RefTarget* obj : globalObjects()) {
            T* castObj = dynamic_object_cast<T>(obj);
            if(castObj) return castObj;
        }
        return nullptr;
    }

Q_SIGNALS:

    /// \brief This signal is emitted whenever the current viewport configuration of this dataset
    ///        has been replaced by a new one.
    /// \note This signal is NOT emitted when parameters of the current viewport configuration change.
    void viewportConfigReplaced(ViewportConfiguration* newViewportConfiguration);

    /// \brief This signal is emitted whenever the current render settings of this dataset
    ///        have been replaced by new ones.
    /// \note This signal is NOT emitted when parameters of the current render settings object change.
    void renderSettingsReplaced(RenderSettings* newRenderSettings);

    /// \brief This signal is emitted whenever the dataset has been saved under a new file name.
    void filePathChanged(const QString& filePath);

protected:

    /// Is called when a RefTarget referenced by this object has generated an event.
    virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

    /// Is called when the value of a reference field of this RefMaker changes.
    virtual void referenceReplaced(const PropertyFieldDescriptor* field, RefTarget* oldTarget, RefTarget* newTarget, int listIndex) override;

    /// This method is called once for this object after it has been completely loaded from a stream.
    virtual void loadFromStreamComplete(ObjectLoadStream& stream) override;

private:

    /// Returns a viewport configuration that is used as template for new scenes.
    static OORef<ViewportConfiguration> createDefaultViewportConfiguration();

private:

    /// The configuration of the interactive viewports in the OVITO desktop application.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<ViewportConfiguration>, viewportConfig, setViewportConfig, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_ALWAYS_DEEP_COPY | PROPERTY_FIELD_MEMORIZE);

    /// The settings for rendering an output image of the scene.
    DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<RenderSettings>, renderSettings, setRenderSettings, PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_ALWAYS_DEEP_COPY | PROPERTY_FIELD_MEMORIZE);

    /// Global data managed by plugins.
    DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<RefTarget>, globalObjects, setGlobalObjects,
                                                    PROPERTY_FIELD_NO_CHANGE_MESSAGE | PROPERTY_FIELD_ALWAYS_CLONE |
                                                        PROPERTY_FIELD_ALWAYS_DEEP_COPY);

    /// The file path this DataSet has been saved to.
    QString _filePath;

    /// The DataSetContainer which currently hosts this DataSet.
    QPointer<DataSetContainer> _container;

    friend class DataSetContainer;
};

}   // End of namespace
