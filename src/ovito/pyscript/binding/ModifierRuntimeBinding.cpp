#include <ovito/pyscript/PyScript.h>
#include <ovito/pyscript/binding/ModifierRuntimeBinding.h>
#include <ovito/pyscript/binding/PythonBinding.h>
#include <ovito/pyscript/engine/ScriptEngine.h>
#include <ovito/core/dataset/data/AttributeDataObject.h>
#include <ovito/core/dataset/data/BufferAccess.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <ovito/core/dataset/pipeline/PipelineEvaluationRequest.h>
#include <ovito/core/utilities/concurrent/TaskProgress.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/properties/PropertyContainer.h>
#include <ovito/stdobj/table/DataTable.h>

#include <QCoreApplication>
#include <QEventLoop>

namespace PyScript {

using namespace Ovito;
namespace py = pybind11;

namespace {

std::atomic<quint64> g_pythonModifierCancelEpoch{1};

[[noreturn]] void throwPythonCanceled()
{
    PyErr_SetString(PyExc_KeyboardInterrupt, "Operation has been canceled by the user.");
    throw py::error_already_set();
}

class ModifierAttributesView;
class ModifierParticlesView;
class ModifierTablesView;
class ModifierDataTableView;
class ModifierDataObjectView;
class ModifierObjectsView;
class ModifierProgress;

struct SequenceData
{
    qsizetype rows = 0;
    qsizetype components = 1;
    std::vector<double> values;
};

QString tableLookupKey(const DataTable& table)
{
    if(!table.identifier().isEmpty())
        return table.identifier();
    if(!table.title().isEmpty())
        return table.title();
    return table.objectTitle();
}

QString objectLookupKey(const DataObject& object, qsizetype index)
{
    if(const auto* attr = dynamic_object_cast<const AttributeDataObject>(&object); attr && !attr->identifier().isEmpty())
        return attr->identifier();
    if(const auto* table = dynamic_object_cast<const DataTable>(&object))
        return tableLookupKey(*table);
    if(const auto* particles = dynamic_object_cast<const Particles>(&object); particles)
        return particles->identifier().isEmpty() ? QStringLiteral("Particles") : particles->identifier();
    if(!object.identifier().isEmpty())
        return object.identifier();
    if(!object.objectTitle().isEmpty())
        return object.objectTitle();
    return QStringLiteral("%1[%2]").arg(object.getOOClass().name()).arg(index);
}

py::dtype bufferDType(int dataType)
{
    switch(dataType) {
    case DataBuffer::Int8: return py::dtype::of<int8_t>();
    case DataBuffer::Int32: return py::dtype::of<int32_t>();
    case DataBuffer::Int64: return py::dtype::of<int64_t>();
    case DataBuffer::Float32: return py::dtype::of<float>();
    case DataBuffer::Float64: return py::dtype::of<double>();
    default:
        throw Exception(QStringLiteral("Python modifier does not support buffers of type '%1'.").arg(QMetaType(dataType).name()));
    }
}

bool isSequenceLike(py::handle value)
{
    return py::isinstance<py::sequence>(value) && !py::isinstance<py::str>(value) && !py::isinstance<py::bytes>(value);
}

SequenceData parseNumericSequence(py::handle dataHandle)
{
    if(!isSequenceLike(dataHandle))
        throw Exception(QStringLiteral("Expected a one- or two-dimensional numeric sequence."));

    py::sequence outer = dataHandle.cast<py::sequence>();
    SequenceData parsed;
    parsed.rows = py::len(outer);
    if(parsed.rows == 0)
        return parsed;

    py::handle first = outer[0];
    if(isSequenceLike(first)) {
        py::sequence firstRow = first.cast<py::sequence>();
        parsed.components = py::len(firstRow);
        if(parsed.components <= 0)
            throw Exception(QStringLiteral("Nested sequences must contain at least one component."));
        parsed.values.reserve(static_cast<size_t>(parsed.rows * parsed.components));
        for(py::handle rowHandle : outer) {
            if(!isSequenceLike(rowHandle))
                throw Exception(QStringLiteral("Mixed one- and two-dimensional sequence input is not supported."));
            py::sequence row = rowHandle.cast<py::sequence>();
            if(py::len(row) != parsed.components)
                throw Exception(QStringLiteral("All nested sequences must have the same length."));
            for(py::handle component : row)
                parsed.values.push_back(py::cast<double>(component));
        }
    }
    else {
        parsed.components = 1;
        parsed.values.reserve(static_cast<size_t>(parsed.rows));
        for(py::handle value : outer)
            parsed.values.push_back(py::cast<double>(value));
    }

    return parsed;
}

py::object scalarValueAt(const Property* property, const char* base, size_t index)
{
    switch(property->dataType()) {
    case DataBuffer::Int8:
        return py::cast(static_cast<int>(reinterpret_cast<const int8_t*>(base)[index]));
    case DataBuffer::Int32:
        return py::cast(reinterpret_cast<const int32_t*>(base)[index]);
    case DataBuffer::Int64:
        return py::cast(reinterpret_cast<const int64_t*>(base)[index]);
    case DataBuffer::Float32:
        return py::cast(reinterpret_cast<const float*>(base)[index]);
    case DataBuffer::Float64:
        return py::cast(reinterpret_cast<const double*>(base)[index]);
    default:
        throw Exception(QStringLiteral("Unsupported property data type."));
    }
}

py::list propertyList(const ConstPropertyPtr& property)
{
    RawBufferReadAccessAndRef access(property);
    py::list result;
    const char* src = reinterpret_cast<const char*>(access.cdata());

    if(property->componentCount() == 1) {
        for(size_t i = 0; i < property->size(); ++i)
            result.append(scalarValueAt(property.get(), src, i));
    }
    else {
        for(size_t i = 0; i < property->size(); ++i) {
            py::list row;
            const char* rowBase = src + i * property->stride();
            for(size_t c = 0; c < property->componentCount(); ++c)
                row.append(scalarValueAt(property.get(), rowBase, c));
            result.append(std::move(row));
        }
    }

    return result;
}

void writeSequenceToProperty(Property* property, const SequenceData& data)
{
    if(static_cast<qsizetype>(property->size()) != data.rows)
        throw Exception(QStringLiteral("Input sequence length (%1) does not match property length (%2).").arg(data.rows).arg(property->size()));
    if(static_cast<qsizetype>(property->componentCount()) != data.components)
        throw Exception(QStringLiteral("Input component count (%1) does not match property component count (%2).").arg(data.components).arg(property->componentCount()));

    Ovito::detail::BufferAccessUntyped<DataBuffer, true, access_mode::read_write> access{PropertyPtr(property)};
    char* dest = reinterpret_cast<char*>(access.data());

    for(size_t i = 0; i < property->size(); ++i) {
        char* rowBase = dest + i * property->stride();
        for(size_t c = 0; c < property->componentCount(); ++c) {
            const double value = data.values[i * property->componentCount() + c];
            switch(property->dataType()) {
            case DataBuffer::Int8:
                reinterpret_cast<int8_t*>(rowBase)[c] = static_cast<int8_t>(value);
                break;
            case DataBuffer::Int32:
                reinterpret_cast<int32_t*>(rowBase)[c] = static_cast<int32_t>(value);
                break;
            case DataBuffer::Int64:
                reinterpret_cast<int64_t*>(rowBase)[c] = static_cast<int64_t>(value);
                break;
            case DataBuffer::Float32:
                reinterpret_cast<float*>(rowBase)[c] = static_cast<float>(value);
                break;
            case DataBuffer::Float64:
                reinterpret_cast<double*>(rowBase)[c] = value;
                break;
            default:
                throw Exception(QStringLiteral("Unsupported property data type."));
            }
        }
    }
}

int numpyDTypeToBufferType(const py::dtype& dtype)
{
    if(dtype.is(py::dtype::of<int8_t>()))
        return DataBuffer::Int8;
    if(dtype.is(py::dtype::of<bool>()))
        return DataBuffer::Int8;
    if(dtype.is(py::dtype::of<int32_t>()))
        return DataBuffer::Int32;
    if(dtype.is(py::dtype::of<int64_t>()))
        return DataBuffer::Int64;
    if(dtype.is(py::dtype::of<float>()))
        return DataBuffer::Float32;
    if(dtype.is(py::dtype::of<double>()))
        return DataBuffer::Float64;
    return 0;
}

int inferBufferType(py::handle dataHandle)
{
    py::module numpy = py::module::import("numpy");
    py::array array = numpy.attr("asarray")(dataHandle).cast<py::array>();
    int type = numpyDTypeToBufferType(array.dtype());
    if(type == 0) {
        if(py::isinstance<py::bool_>(dataHandle))
            return DataBuffer::Int8;
        return DataBuffer::Float64;
    }
    return type;
}

py::array ensureArray(py::handle dataHandle, int dataType)
{
    py::module numpy = py::module::import("numpy");
    py::dtype dtype = bufferDType(dataType);
    return numpy.attr("asarray")(dataHandle, dtype).cast<py::array>();
}

template<typename RefType>
py::capsule makeRefCapsule(RefType ref)
{
    return py::capsule(new RefType(std::move(ref)), [](void* ptr) {
        delete static_cast<RefType*>(ptr);
    });
}

py::array propertyArrayView(const ConstPropertyPtr& property)
{
    RawBufferReadAccessAndRef access(property);
    py::dtype dtype = bufferDType(property->dataType());
    py::capsule base = makeRefCapsule(property);
    py::array array;
    if(property->componentCount() == 1) {
        array = py::array(dtype, { static_cast<py::ssize_t>(property->size()) },
                          { static_cast<py::ssize_t>(property->stride()) },
                          access.cdata(), base);
    }
    else {
        array = py::array(dtype,
                          { static_cast<py::ssize_t>(property->size()), static_cast<py::ssize_t>(property->componentCount()) },
                          { static_cast<py::ssize_t>(property->stride()), static_cast<py::ssize_t>(property->dataTypeSize()) },
                          access.cdata(), base);
    }
    reinterpret_cast<py::detail::PyArray_Proxy*>(array.ptr())->flags &= ~py::detail::npy_api::NPY_ARRAY_WRITEABLE_;
    return array;
}

py::array propertyArrayView(const PropertyPtr& property)
{
    Ovito::detail::BufferAccessUntyped<DataBuffer, true, access_mode::read_write> access(property);
    py::dtype dtype = bufferDType(property->dataType());
    py::capsule base = makeRefCapsule(property);
    if(property->componentCount() == 1) {
        return py::array(dtype, { static_cast<py::ssize_t>(property->size()) },
                         { static_cast<py::ssize_t>(property->stride()) },
                         access.data(), base);
    }
    return py::array(dtype,
                     { static_cast<py::ssize_t>(property->size()), static_cast<py::ssize_t>(property->componentCount()) },
                     { static_cast<py::ssize_t>(property->stride()), static_cast<py::ssize_t>(property->dataTypeSize()) },
                     access.data(), base);
}

py::array propertyArrayCopy(const ConstPropertyPtr& property)
{
    py::module numpy = py::module::import("numpy");
    return numpy.attr("array")(propertyList(property), py::arg("dtype") = bufferDType(property->dataType())).cast<py::array>();
}

py::array propertyArrayCopy(const PropertyPtr& property)
{
    return propertyArrayCopy(ConstPropertyPtr(property));
}

void copyArrayIntoProperty(Property* property, py::array array)
{
    py::module::import("numpy").attr("copyto")(propertyArrayView(PropertyPtr(property)), array);
}

py::array normalizedArrayInput(py::handle dataHandle, int requestedBufferType)
{
    py::array array = ensureArray(dataHandle, requestedBufferType);
    if(array.ndim() != 1 && array.ndim() != 2)
        throw Exception(QStringLiteral("Expected a one- or two-dimensional NumPy-compatible array."));
    return array;
}

qsizetype arrayElementCount(const py::array& array)
{
    return array.shape(0);
}

qsizetype arrayComponentCount(const py::array& array)
{
    return (array.ndim() == 1) ? 1 : array.shape(1);
}

QStringList componentNamesFromPython(py::object obj)
{
    if(obj.is_none())
        return {};
    return obj.cast<QStringList>();
}

ConstPropertyPtr findPropertyRef(const PropertyContainer& container, const QString& name)
{
    for(const auto& prop : container.properties()) {
        if(prop->name() == name)
            return prop;
    }
    return {};
}

const DataTable* findTable(const DataCollection& data, const QString& name)
{
    for(const DataObject* object : data.objects()) {
        const DataTable* table = dynamic_object_cast<DataTable>(object);
        if(table && tableLookupKey(*table) == name)
            return table;
    }
    return nullptr;
}

DataTable* findMutableTable(DataCollection& data, const QString& name)
{
    if(const DataTable* table = findTable(data, name))
        return data.makeMutable<DataTable>(table);
    return nullptr;
}

DataTable::PlotMode parsePlotMode(const QString& mode)
{
    if(mode.compare(QStringLiteral("line"), Qt::CaseInsensitive) == 0)
        return DataTable::Line;
    if(mode.compare(QStringLiteral("histogram"), Qt::CaseInsensitive) == 0)
        return DataTable::Histogram;
    if(mode.compare(QStringLiteral("bar"), Qt::CaseInsensitive) == 0 || mode.compare(QStringLiteral("barchart"), Qt::CaseInsensitive) == 0)
        return DataTable::BarChart;
    if(mode.compare(QStringLiteral("scatter"), Qt::CaseInsensitive) == 0)
        return DataTable::Scatter;
    if(mode.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0)
        return DataTable::None;
    throw Exception(QStringLiteral("Unsupported plot mode '%1'. Supported values are: line, histogram, bar, scatter, none.").arg(mode));
}

class ModifierAttributesView
{
public:
    ModifierAttributesView(DataOORef<const DataCollection> owner, DataCollection* writable, OOWeakRef<const PipelineNode> createdByNode) :
        _owner(std::move(owner)), _writable(writable), _createdByNode(std::move(createdByNode)) {}

