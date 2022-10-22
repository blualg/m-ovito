////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/pipeline/PipelineStatus.h>
#include <ovito/core/dataset/data/DataObject.h>
#include <ovito/core/dataset/data/DataCollection.h>

namespace Ovito {

/**
 * \brief This data structure holds the list of data objects that flows down a data pipeline.
 */
class OVITO_CORE_EXPORT PipelineFlowState
{
public:

	/// \brief Default constructor that creates an empty state.
	PipelineFlowState() = default;

	/// \brief Constructor that initializes the state with the data from a DataCollection.
	/// \param dataCollection The data which is used to fill the state.
	/// \param status A status object describing the outcome of the pipeline evaluation.
	/// \param validityInterval The time interval during which the state is valid.
	PipelineFlowState(DataOORef<const DataCollection> dataCollection, const PipelineStatus& status, const TimeInterval& validityInterval = TimeInterval::infinite()) :
		_data(std::move(dataCollection)), _status(status), _stateValidity(validityInterval) {}

	/// \brief Discards all contents of this state object and resets it to an empty state.
	void reset() {
		_data.reset();
		_stateValidity.setEmpty();
		_status = PipelineStatus();
	}

	/// \brief Returns whether this flow state has a data collection or not.
	explicit operator bool() const { return (bool)_data; }

	/// \brief Adds an additional data object to this state.
	void addObject(const DataObject* obj) {
		mutableData()->addObject(obj);
	}

	/// \brief Removes a data object from this state.
	void removeObject(const DataObject* obj) {
		mutableData()->removeObject(obj);
	}

	/// \brief Removes a data object from this state.
	void removeObjectByIndex(int index) {
		mutableData()->removeObjectByIndex(index);
	}

	/// \brief Replaces a data object with a new one.
	bool replaceObject(const DataObject* oldObj, const DataObject* newObj) {
		if(newObj != oldObj)
			return mutableData()->replaceObject(oldObj, newObj);
		return false;
	}

	/// \brief Gets the validity interval for this pipeline state.
	/// \return The time interval during which the returned object is valid.
	///         For times outside this interval the geometry pipeline has to be re-evaluated.
	const TimeInterval& stateValidity() const { return _stateValidity; }

	/// \brief Gets a non-const reference to the validity interval of this pipeline state.
	TimeInterval& mutableStateValidity() { return _stateValidity; }

	/// \brief Specifies the validity interval for this pipeline state.
	/// \param newInterval The time interval during which the object set by setResult() is valid.
	/// \sa intersectStateValidity()
	void setStateValidity(const TimeInterval& newInterval) { _stateValidity = newInterval; }

	/// \brief Reduces the validity interval of this pipeline state to include only the given time interval.
	/// \param intersectionInterval The current validity interval is reduced to include only this time interval.
	/// \sa setStateValidity()
	void intersectStateValidity(const TimeInterval& intersectionInterval) { _stateValidity.intersect(intersectionInterval); }

	/// Returns the status of the pipeline evaluation.
	const PipelineStatus& status() const { return _status; }

	/// Sets the stored status.
	void setStatus(const PipelineStatus& status) { _status = status; }

	/// Sets the stored status.
	void setStatus(PipelineStatus&& status) { _status = std::move(status); }

	/// Returns the data collection of this pipeline state after making sure it is safe to modify it.
	DataCollection* mutableData();

	const DataCollection* data() const { return _data.get(); }
	
	void setData(const DataCollection* data) { _data = data; }
	void setData(DataOORef<const DataCollection> data) { _data = std::move(data); }

	/// Moves the payload data our of this PipelineFlowState.
	DataOORef<const DataCollection> takeData() { return std::move(_data); }

	/// \brief Finds an object of the given type in the list of data objects stored in this flow state.
	const DataObject* getObject(const DataObject::OOMetaClass& objectClass) const {
		return data() ? data()->getObject(objectClass) : nullptr;
	}

	/// \brief Finds all objects of the given type in the list of data objects stored in this flow state.
	std::vector<const DataObject*> getObjects(const DataObject::OOMetaClass& objectClass) const {
		return data() ? data()->getObjects(objectClass) : std::vector<const DataObject*>{};
	}

	/// \brief Finds an object of the given type in the list of data objects stored in this flow state.
	template<class DataObjectClass>
	const DataObjectClass* getObject() const {
		return data() ? data()->getObject<DataObjectClass>() : nullptr;
	}

