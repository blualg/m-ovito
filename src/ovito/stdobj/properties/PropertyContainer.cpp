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

#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/ElementType.h>
#include <ovito/core/dataset/DataSet.h>
#include "PropertyContainer.h"

#include <boost/range/algorithm_ext/is_sorted.hpp>

namespace Ovito::StdObj {

IMPLEMENT_OVITO_CLASS(PropertyContainer);
DEFINE_VECTOR_REFERENCE_FIELD(PropertyContainer, properties);
DEFINE_PROPERTY_FIELD(PropertyContainer, elementCount);
DEFINE_PROPERTY_FIELD(PropertyContainer, title);
DEFINE_SHADOW_PROPERTY_FIELD(PropertyContainer, title);
SET_PROPERTY_FIELD_LABEL(PropertyContainer, properties, "Properties");
SET_PROPERTY_FIELD_LABEL(PropertyContainer, elementCount, "Element count");
SET_PROPERTY_FIELD_LABEL(PropertyContainer, title, "Title");
SET_PROPERTY_FIELD_CHANGE_EVENT(PropertyContainer, title, ReferenceEvent::TitleChanged);

/******************************************************************************
* Constructor.
******************************************************************************/
PropertyContainer::PropertyContainer(ObjectCreationParams params, const QString& title) : DataObject(params), 
	_elementCount(0),
	_title(title)
{
	if(!title.isEmpty())
		freezeInitialParameterValues({SHADOW_PROPERTY_FIELD(PropertyContainer::title)});
}

/******************************************************************************
* Returns the display title of this object.
******************************************************************************/
QString PropertyContainer::objectTitle() const
{
	if(!title().isEmpty()) return title();
	return DataObject::objectTitle();
}

/******************************************************************************
* Returns the given standard property. If it does not exist, an exception is thrown.
******************************************************************************/
const PropertyObject* PropertyContainer::expectProperty(int typeId) const
{
	if(!getOOMetaClass().isValidStandardPropertyId(typeId))
		throwException(tr("Selections are not supported for %1.").arg(getOOMetaClass().propertyClassDisplayName()));
	const PropertyObject* property = getProperty(typeId);
	if(!property) {
		if(typeId == PropertyObject::GenericSelectionProperty)
			throwException(tr("The operation requires an input %1 selection.").arg(getOOMetaClass().elementDescriptionName()));
		else
			throwException(tr("Required %2 property '%1' does not exist in the input dataset.").arg(getOOMetaClass().standardPropertyName(typeId), getOOMetaClass().elementDescriptionName()));
	}
	if(property->size() != elementCount())
		throwException(tr("Property array '%1' has wrong length. It does not match the number of elements in the parent container.").arg(property->name()));
	return property;
}

/******************************************************************************
* Returns the property with the given name and data layout.
******************************************************************************/
const PropertyObject* PropertyContainer::expectProperty(const QString& propertyName, int dataType, size_t componentCount) const
{
	const PropertyObject* property = getProperty(propertyName);
	if(!property)
		throwException(tr("Required property '%1' does not exist in the input dataset.").arg(propertyName));
	if(property->dataType() != dataType)
		throwException(tr("Property '%1' does not have the required data type in the pipeline dataset.").arg(property->name()));
	if(property->componentCount() != componentCount)
		throwException(tr("Property '%1' does not have the required number of components in the pipeline dataset.").arg(property->name()));
	if(property->size() != elementCount())
		throwException(tr("Property array '%1' has wrong length. It does not match the number of elements in the parent container.").arg(property->name()));
	return property;
}

/******************************************************************************
* Duplicates any property objects that are shared with other containers.
* After this method returns, all property objects are exclusively owned by the container and
* can be safely modified without unwanted side effects.
******************************************************************************/
QVector<PropertyObject*> PropertyContainer::makePropertiesMutable()
{
	QVector<PropertyObject*> result;
	for(const PropertyObject* property : properties()) {
		result.push_back(makeMutable(property));
	}
	return result;
}

/******************************************************************************
* Sets the current number of data elements stored in the container.
* The lengths of the property arrays will be adjusted accordingly.
******************************************************************************/
void PropertyContainer::setElementCount(size_t count)
{
	if(count == elementCount())
		return;

	// Make sure the property arrays can be safely modified and resize each array.
	for(PropertyObject* prop : makePropertiesMutable())
		prop->resize(count, true);

	// Update internal element counter.
	_elementCount.set(this, PROPERTY_FIELD(elementCount), count);
}

/******************************************************************************
* Deletes those data elements for which the bit is set in the given bitmask array.
* Returns the number of deleted elements.
******************************************************************************/
size_t PropertyContainer::deleteElements(const boost::dynamic_bitset<>& mask)
{
	OVITO_ASSERT(mask.size() == elementCount());

	size_t deleteCount = mask.count();
	size_t oldElementCount = elementCount();
	size_t newElementCount = oldElementCount - deleteCount;
	if(deleteCount == 0)
		return 0;	// Nothing to delete.

    // Make sure the property arrays can be safely modified.
	// Filter the property arrays and reduce their lengths.
	for(PropertyObject* property : makePropertiesMutable()) {
        OVITO_ASSERT(property->size() == oldElementCount);
        property->filterResize(mask);
        OVITO_ASSERT(property->size() == newElementCount);
	}

	// Update internal element counter.
	_elementCount.set(this, PROPERTY_FIELD(elementCount), newElementCount);

	return deleteCount;
}

/******************************************************************************
* Creates a property and adds it to the container.
* In case the property already exists, it is made sure that it's safe to modify it.
******************************************************************************/
PropertyObject* PropertyContainer::createProperty(int typeId, DataBuffer::InitializationFlags flags, const ConstDataObjectPath& containerPath)
{
	OVITO_ASSERT(isSafeToModify());

	if(getOOMetaClass().isValidStandardPropertyId(typeId) == false) {
		if(typeId == PropertyObject::GenericSelectionProperty)
			throwException(tr("Creating selections is not supported for %1.").arg(getOOMetaClass().propertyClassDisplayName()));
		else if(typeId == PropertyObject::GenericColorProperty)
			throwException(tr("Assigning colors is not supported for %1.").arg(getOOMetaClass().propertyClassDisplayName()));
		else
			throwException(tr("%1 is not a standard property ID supported by the '%2' object class.").arg(typeId).arg(getOOMetaClass().propertyClassDisplayName()));
	}

	// Check if property already exists in the output.
	if(const PropertyObject* existingProperty = getProperty(typeId)) {
		OVITO_ASSERT(existingProperty->size() == elementCount());
		if(existingProperty->isSafeToModify())
			return const_cast<PropertyObject*>(existingProperty);
		if(flags.testFlag(DataBuffer::InitializeMemory))
			return makeMutable(existingProperty);

		// If no memory initialization is requested, create a new PropertyObject from scratch and just adopt 
		// the existing ElementType list to save time.	
		PropertyPtr newProperty = getOOMetaClass().createStandardProperty(dataset(), elementCount(), typeId, flags, containerPath);
		newProperty->setElementTypes(existingProperty->elementTypes());
		replaceReferencesTo(existingProperty, newProperty);
		return newProperty;
	}
	else {
		// Create a new property object.
		PropertyPtr newProperty = getOOMetaClass().createStandardProperty(dataset(), elementCount(), typeId, flags, containerPath);
		addProperty(newProperty);
		return newProperty;
	}
}

/******************************************************************************
* Creates a user-defined property and adds it to the container.
* In case the property already exists, it is made sure that it's safe to modify it.
******************************************************************************/
PropertyObject* PropertyContainer::createProperty(const QString& name, int dataType, size_t componentCount, DataBuffer::InitializationFlags flags, QStringList componentNames)
{
	OVITO_ASSERT(isSafeToModify());

	// Check if property already exists in the output.
	const PropertyObject* existingProperty = getProperty(name);

	// Check if property already exists in the output.
	if(existingProperty) {
		if(existingProperty->dataType() != dataType)
			throwException(tr("Existing property '%1' has a different data type.").arg(name));
		if(existingProperty->componentCount() != componentCount)
			throwException(tr("Existing property '%1' has a different number of components.").arg(name));

		PropertyObject* newProperty = makeMutable(existingProperty);
		OVITO_ASSERT(newProperty->isSafeToModify());
		OVITO_ASSERT(newProperty->size() == elementCount());
		return newProperty;
	}
	else {
		// Create a new property object.
		PropertyPtr newProperty = getOOMetaClass().createUserProperty(dataset(), elementCount(), dataType, componentCount, name, flags, 0, std::move(componentNames));
		addProperty(newProperty);
		return newProperty;
	}
}

/******************************************************************************
* Adds a property object to the container, replacing any preexisting property 
* in the container with the same type. 
******************************************************************************/
const PropertyObject* PropertyContainer::createProperty(const PropertyObject* property)
{
	OVITO_CHECK_POINTER(property);
	OVITO_ASSERT(isSafeToModify());
	OVITO_ASSERT(property->type() == 0 || getOOMetaClass().isValidStandardPropertyId(property->type()));

	// Length of first property array determines number of data elements in the container.
	if(properties().empty() && elementCount() == 0)
		_elementCount.set(this, PROPERTY_FIELD(elementCount), property->size());

	// Length of new property array must match the existing number of elements.
	if(property->size() != elementCount()) {
#ifdef OVITO_DEBUG
		qDebug() << "Property array size mismatch. Container has" << elementCount() << "existing elements. New property" << property->name() << "to be added has" << property->size() << "elements.";
#endif
		throwException(tr("Cannot add new %1 property '%2': Array length is not consistent with number of elements in the parent container.").arg(getOOMetaClass().propertyClassDisplayName()).arg(property->name()));
	}

	// Check if the same property already exists in the container.
	const PropertyObject* existingProperty;
	if(property->type() != 0) {
		existingProperty = getProperty(property->type());
	}
	else {
		existingProperty = nullptr;
		for(const PropertyObject* p : properties()) {
			if(p->type() == 0 && p->name() == property->name()) {
				existingProperty = p;
				break;
			}
		}
	}

	if(existingProperty) {
		replaceReferencesTo(existingProperty, property);
	}
	else {
		OVITO_ASSERT(properties().contains(const_cast<PropertyObject*>(property)) == false);
		addProperty(property);
	}
	return property;
}

/******************************************************************************
* Replaces the property arrays in this property container with a new set of
* properties. Existing element types of typed properties will be preserved by 
* the method. 
******************************************************************************/
void PropertyContainer::setContent(size_t newElementCount, const DataRefVector<PropertyObject>& newProperties)
{
	// Lengths of new property arrays must be consistent.
	for(const PropertyObject* property : newProperties) {
		OVITO_ASSERT(!properties().contains(property));
		if(property->size() != newElementCount) {
			OVITO_ASSERT(false);
			throwException(tr("Cannot add new %1 property '%2': Array length does not match number of elements in the parent container.").arg(getOOMetaClass().propertyClassDisplayName()).arg(property->name()));
		}
	}

	// Removal phase:
	_properties.clear(this, PROPERTY_FIELD(properties));

	// Update internal element counter.
	_elementCount.set(this, PROPERTY_FIELD(elementCount), newElementCount);

	// Insertion phase:
	_properties.setTargets(this, PROPERTY_FIELD(properties), std::move(newProperties));
}

/******************************************************************************
* Duplicates all data elements by extensing the property arrays and
* replicating the existing data N times.
******************************************************************************/
void PropertyContainer::replicate(size_t n, bool replicatePropertyValues)
{
	OVITO_ASSERT(n >= 1);
	if(n <= 1) return;

	size_t newCount = elementCount() * n;
	if(newCount / n != elementCount())
		throwException(tr("Replicate operation failed: Maximum number of elements exceeded."));

	// Make sure the property arrays can be safely modified and replicate the values in each of them.
	for(PropertyObject* property : makePropertiesMutable())
		property->replicate(n, replicatePropertyValues);

	setElementCount(newCount);
}

/******************************************************************************
* Sorts the data elements with respect to their unique IDs.
* Does nothing if data elements do not have IDs.
******************************************************************************/
std::vector<size_t> PropertyContainer::sortById()
{
#ifdef OVITO_DEBUG
	verifyIntegrity();
#endif	
	if(!getOOMetaClass().isValidStandardPropertyId(PropertyObject::GenericIdentifierProperty))
		return {};
	ConstDataBufferAccess<qlonglong> ids = getProperty(PropertyObject::GenericIdentifierProperty);
	if(!ids) 
		return {};

	// Determine new permutation of data elements which sorts them by ascending ID.
	std::vector<size_t> permutation(ids.size());
	std::iota(permutation.begin(), permutation.end(), (size_t)0);
	std::sort(permutation.begin(), permutation.end(), [&](size_t a, size_t b) { return ids[a] < ids[b]; });
	std::vector<size_t> invertedPermutation(ids.size());
	bool isAlreadySorted = true;
	for(size_t i = 0; i < permutation.size(); i++) {
		invertedPermutation[permutation[i]] = i;
		if(permutation[i] != i) isAlreadySorted = false;
	}
	ids.reset();
	if(isAlreadySorted) return {};

	// Re-order all values in the property arrays.
	for(PropertyObject* prop : makePropertiesMutable()) {
		prop->reorderElements(permutation);
	}

	OVITO_ASSERT(boost::range::is_sorted(ConstDataBufferAccess<qlonglong>(getProperty(PropertyObject::GenericIdentifierProperty)).crange()));

	return invertedPermutation;
}

/******************************************************************************
* Makes sure that all property arrays in this container have a consistent length.
* If this is not the case, the method throws an exception.
******************************************************************************/
void PropertyContainer::verifyIntegrity() const
{
	size_t c = elementCount();
	for(const PropertyObject* property : properties()) {
//		OVITO_ASSERT_MSG(property->size() == c, "PropertyContainer::verifyIntegrity()", qPrintable(QString("Property array '%1' has wrong length. It does not match the number of elements in the parent %2 container.").arg(property->name()).arg(getOOMetaClass().propertyClassDisplayName())));
		if(property->size() != c) {
			throwException(tr("Property array '%1' has wrong length. It does not match the number of elements in the parent %2 container.").arg(property->name()).arg(getOOMetaClass().propertyClassDisplayName()));
		}
	}
}

/******************************************************************************
* Saves the class' contents to the given stream.
******************************************************************************/
void PropertyContainer::saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const
{
	DataObject::saveToStream(stream, excludeRecomputableData);
	stream.beginChunk(0x01);
	stream << excludeRecomputableData;
	stream.endChunk();
}

/******************************************************************************
* Loads the class' contents from the given stream.
******************************************************************************/
void PropertyContainer::loadFromStream(ObjectLoadStream& stream)
{
	DataObject::loadFromStream(stream);
	if(stream.formatVersion() >= 30004) {
		stream.expectChunk(0x01);
		bool excludeRecomputableData;
		stream >> excludeRecomputableData;
		if(excludeRecomputableData)
			setElementCount(0);
		stream.closeChunk();
	}
	// This is needed only for backward compatibility with early dev builds of OVITO 3.0:
	if(identifier().isEmpty())
		setIdentifier(getOOMetaClass().pythonName());
}

/******************************************************************************
* Is called once for this object after it has been completely loaded from a stream.
******************************************************************************/
void PropertyContainer::loadFromStreamComplete(ObjectLoadStream& stream)
{
	DataObject::loadFromStreamComplete(stream);

	// For backward compatibility with old OVITO versions.
	// Make sure size of deserialized property arrays is consistent. 
	if(stream.formatVersion() < 30004) {
		for(const PropertyObject* property : properties()) {
			if(property->size() != elementCount()) {
				makeMutable(property)->resize(elementCount(), true);
			}
		}
	}

	// For backward compatibility with OVITO 3.3.5: 
	// The ElementType::ownerProperty parameter field did not exist in older OVITO versions and does not have
	// a valid value when loaded from a state file. The following code initializes the parameter field to 
	// a meaningful value. 
	if(stream.formatVersion() < 30007) {
		for(const PropertyObject* property : properties()) {
			for(const ElementType* type : property->elementTypes()) {
				if(type->ownerProperty().isNull()) {
					const_cast<ElementType*>(type)->_ownerProperty.set(const_cast<ElementType*>(type), PROPERTY_FIELD(ElementType::ownerProperty), PropertyReference(&OOClass(), property));
				}
				if(ElementType* proxyType = dynamic_object_cast<ElementType>(type->editableProxy())) {
					if(proxyType->ownerProperty().isNull())
						proxyType->_ownerProperty.set(proxyType, PROPERTY_FIELD(ElementType::ownerProperty), type->ownerProperty());
				}
			}
		}
	}
}

/******************************************************************************
* Generates the info string to be displayed in the OVITO status bar for an element from this container.
******************************************************************************/
QString PropertyContainer::elementInfoString(size_t elementIndex, const ConstDataObjectRefPath& path) const
{
	QString str;
	for(const PropertyObject* property : properties()) {
		if(property->size() <= elementIndex) continue;
		if(property->type() == PropertyObject::GenericSelectionProperty) continue;
		if(property->type() == PropertyObject::GenericColorProperty) continue;
		if(!str.isEmpty()) str += QStringLiteral("<sep>");
		str += QStringLiteral("<key>");
		str += property->name().toHtmlEscaped();
		str += QStringLiteral(":</key> <val>");
		if(property->dataType() == PropertyObject::Int) {
			ConstPropertyAccess<int, true> data(property);
			for(size_t component = 0; component < data.componentCount(); component++) {
				if(component != 0) str += QStringLiteral(", ");
				str += QString::number(data.get(elementIndex, component));
				if(property->elementTypes().empty() == false) {
					if(const ElementType* ptype = property->elementType(data.get(elementIndex, component))) {
						if(!ptype->name().isEmpty())
							str += QString(" (%1)").arg(ptype->name().toHtmlEscaped());
					}
				}
			}
		}
		else if(property->dataType() == PropertyObject::Int64) {
			ConstPropertyAccess<qlonglong, true> data(property);
			for(size_t component = 0; component < property->componentCount(); component++) {
				if(component != 0) str += QStringLiteral(", ");
				str += QString::number(data.get(elementIndex, component));
			}
		}
		else if(property->dataType() == PropertyObject::Float) {
			ConstPropertyAccess<FloatType, true> data(property);
			for(size_t component = 0; component < property->componentCount(); component++) {
				if(component != 0) str += QStringLiteral(", ");
				str += QString::number(data.get(elementIndex, component));
			}
		}
		else {
			str += QStringLiteral("<%1>").arg(getQtTypeNameFromId(property->dataType()) ? getQtTypeNameFromId(property->dataType()) : "unknown");
		}
		str += QStringLiteral("</val>");
	}
	return str;
}

}	// End of namespace