    py::list keys() const {
        py::list result;
        for(const DataObject* object : _owner->objects()) {
            if(const auto* attr = dynamic_object_cast<AttributeDataObject>(object))
                result.append(py::cast(attr->identifier()));
        }
        return result;
    }

    py::object getItem(const QString& key) const {
        for(const DataObject* object : _owner->objects()) {
            if(const auto* attr = dynamic_object_cast<AttributeDataObject>(object); attr && attr->identifier() == key)
                return py::cast(attr->value());
        }
        throw py::key_error(key.toStdString());
    }

    void setItem(const QString& key, py::object value) {
        if(!_writable)
            throw Exception(QStringLiteral("This attributes view is read-only."));
        _writable->setAttribute(key, value.cast<QVariant>(), _createdByNode);
    }

    void delItem(const QString& key) {
        if(!_writable)
            throw Exception(QStringLiteral("This attributes view is read-only."));
        for(const DataObject* object : _owner->objects()) {
            if(const auto* attr = dynamic_object_cast<AttributeDataObject>(object); attr && attr->identifier() == key) {
                _writable->removeObject(attr);
                return;
            }
        }
        throw py::key_error(key.toStdString());
    }

    qsizetype size() const {
        qsizetype count = 0;
        for(const DataObject* object : _owner->objects()) {
            if(dynamic_object_cast<AttributeDataObject>(object))
                ++count;
        }
        return count;
    }

private:
    DataOORef<const DataCollection> _owner;
    DataCollection* _writable = nullptr;
    OOWeakRef<const PipelineNode> _createdByNode;
};

class ModifierPropertyContainerView
{
public:
    ModifierPropertyContainerView(DataOORef<const DataObject> owner, const PropertyContainer* container, PropertyContainer* writable) :
        _owner(std::move(owner)), _container(container), _writable(writable) {}