	/// \brief Determines if an object of the given type is in this flow state.
	template<class DataObjectClass>
	bool containsObject() const {
		return data() ? data()->containsObject<DataObjectClass>() : nullptr;
	}

	/// Throws an exception if the input does not contain a data object of the given type.
	const DataObject* expectObject(const DataObject::OOMetaClass& objectClass) const {
		OVITO_ASSERT(data());
		return data()->expectObject(objectClass);
	}

	/// Throws an exception if the input does not contain a data object of the given type.
	template<class DataObjectClass>
	const DataObjectClass* expectObject() const {
		OVITO_ASSERT(data());
		return data()->expectObject<DataObjectClass>();
	}

	/// Throws an exception if the input does not contain a data object of the given type.
	DataObject* expectMutableObject(const DataObject::OOMetaClass& objectClass) {
		return mutableData()->expectMutableObject(objectClass);
	}

	/// Throws an exception if the input does not contain a data object of the given type.
	template<class DataObjectClass>
	DataObjectClass* expectMutableObject() {
		return mutableData()->expectMutableObject<DataObjectClass>();
	}

	/// Finds an object of the given type in the list of data objects stored in this flow state.
	/// If it exists, makes the data object mutable.
	template<class DataObjectClass>
	DataObjectClass* getMutableObject() {
		if(const DataObjectClass* obj = getObject<DataObjectClass>())
			return mutableData()->makeMutable<DataObjectClass>(obj);
		return nullptr;
	}

	/// Finds an object of the given type in the list of data objects stored in this flow state
	/// or among any of their sub-objects.
	bool containsObjectRecursive(const DataObject::OOMetaClass& objectClass) const {
		return data() ? data()->containsObjectRecursive(objectClass) : false;
	}

