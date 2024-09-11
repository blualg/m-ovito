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


#include <ovito/stdobj/StdObj.h>
#include <ovito/stdobj/properties/PropertyContainer.h>

namespace Ovito {

/**
 * \brief A data object type that consists of a set of data columns, which are typically used to generate 2d data plots.
 */
class OVITO_STDOBJ_EXPORT DataTable : public PropertyContainer
{
    /// Define a new property metaclass for data table property containers.
    class OVITO_STDOBJ_EXPORT OOMetaClass : public PropertyContainerClass
    {
    public:

        /// Inherit constructor from base class.
        using PropertyContainerClass::PropertyContainerClass;

        /// Creates a storage object for standard data table properties.
        virtual PropertyPtr createStandardPropertyInternal(DataBuffer::BufferInitialization init, size_t elementCount, int type, const ConstDataObjectPath& containerPath) const override;

    protected:

        /// Is called by the system after construction of the meta-class instance.
        virtual void initialize() override;
    };

    OVITO_CLASS_META(DataTable, OOMetaClass);

public:

    enum PlotMode {
        None,
        Line,
        Histogram,
        BarChart,
        Scatter
    };
    Q_ENUM(PlotMode);

    /// Constructor.
    void initializeObject(ObjectInitializationFlags flags, PlotMode plotMode = Line, const QString& title = QString(), ConstPropertyPtr y = {}, ConstPropertyPtr x = {});

    /// Assigns a property array as x-coordinates of the data points (for the purpose of plotting).
    void setX(const Property* property);

    /// Assigns a property array as y-coordinates of the data points (for the purpose of plotting).
    void setY(const Property* property);

    /// Returns the data array containing the x-coordinates of the data points.
    /// If no explicit x-coordinate data is available, the array is dynamically generated
    /// from the x-axis interval set for this data table.
    ConstPropertyPtr getXValues() const;

    /// Indicates whether this data object wants to be shown in the pipeline editor under the data source section.
    virtual PipelineEditorObjectListMode pipelineEditorObjectListMode() const override {
        return PipelineEditorObjectListMode::Show; // Note: Not showing typed properties in the pipeline editor that belong to this DataTable.
    }

protected:

    /// From RefMaker.
    virtual void referenceRemoved(const PropertyFieldDescriptor* field, RefTarget* oldTarget, int listIndex) override;

private:

    /// The lower bound of the x-interval of the histogram if data points have no explicit x-coordinates.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, intervalStart, setIntervalStart);

    /// The upper bound of the x-interval of the histogram if data points have no explicit x-coordinates.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, intervalEnd, setIntervalEnd);

    /// The label of the x-axis (optional).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, axisLabelX, setAxisLabelX);

    /// The label of the y-axis (optional).
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString{}, axisLabelY, setAxisLabelY);

    /// The plotting mode for this data table.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PlotMode{Line}, plotMode, setPlotMode);
    DECLARE_SHADOW_PROPERTY_FIELD(plotMode);

    /// Property containing the X coordinates of data points for plotting.
    /// Note: Using OORef<> instead of DataOORef<> here, because the PropertyContainer already holds a strong reference to the same property object.
    /// Thus, there is no need to create a second strong reference here, because this would prevent modifying the property object even when it is exclusively owned by the data table.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<const Property>, x, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_SUB_ANIM);

    /// Property containing the Y coordinates of data points for plotting.
    /// Note: Using OORef<> instead of DataOORef<> here, because the PropertyContainer already holds a strong reference to the same property object.
    /// Thus, there is no need to create a second strong reference here, because this would prevent modifying the property object even when it is exclusively owned by the data table.
    DECLARE_REFERENCE_FIELD_FLAGS(OORef<const Property>, y, PROPERTY_FIELD_DONT_PROPAGATE_MESSAGES | PROPERTY_FIELD_NO_SUB_ANIM);
};

}   // End of namespace