    py::list keys() const {
        py::list result;
        for(const auto& prop : _container->properties())
            result.append(py::cast(prop->name()));
        return result;
    }

    qsizetype count() const { return static_cast<qsizetype>(_container->elementCount()); }

    bool contains(const QString& name) const { return _container->getProperty(name) != nullptr; }

    py::object get(const QString& name, py::object defaultValue) const {
        if(ConstPropertyPtr prop = findPropertyRef(*_container, name))
            return propertyArrayCopy(prop);
        return defaultValue;
    }

    py::object getItem(const QString& name) const {
        if(ConstPropertyPtr prop = findPropertyRef(*_container, name))
            return propertyArrayCopy(prop);
        throw py::key_error(name.toStdString());
    }

    py::object createProperty(const QString& name, py::object data, py::object dtypeObj, int components, py::object componentNamesObj) {
        if(!_writable)
            throw Exception(QStringLiteral("This property container is read-only."));
        Q_UNUSED(dtypeObj);

        Property* property = nullptr;
        if(const Property* existing = _container->getProperty(name)) {
            property = _writable->makePropertyMutable(existing, DataBuffer::Initialized);
        }
        else {
            if(data.is_none())
                throw Exception(QStringLiteral("Creating a new property requires an initial data array."));

            const int bufferType = dtypeObj.is_none() ? inferBufferType(data) : dtypeObj.cast<int>();
            py::array normalizedInput = normalizedArrayInput(data, bufferType);
            const qsizetype rows = arrayElementCount(normalizedInput);
            const qsizetype componentsFromData = arrayComponentCount(normalizedInput);

            if(_writable->elementCount() == 0)
                _writable->setElementCount(rows);
            else if(static_cast<qsizetype>(_writable->elementCount()) != rows)
                throw Exception(QStringLiteral("Input array length (%1) does not match the number of elements in the target container (%2).")
                                    .arg(rows).arg(_writable->elementCount()));

            int standardTypeId = _writable->getOOMetaClass().standardPropertyTypeId(name);
            if(standardTypeId != 0) {
                property = _writable->createProperty(DataBuffer::Uninitialized, standardTypeId);
            }
            else {
                int componentCount = components > 0 ? components : static_cast<int>(componentsFromData);
                property = _writable->createProperty(DataBuffer::Uninitialized, name, bufferType, componentCount, componentNamesFromPython(componentNamesObj));
            }
            copyArrayIntoProperty(property, normalizedInput);
            return propertyArrayCopy(ConstPropertyPtr(property));
        }

        if(data.is_none())
            return propertyArrayCopy(ConstPropertyPtr(property));

        const int bufferType = dtypeObj.is_none() ? property->dataType() : dtypeObj.cast<int>();
        py::array normalizedInput = normalizedArrayInput(data, bufferType);
        if(arrayElementCount(normalizedInput) != static_cast<qsizetype>(property->size()))
            throw Exception(QStringLiteral("Input array length (%1) does not match property length (%2).").arg(arrayElementCount(normalizedInput)).arg(property->size()));
        if(arrayComponentCount(normalizedInput) != static_cast<qsizetype>(property->componentCount()))
            throw Exception(QStringLiteral("Input component count (%1) does not match property component count (%2).").arg(arrayComponentCount(normalizedInput)).arg(property->componentCount()));
        copyArrayIntoProperty(property, normalizedInput);
        return propertyArrayCopy(ConstPropertyPtr(property));
    }

