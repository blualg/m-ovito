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
#include <ovito/core/oo/RefTarget.h>
#include <ovito/core/dataset/animation/TimeInterval.h>
#include <ovito/core/dataset/data/DataVis.h>
#include <ovito/core/dataset/pipeline/PipelineObject.h>

namespace Ovito {

/**
 * \brief Abstract base class for all objects that represent a part of a data collection.
 */
class OVITO_CORE_EXPORT DataObject : public RefTarget
{
public:

	/// Give this object type its own metaclass.
	class OVITO_CORE_EXPORT OOMetaClass : public RefTarget::OOMetaClass
	{
	public:

		/// Inherit constructor from base class.
		using RefTarget::OOMetaClass::OOMetaClass;

		/// Generates a human-readable string representation of the data object reference.
		virtual QString formatDataObjectPath(const ConstDataObjectPath& path) const;
	};

	OVITO_CLASS_META(DataObject, OOMetaClass)

protected:

	/// \brief Constructor.
	using RefTarget::RefTarget;

public:

	/// \brief Asks the object for its validity interval at the given time.
	/// \param time The animation time at which the validity interval should be computed.
	/// \return The maximum time interval that contains \a time and during which the object is valid.
	///
	/// When computing the validity interval of the object, an implementation of this method
	/// should take validity intervals of all sub-objects and sub-controllers into account.
	///
	/// The default implementation returns TimeInterval::infinite().
	virtual TimeInterval objectValidity(TimePoint time) { return TimeInterval::infinite(); }

	/// \brief Attaches a visualization elements to this data object that will be responsible for rendering the
	///        data.
	void addVisElement(DataVis* vis) {
		OVITO_ASSERT(vis != nullptr);
		_visElements.push_back(this, PROPERTY_FIELD(visElements), vis);
	}

	/// \brief Attaches a visualization element to this data object that will be responsible for rendering the
	///        data.
	void insertVisElement(int index, DataVis* vis) {
		OVITO_ASSERT(vis != nullptr);
		_visElements.insert(this, PROPERTY_FIELD(visElements), index, vis);
	}

	/// \brief Detaches a visualization element from this data object.
	void removeVisElement(int index) {
		_visElements.remove(this, PROPERTY_FIELD(visElements), index);
	}

	/// \brief Attaches a visual element to this data object that will be responsible for rendering the
	///        data. Any existing visual elements will be replaced.
	void setVisElement(DataVis* vis) {
		_visElements.clear(this, PROPERTY_FIELD(visElements));
		if(vis)
			_visElements.push_back(this, PROPERTY_FIELD(visElements), vis);
	}

	/// \brief Returns the first visualization element attached to this data object or NULL if there is
	///        no element attached.
	DataVis* visElement() const {
		return !visElements().empty() ? visElements().front() : nullptr;
	}

	/// \brief Returns the first visualization element of the given type attached to this data object or NULL if there is
	///        no such vis element attached.
	template<class DataVisType>
	DataVisType* visElement() const {
		for(DataVis* vis : visElements()) {
			if(DataVisType* typedVis = dynamic_object_cast<DataVisType>(vis))
				return typedVis;
		}
		return nullptr;
	}

	/// Determines if it is safe to modify this data object without unwanted sideeffects.
	/// Returns true if there is only one exclusive owner of this data object (if any).
	/// Returns false if there are multiple references to this data object from several
	/// data collections or other container data objects.
	bool isSafeToModify() const;

	/// Returns whether this data object wants to be shown in the pipeline editor
	/// under the data source section. The default implementation returns false.
	virtual bool showInPipelineEditor() const { return false; }

	/// \brief Visits the direct sub-objects of this data object
	///        and invokes the given visitor function for every sub-object.
	///
	/// \param fn A functor that takes a DataObject pointer as argument and returns a bool to
	///           indicate whether visiting of further sub-objects should continue.
	template<class Function>
	bool visitSubObjects(Function fn) const {
		for(const PropertyFieldDescriptor* field : getOOMetaClass().propertyFields()) {
			if(field->isReferenceField() && !field->isWeakReference() && field->targetClass()->isDerivedFrom(DataObject::OOClass()) && !field->flags().testFlag(PROPERTY_FIELD_NO_SUB_ANIM)) {
				if(!field->isVector()) {
					if(const DataObject* subObject = static_object_cast<DataObject>(getReferenceFieldTarget(field))) {
						if(fn(subObject))
							return true;
					}
				}
				else {
					int count = getVectorReferenceFieldSize(field);
					for(int i = 0; i < count; i++) {
						if(const DataObject* subObject = static_object_cast<DataObject>(getVectorReferenceFieldTarget(field, i))) {
							if(fn(subObject))
								return true;
						}
					}
				}
			}
		}
		return false;
	}