	/// Finds all objects of the given type in this flow state (also searching among sub-objects).
	/// Returns them as a list of object paths.
	std::vector<ConstDataObjectPath> getObjectsRecursive(const DataObject::OOMetaClass& objectClass) const {
		return data() ? data()->getObjectsRecursive(objectClass) : std::vector<ConstDataObjectPath>{};
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	ConstDataObjectPath getObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const {
		return data() ? data()->getObject(objectClass, pathString) : ConstDataObjectPath{};
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	ConstDataObjectPath getObject(const DataObjectReference& dataRef) const {
		return data() ? data()->getObject(dataRef) : ConstDataObjectPath{};
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	template<class DataObjectClass>
	ConstDataObjectPath getObject(const QString& pathString) const {
		return data() ? data()->getObject<DataObjectClass>(pathString) : ConstDataObjectPath{};
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	template<class DataObjectClass>
	ConstDataObjectPath getObject(const TypedDataObjectReference<DataObjectClass>& dataRef) const {
		return data() ? data()->getObject<DataObjectClass>(dataRef) : ConstDataObjectPath{};
	}

	/// Throws an exception if the input does not contain any a data object of the given type and under the given hierarchy path.
	ConstDataObjectPath expectObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const {
		OVITO_ASSERT(data());
		return data()->expectObject(objectClass, pathString);
	}

	/// Throws an exception if the input does not contain any a data object of the given type and under the given hierarchy path.
	ConstDataObjectPath expectObject(const DataObjectReference& dataRef) const {
		OVITO_ASSERT(data());
		return data()->expectObject(dataRef);
	}

	/// Throws an exception if the input does not contain any a data object of the given type and under the given hierarchy path.
	template<class DataObjectClass>
	ConstDataObjectPath expectObject(const QString& pathString) const {
		OVITO_ASSERT(data());
		return data()->expectObject<DataObjectClass>(pathString);
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	template<class DataObjectClass>
	ConstDataObjectPath expectObject(const TypedDataObjectReference<DataObjectClass>& dataRef) const {
		OVITO_ASSERT(data());
		return data()->expectObject<DataObjectClass>(dataRef);
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	const DataObject* getLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const {
		return data() ? data()->getLeafObject(objectClass, pathString) : nullptr;
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	const DataObject* getLeafObject(const DataObjectReference& dataRef) const {
		return data() ? data()->getLeafObject(dataRef) : nullptr;
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	template<class DataObjectClass>
	const DataObjectClass* getLeafObject(const TypedDataObjectReference<DataObjectClass>& dataRef) const {
		return data() ? data()->getLeafObject(dataRef) : nullptr;
	}

	/// Throws an exception if the input does not contain a data object of the given type.
	const DataObject* expectLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) const {
		OVITO_ASSERT(data());
		return data()->expectLeafObject(objectClass, pathString);
	}

	/// Throws an exception if the input does not contain a data object of the given type.
	const DataObject* expectLeafObject(const DataObjectReference& dataRef) const {
		OVITO_ASSERT(data());
		return data()->expectLeafObject(dataRef);
	}

	/// Throws an exception if the input does not contain a data object of the given type.
	template<class DataObjectClass>
	const DataObjectClass* expectLeafObject(const TypedDataObjectReference<DataObjectClass>& dataRef) const {
		OVITO_ASSERT(data());
		return data()->expectLeafObject<DataObjectClass>(dataRef);
	}

	/// Finds an object of the given type and with the given identifier in the list of data objects stored in this flow state.
	const DataObject* getObjectBy(const DataObject::OOMetaClass& objectClass, const PipelineObject* dataSource, const QString& identifier) const {
		return data() ? data()->getObjectBy(objectClass, dataSource, identifier) : nullptr;
	}

	/// Finds an object of the given type and with the given identifier in the list of data objects stored in this flow state.
	template<class DataObjectClass>
	const DataObjectClass* getObjectBy(const PipelineObject* dataSource, const QString& identifier) const {
		return data() ? data()->getObjectBy<DataObjectClass>(dataSource, identifier) : nullptr;
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	/// Duplicates it, and all its parent objects, if needed so that it can be safely modified without unwanted side effects.
	DataObjectPath getMutableObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) {
		return mutableData()->getMutableObject(objectClass, pathString);
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	/// Duplicates it, and all its parent objects, if needed so that it can be safely modified without unwanted side effects.
	template<class DataObjectClass>
	DataObjectPath getMutableObject(const TypedDataObjectReference<DataObjectClass>& dataRef) {
		return mutableData()->getMutableObject<DataObjectClass>(dataRef);
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	DataObject* getMutableLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) {
		return mutableData()->getMutableLeafObject(objectClass, pathString);
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	DataObject* getMutableLeafObject(const DataObjectReference& dataRef) {
		return mutableData()->getMutableLeafObject(dataRef);
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	template<class DataObjectClass>
	DataObjectClass* getMutableLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) {
		return mutableData()->getMutableLeafObject<DataObjectClass>(objectClass, pathString);
	}

	/// Finds an object of the given type and under the hierarchy path in this flow state.
	template<class DataObjectClass>
	DataObjectClass* getMutableLeafObject(const TypedDataObjectReference<DataObjectClass>& dataRef) {
		return mutableData()->getMutableLeafObject<DataObjectClass>(dataRef);
	}

	/// Throws an exception if the input does not contain a data object of the given type and under the given hierarchy path.
	DataObjectPath expectMutableObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) {
		return mutableData()->expectMutableObject(objectClass, pathString);
	}

	/// Throws an exception if the input does not contain a data object of the given type and under the given hierarchy path.
	DataObjectPath expectMutableObject(const DataObjectReference& dataRef) {
		return mutableData()->expectMutableObject(dataRef);
	}

	/// Throws an exception if the input does not contain a data object of the given type and under the given hierarchy path.
	DataObject* expectMutableLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) {
		return mutableData()->expectMutableLeafObject(objectClass, pathString);
	}

	/// Throws an exception if the input does not contain a data object of the given type and under the given hierarchy path.
	DataObject* expectMutableLeafObject(const DataObjectReference& dataRef) {
		return mutableData()->expectMutableLeafObject(dataRef);
	}

	/// Throws an exception if the input does not contain a data object of the given type and under the given hierarchy path.
	template<class DataObjectClass>
	DataObjectClass* expectMutableLeafObject(const DataObject::OOMetaClass& objectClass, const QString& pathString) {
		return mutableData()->expectMutableLeafObject<DataObjectClass>(objectClass, pathString);
	}

	/// Throws an exception if the input does not contain a data object of the given type and under the given hierarchy path.
	template<class DataObjectClass>
	DataObjectClass* expectMutableLeafObject(const TypedDataObjectReference<DataObjectClass>& dataRef) {
		return mutableData()->expectMutableLeafObject<DataObjectClass>(dataRef);
	}

	/// Ensures that a DataObject from this flow state is not shared with others and is safe to modify.
	DataObject* makeMutable(const DataObject* obj, bool deepCopy = false) {
		return mutableData()->makeMutable(obj, deepCopy);
	}

	/// Ensures that a DataObject from this flow state is not shared with others and is safe to modify.
	template<class DataObjectClass>
	DataObjectClass* makeMutable(const DataObjectClass* obj, bool deepCopy = false) {
		return mutableData()->makeMutable<DataObjectClass>(obj, deepCopy);
	}

	/// Ensures that a DataObject from this flow state is not shared with others and is safe to modify.
	DataObjectPath makeMutable(const ConstDataObjectPath& path) {
		return mutableData()->makeMutable(path);
	}

	/// Ensures that a DataObject from this flow state is not shared with others and is safe to modify.
	DataObjectPath makeMutable(const ConstDataObjectPath& path, CloneHelper& cloneHelper) {
		return mutableData()->makeMutable(path, cloneHelper);
	}

	/// Makes the last object in the data path mutable and returns a pointer to the mutable copy.
	/// Also update the data path to point to the new object.
	DataObject* makeMutableInplace(ConstDataObjectPath& path);

	/// Instantiates a new data object.
	template<class DataObjectType, typename... Args>
	DataObjectType* createObject(Args&&... args) {
		return mutableData()->createObject<DataObjectType>(std::forward<Args>(args)...);
	}

	/// Instantiates a new data object.
	template<class DataObjectType, typename... Args>
	DataObjectType* createObjectWithVis(Args&&... args) {
		return mutableData()->createObjectWithVis<DataObjectType>(std::forward<Args>(args)...);
	}

	/// Adds a data object to this collection while making sure the object gets a unique identifier.
	template<class DataObjectType>
	void addObjectWithUniqueId(const DataObjectType* obj) {
		return mutableData()->addObjectWithUniqueId<DataObjectType>(obj);
	}

	/// Builds a list of the global attributes stored in this pipeline state.
	QVariantMap buildAttributesMap() const {
		OVITO_ASSERT(data());
		return data()->buildAttributesMap();
	}

	/// Looks up the value for the given global attribute.
	/// Returns a given default value if the attribute is not defined in this pipeline state.
	QVariant getAttributeValue(const QString& attrName, const QVariant& defaultValue = QVariant()) const {
		return data() ? data()->getAttributeValue(attrName, defaultValue) : defaultValue;
	}

	/// Looks up the value for the global attribute with the given base name and creator.
	/// Returns a given default value if the attribute is not defined in this pipeline state.
	QVariant getAttributeValue(const PipelineObject* dataSource, const QString& attrBaseName, const QVariant& defaultValue = QVariant()) const {
		return data() ? data()->getAttributeValue(dataSource, attrBaseName, defaultValue) : defaultValue;
	}

	/// Inserts a new global attribute into the pipeline state.
	AttributeDataObject* addAttribute(const QString& key, QVariant value, const PipelineObject* dataSource) {
		return mutableData()->addAttribute(key, std::move(value), dataSource);
	}

	/// Inserts a new global attribute into the pipeline state overwritting any existing attribute with the same name.
	AttributeDataObject* setAttribute(const QString& key, QVariant value, const PipelineObject* dataSource) {
		return mutableData()->setAttribute(key, std::move(value), dataSource);
	}

	/// Returns a new unique data object identifier that does not collide with the
	/// identifiers of any existing data object of the given type in the same data
	/// collection.
	QString generateUniqueIdentifier(const QString& baseName, const OvitoClass& dataObjectClass) const {
		OVITO_ASSERT(data());
		return data()->generateUniqueIdentifier(baseName, dataObjectClass);
	}

	/// Returns a new unique data object identifier that does not collide with the
	/// identifiers of any existing data object of the given type in the same data
	/// collection.
	template<class DataObjectClass>
	QString generateUniqueIdentifier(const QString& baseName) const {
		OVITO_ASSERT(data());
		return data()->generateUniqueIdentifier<DataObjectClass>(baseName);
	}

private:

	/// The payload data.
	DataOORef<const DataCollection> _data;

	/// The interval along the animation time line in which the pipeline state is valid.
	TimeInterval _stateValidity = TimeInterval::empty();

	/// The status of the pipeline evaluation.
	PipelineStatus _status;
};

}	// End of namespace