    void deleteProperty(const QString& name) {
        if(!_writable)
            throw Exception(QStringLiteral("This property container is read-only."));
        if(const Property* property = _writable->getProperty(name)) {
            _writable->removeProperty(property);
            return;
        }
        throw py::key_error(name.toStdString());
    }

protected:
    DataOORef<const DataObject> _owner;
    const PropertyContainer* _container = nullptr;
    PropertyContainer* _writable = nullptr;
};

class ModifierParticlesView : public ModifierPropertyContainerView
{
public:
    ModifierParticlesView(DataOORef<const DataObject> owner, const Particles* particles, Particles* writable) :
        ModifierPropertyContainerView(std::move(owner), particles, writable) {}
};

class ModifierDataTableView : public ModifierPropertyContainerView
{
public:
    ModifierDataTableView(DataOORef<const DataObject> owner, const DataTable* table, DataTable* writable) :
        ModifierPropertyContainerView(std::move(owner), table, writable), _table(table), _writableTable(writable) {}

    py::object x() const {
        if(ConstPropertyPtr xprop = _table->getXValues())
            return propertyArrayCopy(xprop);
        throw Exception(QStringLiteral("This data table does not have X values."));
    }

    py::object y() const {
        if(const Property* prop = _table->y()) {
            ConstPropertyPtr property(prop);
            return propertyArrayCopy(property);
        }
        return py::none();
    }

    QString title() const { return _table->title(); }
    QString axisLabelX() const { return _table->axisLabelX(); }
    QString axisLabelY() const { return _table->axisLabelY(); }

    void setTitle(const QString& value) {
        if(!_writableTable)
            throw Exception(QStringLiteral("This data table is read-only."));
        _writableTable->setTitle(value);
    }

    void setAxisLabelX(const QString& value) {
        if(!_writableTable)
            throw Exception(QStringLiteral("This data table is read-only."));
        _writableTable->setAxisLabelX(value);
    }

    void setAxisLabelY(const QString& value) {
        if(!_writableTable)
            throw Exception(QStringLiteral("This data table is read-only."));
        _writableTable->setAxisLabelY(value);
    }

private:
    const DataTable* _table = nullptr;
    DataTable* _writableTable = nullptr;
};

class ModifierTablesView
{
public:
    ModifierTablesView(DataOORef<const DataCollection> owner, DataCollection* writable, OOWeakRef<const PipelineNode> createdByNode) :
        _owner(std::move(owner)), _writable(writable), _createdByNode(std::move(createdByNode)) {}

    py::list keys() const {
        py::list result;
        for(const DataObject* object : _owner->objects()) {
            if(const auto* table = dynamic_object_cast<DataTable>(object))
                result.append(py::cast(tableLookupKey(*table)));
        }
        return result;
    }