	/// Duplicates the given sub-object from this container object if it is shared with others.
	/// After this method returns, the returned sub-object will be exclusively owned by this container and
	/// can be safely modified without unwanted side effects.
	DataObject* makeMutable(const DataObject* subObject);

	/// Duplicates the given sub-object from this container object if it is shared with others.
	/// After this method returns, the returned sub-object will be exclusively owned by this container and
	/// can be safely modified without unwanted side effects.
	DataObject* makeMutable(const DataObject* subObject, CloneHelper& cloneHelper);

	/// Duplicates the given sub-object from this container object if it is shared with others.
	/// After this method returns, the returned sub-object will be exclusively owned by this container and
	/// can be safely modified without unwanted side effects.
	template<class DataObjectClass>
	DataObjectClass* makeMutable(const DataObjectClass* subObject) {
		return static_object_cast<DataObjectClass>(makeMutable(static_cast<const DataObject*>(subObject)));
	}

	/// Returns the absolute path of this DataObject within the DataCollection.
	/// Returns an empty path if the DataObject is not exclusively owned by one DataCollection.
	ConstDataObjectPath exclusiveDataObjectPath() const;

	/// Creates an editable proxy object for this DataObject and synchronizes its parameters.
	virtual void updateEditableProxies(PipelineFlowState& state, ConstDataObjectPath& dataPath) const;

protected:

	/// Is called when a RefTarget referenced by this object has generated an event.
	virtual bool referenceEvent(RefTarget* source, const ReferenceEvent& event) override;

	/// Saves the class' contents to the given stream.
	virtual void saveToStream(ObjectSaveStream& stream, bool excludeRecomputableData) const override;

	/// Loads the class' contents from the given stream.
	virtual void loadFromStream(ObjectLoadStream& stream) override;

private:

	/// Increments the shared-ownership count of this DataObject by one. This method is called by the DataOORef smart-pointer class.
	void incrementDataReferenceCount() const noexcept {
		OVITO_CHECK_OBJECT_POINTER(this);
		_dataReferenceCount.ref();
	}

	/// Decrements the shared-ownership count of this DataObject by one. This method is called by the DataOORef smart-pointer class.
	void decrementDataReferenceCount() const noexcept {
		OVITO_CHECK_OBJECT_POINTER(this);
		OVITO_ASSERT(_dataReferenceCount.loadAcquire() > 0);
		_dataReferenceCount.deref();
	}

private:

	/// The unique identifier of the data object by which it can be referred to from Python, for example.
	DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, identifier, setIdentifier);

	/// The attached visual elements that are responsible for rendering this object's data.
	DECLARE_MODIFIABLE_VECTOR_REFERENCE_FIELD_FLAGS(OORef<DataVis>, visElements, setVisElements, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_MEMORIZE);

	/// The pipeline object that created this data object (may be null).
	DECLARE_RUNTIME_PROPERTY_FIELD(QPointer<PipelineObject>, dataSource, setDataSource);

	/// The attached editable proxy object.
	DECLARE_MODIFIABLE_REFERENCE_FIELD_FLAGS(OORef<RefTarget>, editableProxy, setEditableProxy, PROPERTY_FIELD_NEVER_CLONE_TARGET | PROPERTY_FIELD_NO_SUB_ANIM);

	/// The current number of strong references to this DataObject that exist.
	mutable QAtomicInt _dataReferenceCount{0};

	// Give DataOORef smart-pointer class direct access to the DataObject's shared owenership counter.
	template<typename DataObjectClass> friend class DataOORef;
};

/// A pointer to a DataObject-derived metaclass.
using DataObjectClassPtr = const DataObject::OOMetaClass*;

}	// End of namespace

#include <ovito/core/dataset/data/DataOORef.h>

namespace Ovito {

// Instantiate class templates.
#ifndef OVITO_BUILD_MONOLITHIC
	#if !defined(Core_EXPORTS)
		extern template class OVITO_CORE_EXPORT SingleReferenceFieldBase<DataOORef<const DataObject>>;
		extern template class OVITO_CORE_EXPORT VectorReferenceFieldBase<DataOORef<const DataObject>>;
	#elif !defined(Q_CC_MSVC) && !defined(Q_CC_CLANG)
		template class OVITO_CORE_EXPORT SingleReferenceFieldBase<DataOORef<const DataObject>>;
		template class OVITO_CORE_EXPORT VectorReferenceFieldBase<DataOORef<const DataObject>>;
	#endif
#endif

}	// End of namespace