    qsizetype size() const {
        qsizetype count = 0;
        for(const DataObject* object : _owner->objects()) {
            if(dynamic_object_cast<DataTable>(object))
                ++count;
        }
        return count;
    }

    py::object get(const QString& name, py::object defaultValue) const {
        if(const DataTable* table = findTable(*_owner, name))
            return py::cast(ModifierDataTableView(_owner, table, _writable ? findMutableTable(*_writable, name) : nullptr));
        return defaultValue;
    }

    py::object getItem(const QString& name) const {
        if(const DataTable* table = findTable(*_owner, name))
            return py::cast(ModifierDataTableView(_owner, table, _writable ? findMutableTable(*_writable, name) : nullptr));
        throw py::key_error(name.toStdString());
    }

    py::object create(const QString& identifier, py::object xData, py::object yData, const QString& plotMode,
                      const QString& title, const QString& axisLabelX, const QString& axisLabelY,
                      py::object yComponentNames) {
        if(!_writable)
            throw Exception(QStringLiteral("This tables view is read-only."));

        DataTable* existing = findMutableTable(*_writable, identifier);
        DataOORef<DataTable> tableRef;
        DataTable* table = nullptr;
        if(existing) {
            table = existing;
            tableRef = DataOORef<DataTable>(table);
            table->setCreatedByNode(_createdByNode);
            table->setPlotMode(parsePlotMode(plotMode));
            table->setTitle(title.isEmpty() ? identifier : title);
        }
        else {
            tableRef = DataOORef<DataTable>::create(ObjectInitializationFlag::DontCreateVisElement, parsePlotMode(plotMode), title.isEmpty() ? identifier : title);
            table = tableRef.get();
            table->setIdentifier(identifier);
            table->setCreatedByNode(_createdByNode);
            _writable->addObject(tableRef);
        }

        auto fillAxisProperty = [&](const char* propertyName, py::object values, bool isX, QStringList componentNames = {}) {
            if(values.is_none())
                return;
            const int bufferType = inferBufferType(values);
            py::array normalizedInput = normalizedArrayInput(values, bufferType);
            const qsizetype rowCount = arrayElementCount(normalizedInput);
            const qsizetype componentCount = arrayComponentCount(normalizedInput);
            if(isX && componentCount != 1)
                throw Exception(QStringLiteral("X values must be a one-dimensional numeric sequence."));
            if(table->elementCount() == 0)
                table->setElementCount(rowCount);
            else if(static_cast<qsizetype>(table->elementCount()) != rowCount)
                throw Exception(QStringLiteral("Input array length (%1) does not match the target table row count (%2).")
                                    .arg(rowCount).arg(table->elementCount()));
            Property* prop = table->createProperty(DataBuffer::Uninitialized, QString::fromLatin1(propertyName), bufferType, componentCount, std::move(componentNames));
            copyArrayIntoProperty(prop, normalizedInput);
            if(isX)
                table->setX(prop);
            else
                table->setY(prop);
        };
        fillAxisProperty("X", xData, true);
        fillAxisProperty("Y", yData, false, componentNamesFromPython(yComponentNames));
        if(!axisLabelX.isEmpty())
            table->setAxisLabelX(axisLabelX);
        if(!axisLabelY.isEmpty())
            table->setAxisLabelY(axisLabelY);
        return py::cast(ModifierDataTableView(_owner, table, table));
    }

    void delItem(const QString& name) {
        if(!_writable)
            throw Exception(QStringLiteral("This tables view is read-only."));
        if(const DataTable* table = findTable(*_writable, name)) {
            _writable->removeObject(table);
            return;
        }
        throw py::key_error(name.toStdString());
    }

private:
    DataOORef<const DataCollection> _owner;
    DataCollection* _writable = nullptr;
    OOWeakRef<const PipelineNode> _createdByNode;
};

class ModifierDataObjectView
{
public:
    ModifierDataObjectView(DataOORef<const DataCollection> owner, DataCollection* writable, const DataObject* object, qsizetype index) :
        _owner(std::move(owner)), _writable(writable), _object(object), _index(index) {}

    QString key() const { return objectLookupKey(*_object, _index); }
    QString identifier() const { return _object->identifier(); }
    QString title() const { return _object->objectTitle(); }
    QString objectType() const { return _object->getOOClass().name(); }
    qsizetype index() const { return _index; }

    void remove() {
        if(!_writable)
            throw Exception(QStringLiteral("This data object view is read-only."));
        _writable->removeObject(_object);
    }

    const DataObject* object() const { return _object; }

private:
    DataOORef<const DataCollection> _owner;
    DataCollection* _writable = nullptr;
    const DataObject* _object = nullptr;
    qsizetype _index = -1;
};

py::object wrapDataObject(DataOORef<const DataCollection> owner, DataCollection* writable, const DataObject* object, qsizetype index)
{
    return py::cast(ModifierDataObjectView(std::move(owner), writable, object, index));
}

class ModifierObjectsView
{
public:
    ModifierObjectsView(DataOORef<const DataCollection> owner, DataCollection* writable) :
        _owner(std::move(owner)), _writable(writable) {}

    qsizetype size() const { return static_cast<qsizetype>(_owner->objects().size()); }

    py::list keys() const {
        py::list result;
        for(qsizetype index = 0; index < size(); ++index)
            result.append(py::cast(objectLookupKey(*_owner->getObject(index), index)));
        return result;
    }

    py::object get(py::object key, py::object defaultValue) const {
        try {
            return getItem(key);
        }
        catch(const py::key_error&) {
            return defaultValue;
        }
        catch(const py::index_error&) {
            return defaultValue;
        }
    }

    py::object getItem(py::object key) const {
        if(py::isinstance<py::int_>(key)) {
            const qsizetype index = key.cast<qsizetype>();
            if(index < 0 || index >= size())
                throw py::index_error();
            return wrapDataObject(_owner, _writable, _owner->getObject(index), index);
        }

        const QString name = key.cast<QString>();
        for(qsizetype index = 0; index < size(); ++index) {
            const DataObject* object = _owner->getObject(index);
            if(objectLookupKey(*object, index) == name)
                return wrapDataObject(_owner, _writable, object, index);
        }
        throw py::key_error(name.toStdString());
    }

private:
    DataOORef<const DataCollection> _owner;
    DataCollection* _writable = nullptr;
};

class ModifierDataCollectionView
{
public:
    ModifierDataCollectionView(DataOORef<const DataCollection> owner, DataCollection* writable, OOWeakRef<const PipelineNode> createdByNode) :
        _owner(std::move(owner)), _writable(writable), _createdByNode(std::move(createdByNode)) {}

    ModifierAttributesView attributes() const { return ModifierAttributesView(_owner, _writable, _createdByNode); }
    ModifierTablesView tables() const { return ModifierTablesView(_owner, _writable, _createdByNode); }
    ModifierObjectsView objects() const { return ModifierObjectsView(_owner, _writable); }

    py::object particles() const {
        if(!_owner)
            return py::none();
        const Particles* particles = _owner->getObject<Particles>();
        if(!particles)
            return py::none();
        Particles* writableParticles = _writable ? _writable->getMutableObject<Particles>() : nullptr;
        return py::cast(ModifierParticlesView(_owner, particles, writableParticles));
    }

    py::object createTable(const QString& identifier, py::object xData, py::object yData, const QString& plotMode, const QString& title) {
        return tables().create(identifier, xData, yData, plotMode, title, QString{}, QString{}, py::none());
    }

    py::object createTableEx(const QString& identifier, py::object xData, py::object yData, const QString& plotMode,
                             const QString& title, const QString& axisLabelX, const QString& axisLabelY, py::object yComponentNames) {
        return tables().create(identifier, xData, yData, plotMode, title, axisLabelX, axisLabelY, yComponentNames);
    }

    void remove(py::object target) {
        if(!_writable)
            throw Exception(QStringLiteral("This data collection view is read-only."));

        if(py::isinstance<py::int_>(target)) {
            const qsizetype index = target.cast<qsizetype>();
            if(index < 0 || index >= static_cast<qsizetype>(_writable->objects().size()))
                throw py::index_error();
            _writable->removeObjectByIndex(index);
            return;
        }

        if(py::isinstance<py::str>(target)) {
            const QString name = target.cast<QString>();
            for(qsizetype index = 0; index < static_cast<qsizetype>(_writable->objects().size()); ++index) {
                const DataObject* object = _writable->getObject(index);
                if(objectLookupKey(*object, index) == name) {
                    _writable->removeObject(object);
                    return;
                }
            }
            throw py::key_error(name.toStdString());
        }

        if(py::isinstance<ModifierDataObjectView>(target)) {
            py::cast<ModifierDataObjectView>(target).remove();
            return;
        }

        throw Exception(QStringLiteral("Unsupported object handle passed to data.remove()."));
    }

private:
    DataOORef<const DataCollection> _owner;
    DataCollection* _writable = nullptr;
    OOWeakRef<const PipelineNode> _createdByNode;
};

class ModifierUpstream
{
public:
    ModifierUpstream(OOWeakRef<const PipelineNode> node, int currentFrame, bool interactiveMode) :
        _node(std::move(node)), _currentFrame(currentFrame), _interactiveMode(interactiveMode) {}

    int currentFrame() const { return _currentFrame; }

    int numFrames() const {
        if(OORef<const PipelineNode> node = _node.lock())
            return node->numberOfSourceFrames();
        return 0;
    }

    py::object compute(py::object frameObj) const {
        OORef<const PipelineNode> node = _node.lock();
        if(!node)
            throw Exception(QStringLiteral("The upstream pipeline is no longer available."));

        int frame = frameObj.is_none() ? _currentFrame : frameObj.cast<int>();
        const AnimationTime frameTime = AnimationTime::fromFrame(frame);
        // Upstream frame sampling performed from inside a Python modifier should not
        // disturb the interactive viewport state of the live scene. These helper
        // evaluations are analysis reads, so always request a non-interactive result.
        //
        // If this wrapper was created for a modifier evaluation, request the
        // historical input through the owning ModificationNode. That path is
        // specifically designed for modifiers which need their upstream data at
        // other animation times.
        PipelineEvaluationRequest request(frameTime, true, false);
        PipelineEvaluationResult singleResult;
        Future<std::vector<PipelineFlowState>> resultFuture;
        bool usedSingleResult = false;
        if(const auto* modificationNode = dynamic_object_cast<const ModificationNode>(node.get())) {
            singleResult = modificationNode->evaluateInput(request);
            usedSingleResult = true;
        }
        else {
            // Fall back to the generic pipeline-node helper for non-modifier nodes.
            resultFuture = const_cast<PipelineNode*>(node.get())->evaluateMultiple(request, { frameTime });
        }
        if(usedSingleResult) {
            bool completed = false;
            {
                py::gil_scoped_release release;
                completed = ScriptEngine::waitForFuture(singleResult);
            }
            if(!completed) {
                PyErr_SetString(PyExc_KeyboardInterrupt, "Operation has been canceled by the user.");
                throw py::error_already_set();
            }
            return createModifierDataWrapper(singleResult.result().data(), false);
        }
        bool completed = false;
        {
            py::gil_scoped_release release;
            completed = ScriptEngine::waitForFuture(resultFuture);
        }
        if(!completed) {
            PyErr_SetString(PyExc_KeyboardInterrupt, "Operation has been canceled by the user.");
            throw py::error_already_set();
        }
        const std::vector<PipelineFlowState>& results = resultFuture.result();
        if(results.empty())
            throw Exception(QStringLiteral("The upstream pipeline did not return a result."));
        return createModifierDataWrapper(results.front().data(), false);
    }

private:
    OOWeakRef<const PipelineNode> _node;
    int _currentFrame = 0;
    bool _interactiveMode = false;
};

class ModifierProgress
{
public:
    explicit ModifierProgress(std::shared_ptr<TaskProgress> progress, std::function<void(const QString&)> statusCallback = {}, std::function<bool()> cancelRequestedCallback = {}) :
        _progress(std::move(progress)), _statusCallback(std::move(statusCallback)), _cancelRequestedCallback(std::move(cancelRequestedCallback)),
        _cancelEpoch(g_pythonModifierCancelEpoch.load(std::memory_order_acquire)) {}

    void text(const QString& message) const {
        throwIfCanceled();
        if(_progress)
            _progress->setText(message);
        _lastText = message;
        publishStatus();
        throwIfCanceled();
    }

    void fraction(double value) const {
        throwIfCanceled();
        if(_progress) {
            _progress->setMaximum(1000);
            _progress->setValue(static_cast<qlonglong>(qBound(0.0, value, 1.0) * 1000.0));
        }
        _lastFraction = qBound(0.0, value, 1.0);
        publishStatus();
        throwIfCanceled();
    }

    void value(qlonglong current, qlonglong maximum = -1) const {
        throwIfCanceled();
        if(_progress) {
            if(maximum >= 0)
                _progress->setMaximum(maximum, false);
            _progress->setValue(current);
        }
        throwIfCanceled();
    }

    void checkCanceled() const { throwIfCanceled(); }

private:
    void throwIfCanceled() const
    {
        if(g_pythonModifierCancelEpoch.load(std::memory_order_acquire) != _cancelEpoch)
            throwPythonCanceled();
        if(_cancelRequestedCallback && _cancelRequestedCallback())
            throwPythonCanceled();
        try {
            this_task::throwIfCanceled();
        }
        catch(const OperationCanceled&) {
            throwPythonCanceled();
        }
    }

    void publishStatus() const
    {
        if(!_statusCallback)
            return;

        QString status = _lastText;
        if(_lastFraction >= 0.0) {
            const int percent = static_cast<int>(std::round(_lastFraction * 100.0));
            if(status.isEmpty())
                status = QStringLiteral("Progress: %1%").arg(percent);
            else
                status += QStringLiteral(" (%1%)").arg(percent);
        }
        if(!status.isEmpty())
            _statusCallback(status);

        if(QCoreApplication::instance()) {
            QCoreApplication::sendPostedEvents(nullptr, 0);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        }
    }

    std::shared_ptr<TaskProgress> _progress;
    std::function<void(const QString&)> _statusCallback;
    std::function<bool()> _cancelRequestedCallback;
    quint64 _cancelEpoch = 0;
    mutable QString _lastText;
    mutable double _lastFraction = -1.0;
};

} // namespace

quint64 currentPythonModifierCancelEpoch() noexcept
{
    return g_pythonModifierCancelEpoch.load(std::memory_order_acquire);
}

void advancePythonModifierCancelEpoch() noexcept
{
    g_pythonModifierCancelEpoch.fetch_add(1, std::memory_order_acq_rel);
}

void defineModifierRuntimeBindings(py::module m)
{
    m.attr("_modifier_runtime_caches") = py::dict();

    py::class_<ModifierAttributesView>(m, "Attributes")
        .def("__len__", &ModifierAttributesView::size)
        .def("__iter__", [](const ModifierAttributesView& self) { return self.keys().attr("__iter__")(); }, py::keep_alive<0, 1>())
        .def("keys", &ModifierAttributesView::keys)
        .def("__getitem__", &ModifierAttributesView::getItem)
        .def("__setitem__", &ModifierAttributesView::setItem)
        .def("__delitem__", &ModifierAttributesView::delItem);

    py::class_<ModifierParticlesView>(m, "Particles")
        .def_property_readonly("count", &ModifierPropertyContainerView::count)
        .def("__len__", &ModifierPropertyContainerView::count)
        .def("keys", &ModifierPropertyContainerView::keys)
        .def("__contains__", &ModifierPropertyContainerView::contains)
        .def("__getitem__", &ModifierPropertyContainerView::getItem)
        .def("get", &ModifierPropertyContainerView::get, py::arg("name"), py::arg("default") = py::none())
        .def("create_property", &ModifierPropertyContainerView::createProperty,
             py::arg("name"), py::arg("data") = py::none(), py::kw_only(),
             py::arg("dtype") = py::none(), py::arg("components") = 0, py::arg("component_names") = py::none())
        .def("delete_property", &ModifierPropertyContainerView::deleteProperty);

    py::class_<ModifierDataTableView>(m, "DataTable")
        .def("keys", &ModifierPropertyContainerView::keys)
        .def("__contains__", &ModifierPropertyContainerView::contains)
        .def("__getitem__", &ModifierPropertyContainerView::getItem)
        .def("get", &ModifierPropertyContainerView::get, py::arg("name"), py::arg("default") = py::none())
        .def("create_property", &ModifierPropertyContainerView::createProperty,
             py::arg("name"), py::arg("data") = py::none(), py::kw_only(),
             py::arg("dtype") = py::none(), py::arg("components") = 0, py::arg("component_names") = py::none())
        .def_property_readonly("x", &ModifierDataTableView::x)
        .def_property_readonly("y", &ModifierDataTableView::y)
        .def_property("title", &ModifierDataTableView::title, &ModifierDataTableView::setTitle)
        .def_property("axis_label_x", &ModifierDataTableView::axisLabelX, &ModifierDataTableView::setAxisLabelX)
        .def_property("axis_label_y", &ModifierDataTableView::axisLabelY, &ModifierDataTableView::setAxisLabelY);

    py::class_<ModifierTablesView>(m, "Tables")
        .def("__len__", &ModifierTablesView::size)
        .def("__iter__", [](const ModifierTablesView& self) { return self.keys().attr("__iter__")(); }, py::keep_alive<0, 1>())
        .def("keys", &ModifierTablesView::keys)
        .def("__getitem__", &ModifierTablesView::getItem)
        .def("get", &ModifierTablesView::get, py::arg("name"), py::arg("default") = py::none())
        .def("create", &ModifierTablesView::create,
             py::arg("name"), py::arg("x") = py::none(), py::arg("y") = py::none(),
             py::kw_only(), py::arg("plot_mode") = QStringLiteral("line"), py::arg("title") = QString(),
             py::arg("axis_label_x") = QString(), py::arg("axis_label_y") = QString(),
             py::arg("y_component_names") = py::none())
        .def("__delitem__", &ModifierTablesView::delItem);

    py::class_<ModifierDataObjectView>(m, "DataObject")
        .def_property_readonly("key", &ModifierDataObjectView::key)
        .def_property_readonly("identifier", &ModifierDataObjectView::identifier)
        .def_property_readonly("title", &ModifierDataObjectView::title)
        .def_property_readonly("object_type", &ModifierDataObjectView::objectType)
        .def_property_readonly("index", &ModifierDataObjectView::index)
        .def("remove", &ModifierDataObjectView::remove);

    py::class_<ModifierObjectsView>(m, "DataObjects")
        .def("__len__", &ModifierObjectsView::size)
        .def("__iter__", [](const ModifierObjectsView& self) { return self.keys().attr("__iter__")(); }, py::keep_alive<0, 1>())
        .def("keys", &ModifierObjectsView::keys)
        .def("__getitem__", &ModifierObjectsView::getItem)
        .def("get", &ModifierObjectsView::get, py::arg("key"), py::arg("default") = py::none());

    py::class_<ModifierDataCollectionView>(m, "DataCollection")
        .def_property_readonly("attributes", &ModifierDataCollectionView::attributes)
        .def_property_readonly("particles", &ModifierDataCollectionView::particles)
        .def_property_readonly("tables", &ModifierDataCollectionView::tables)
        .def_property_readonly("objects", &ModifierDataCollectionView::objects)
        .def("create_table", &ModifierDataCollectionView::createTableEx,
             py::arg("name"), py::arg("x") = py::none(), py::arg("y") = py::none(),
             py::kw_only(), py::arg("plot_mode") = QStringLiteral("line"), py::arg("title") = QString(),
             py::arg("axis_label_x") = QString(), py::arg("axis_label_y") = QString(),
             py::arg("y_component_names") = py::none())
        .def("remove", &ModifierDataCollectionView::remove);

    py::class_<ModifierUpstream>(m, "Upstream")
        .def_property_readonly("current_frame", &ModifierUpstream::currentFrame)
        .def_property_readonly("num_frames", &ModifierUpstream::numFrames)
        .def("compute", &ModifierUpstream::compute, py::arg("frame") = py::none());

    py::class_<ModifierProgress>(m, "Progress")
        .def("text", &ModifierProgress::text)
        .def("fraction", &ModifierProgress::fraction)
        .def("value", &ModifierProgress::value, py::arg("current"), py::arg("maximum") = -1)
        .def("check_canceled", &ModifierProgress::checkCanceled);
}

py::object createModifierDataWrapper(DataOORef<const DataCollection> data, bool writable, OOWeakRef<const PipelineNode> createdByNode)
{
    DataCollection* mutableData = writable ? const_cast<DataCollection*>(data.get()) : nullptr;
    return py::cast(ModifierDataCollectionView(std::move(data), mutableData, std::move(createdByNode)));
}

py::object createModifierUpstreamWrapper(OOWeakRef<const PipelineNode> upstreamNode, int currentFrame, bool interactiveMode)
{
    return py::cast(ModifierUpstream(std::move(upstreamNode), currentFrame, interactiveMode));
}

py::object createModifierProgressWrapper(std::shared_ptr<TaskProgress> progress, std::function<void(const QString&)> statusCallback, std::function<bool()> cancelRequestedCallback)
{
    return py::cast(ModifierProgress(std::move(progress), std::move(statusCallback), std::move(cancelRequestedCallback)));
}

py::dict getModifierCache(quintptr modifierKey, quint64 generation)
{
    py::dict caches = py::module::import("ovito_modifier_runtime").attr("_modifier_runtime_caches").cast<py::dict>();
    QString cacheKey = QStringLiteral("%1:%2").arg(modifierKey).arg(generation);
    py::object key = py::cast(cacheKey);
    if(!caches.contains(key))
        caches[key] = py::dict();
    return caches[key].cast<py::dict>();
}

} // namespace PyScript
