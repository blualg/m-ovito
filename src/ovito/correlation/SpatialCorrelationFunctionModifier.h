////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
//  Copyright 2017 Lars Pastewka
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
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/stdobj/properties/Property.h>
#include <ovito/stdobj/table/DataTable.h>
#include <ovito/particles/util/CutoffNeighborFinder.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/core/dataset/pipeline/Modifier.h>

#include <complex>

namespace Ovito {

/**
 * \brief This modifier computes the spatial correlation function between two particle properties.
 */
class OVITO_CORRELATIONFUNCTIONPLUGIN_EXPORT SpatialCorrelationFunctionModifier : public Modifier
{
    /// Give this modifier class its own metaclass.
    class OOMetaClass : public Modifier::OOMetaClass
    {
    public:

        /// Inherit constructor from base metaclass.
        using Modifier::OOMetaClass::OOMetaClass;

        /// Asks the metaclass whether the modifier can be applied to the given input data.
        virtual bool isApplicableTo(const DataCollection& input) const override;
    };

    OVITO_CLASS_META(SpatialCorrelationFunctionModifier, OOMetaClass)

public:

    enum AveragingDirectionType {
        CELL_VECTOR_1 = 0,
        CELL_VECTOR_2 = 1,
        CELL_VECTOR_3 = 2,
        RADIAL = 3
    };
    Q_ENUM(AveragingDirectionType);

    enum NormalizationType {
        VALUE_CORRELATION = 0,
        DIFFERENCE_CORRELATION = 1
    };
    Q_ENUM(NormalizationType);

    /// This method is called by the system after the modifier has been inserted into a data pipeline.
    virtual void initializeModifier(const ModifierInitializationRequest& request) override;

    /// Is called by the pipeline system before a new modifier evaluation begins.
    virtual void preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const override;

    /// Modifies the input data.
    virtual Future<PipelineFlowState> evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state) override;

    /// Indicates that a preliminary viewport update will be performed immediately after this modifier
	/// has computed new results.
    virtual bool shouldRefreshViewportsAfterEvaluation() override { return true; }

protected:

    /// Sends an event to all dependents of this RefTarget.
    virtual void notifyDependentsImpl(const ReferenceEvent& event) noexcept override;

    /// Indicates whether the modifier wants to keep its partial compute results after one of its parameters has been changed.
    virtual bool shouldKeepPartialResultsAfterChange(const PropertyFieldEvent& event) override {
        // Avoid a full recomputation if one of the plotting-related parameters of the modifier change.
        if(event.field() == PROPERTY_FIELD(fixRealSpaceXAxisRange) ||
                event.field() == PROPERTY_FIELD(fixRealSpaceYAxisRange) ||
                event.field() == PROPERTY_FIELD(realSpaceXAxisRangeStart) ||
                event.field() == PROPERTY_FIELD(realSpaceXAxisRangeEnd) ||
                event.field() == PROPERTY_FIELD(realSpaceYAxisRangeStart) ||
                event.field() == PROPERTY_FIELD(realSpaceYAxisRangeEnd) ||
                event.field() == PROPERTY_FIELD(fixReciprocalSpaceXAxisRange) ||
                event.field() == PROPERTY_FIELD(fixReciprocalSpaceYAxisRange) ||
                event.field() == PROPERTY_FIELD(reciprocalSpaceXAxisRangeStart) ||
                event.field() == PROPERTY_FIELD(reciprocalSpaceXAxisRangeEnd) ||
                event.field() == PROPERTY_FIELD(reciprocalSpaceYAxisRangeStart) ||
                event.field() == PROPERTY_FIELD(reciprocalSpaceYAxisRangeEnd) ||
                event.field() == PROPERTY_FIELD(normalizeRealSpace) ||
                event.field() == PROPERTY_FIELD(normalizeRealSpaceByRDF) ||
                event.field() == PROPERTY_FIELD(normalizeRealSpaceByCovariance) ||
                event.field() == PROPERTY_FIELD(normalizeReciprocalSpace) ||
                event.field() == PROPERTY_FIELD(typeOfRealSpacePlot) ||
                event.field() == PROPERTY_FIELD(typeOfReciprocalSpacePlot))
            return true;
        return Modifier::shouldKeepPartialResultsAfterChange(event);
    }

private:

    /// Computes the modifier's results.
    class CorrelationAnalysisEngine : private TaskProgress
    {
    public:

        /// Constructor.
        CorrelationAnalysisEngine(ConstPropertyPtr positions,
                                  ConstPropertyPtr sourceProperty1,
                                  size_t vecComponent1,
                                  ConstPropertyPtr sourceProperty2,
                                  size_t vecComponent2,
                                  const SimulationCell* simCell,
                                  FloatType fftGridSpacing,
                                  bool applyWindow,
                                  bool doComputeNeighCorrelation,
                                  FloatType neighCutoff,
                                  int numberOfNeighBins,
                                  AveragingDirectionType averagingDirection) :
            TaskProgress(this_task::ui()),
            _positions(std::move(positions)),
            _sourceProperty1(std::move(sourceProperty1)), _vecComponent1(vecComponent1),
            _sourceProperty2(std::move(sourceProperty2)), _vecComponent2(vecComponent2),
            _simCell(simCell), _fftGridSpacing(fftGridSpacing),
            _applyWindow(applyWindow), _neighCutoff(neighCutoff),
            _averagingDirection(averagingDirection),
            _neighCorrelation(doComputeNeighCorrelation ? DataTable::OOClass().createUserProperty(DataBuffer::Initialized, numberOfNeighBins, DataBuffer::FloatDefault, 1, tr("Neighbor C(r)")) : nullptr) {}

        /// Computes the modifier's results and stores them in this object for later retrieval.
        void perform();

        /// Injects the computed results into the data pipeline.
        void applyResults(PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode);

        /// Compute real and reciprocal space correlation function via FFT.
        void computeFftCorrelation();

        /// Compute real space correlation function via direction summation over neighbors.
        void computeNeighCorrelation();

        /// Compute means and covariance.
        void computeLimits();

        /// Returns the property storage that contains the input particle positions.
        const ConstPropertyPtr& positions() const { return _positions; }

        /// Returns the property storage that contains the first input particle property.
        const ConstPropertyPtr& sourceProperty1() const { return _sourceProperty1; }

        /// Returns the property storage that contains the second input particle property.
        const ConstPropertyPtr& sourceProperty2() const { return _sourceProperty2; }

        /// Returns the simulation cell data.
        const DataOORef<const SimulationCell>& cell() const { return _simCell; }

        /// Returns the FFT cutoff radius.
        FloatType fftGridSpacing() const { return _fftGridSpacing; }

        /// Returns the neighbor cutoff radius.
        FloatType neighCutoff() const { return _neighCutoff; }

        /// Returns the real-space correlation function.
        const PropertyPtr& realSpaceCorrelation() const { return _realSpaceCorrelation; }

        /// Returns the RDF evaluated from an FFT correlation.
        const PropertyPtr& realSpaceRDF() const { return _realSpaceRDF; }

        /// Returns the short-ranged real-space correlation function.
        const PropertyPtr& neighCorrelation() const { return _neighCorrelation; }

        /// Returns the RDF evaluated from a direct sum over neighbor shells.
        const PropertyPtr& neighRDF() const { return _neighRDF; }

        /// Returns the reciprocal-space correlation function.
        const PropertyPtr& reciprocalSpaceCorrelation() const { return _reciprocalSpaceCorrelation; }

        /// Returns the mean of the first property.
        FloatType mean1() const { return _mean1; }

        /// Returns the mean of the second property.
        FloatType mean2() const { return _mean2; }

        /// Returns the variance of the first property.
        FloatType variance1() const { return _variance1; }

        /// Returns the variance of the second property.
        FloatType variance2() const { return _variance2; }

        /// Returns the (co)variance.
        FloatType covariance() const { return _covariance; }

        void setMoments(FloatType mean1, FloatType mean2, FloatType variance1,
                        FloatType variance2, FloatType covariance) {
            _mean1 = mean1;
            _mean2 = mean2;
            _variance1 = variance1;
            _variance2 = variance2;
            _covariance = covariance;
        }

    private:

        /// Real-to-complex FFT.
        std::vector<std::complex<FloatType>> r2cFFT(int nX, int nY, int nZ, std::vector<FloatType>& rData);

        /// Complex-to-real inverse FFT
        std::vector<FloatType> c2rFFT(int nX, int nY, int nZ, std::vector<std::complex<FloatType>>& cData);

        /// Map property onto grid.
        std::vector<FloatType>  mapToSpatialGrid(const Property* property,
                              size_t propertyVectorComponent,
                              const AffineTransformation& reciprocalCell,
                              int nX, int nY, int nZ,
                              bool applyWindow);

        const size_t _vecComponent1;
        const size_t _vecComponent2;
        const FloatType _fftGridSpacing;
        const bool _applyWindow;
        const FloatType _neighCutoff;
        const AveragingDirectionType _averagingDirection;
        DataOORef<const SimulationCell> _simCell;
        ConstPropertyPtr _positions;
        ConstPropertyPtr _sourceProperty1;
        ConstPropertyPtr _sourceProperty2;

        PropertyPtr _realSpaceCorrelation;
        FloatType _realSpaceCorrelationRange;
        PropertyPtr _realSpaceRDF;
        PropertyPtr _neighCorrelation;
        PropertyPtr _neighRDF;
        PropertyPtr _reciprocalSpaceCorrelation;
        FloatType _reciprocalSpaceCorrelationRange;
        FloatType _mean1 = 0;
        FloatType _mean2 = 0;
        FloatType _variance1 = 0;
        FloatType _variance2 = 0;
        FloatType _covariance = 0;
    };

private:

    /// The particle property that serves as the first data source for the correlation function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, sourceProperty1, setSourceProperty1);
    /// The particle property that serves as the second data source for the correlation function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(PropertyReference{}, sourceProperty2, setSourceProperty2);
    /// Controls the cutoff radius for the FFT grid.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{3.0}, fftGridSpacing, setFFTGridSpacing);
    /// Controls if a windowing function should be applied in non-periodic directions.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{true}, applyWindow, setApplyWindow, PROPERTY_FIELD_MEMORIZE);
    /// Controls whether the real-space correlation should be computed by direct summation.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, doComputeNeighCorrelation, setComputeNeighCorrelation, PROPERTY_FIELD_MEMORIZE);
    /// Controls the cutoff radius for the neighbor lists.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{5.0}, neighCutoff, setNeighCutoff, PROPERTY_FIELD_MEMORIZE);
    /// Controls the number of bins for the neighbor part of the real-space correlation function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(int{50}, numberOfNeighBins, setNumberOfNeighBins, PROPERTY_FIELD_MEMORIZE);
    /// Controls the averaging direction.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(AveragingDirectionType{RADIAL}, averagingDirection, setAveragingDirection, PROPERTY_FIELD_MEMORIZE);
    /// Controls the normalization of the real-space correlation function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(NormalizationType{VALUE_CORRELATION}, normalizeRealSpace, setNormalizeRealSpace, PROPERTY_FIELD_MEMORIZE);
    /// Controls the normalization by rdf of the real-space correlation function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, normalizeRealSpaceByRDF, setNormalizeRealSpaceByRDF, PROPERTY_FIELD_MEMORIZE);
    /// Controls the normalization by covariance of the real-space correlation function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, normalizeRealSpaceByCovariance, setNormalizeRealSpaceByCovariance, PROPERTY_FIELD_MEMORIZE);
    /// Type of real-space plot (lin-lin, log-lin or log-log)
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, typeOfRealSpacePlot, setTypeOfRealSpacePlot);
    /// Controls the whether the range of the x-axis of the plot should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixRealSpaceXAxisRange, setFixRealSpaceXAxisRange);
    /// Controls the start value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, realSpaceXAxisRangeStart, setRealSpaceXAxisRangeStart, PROPERTY_FIELD_MEMORIZE);
    /// Controls the end value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, realSpaceXAxisRangeEnd, setRealSpaceXAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);
    /// Controls the whether the range of the y-axis of the plot should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixRealSpaceYAxisRange, setFixRealSpaceYAxisRange);
    /// Controls the start value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, realSpaceYAxisRangeStart, setRealSpaceYAxisRangeStart, PROPERTY_FIELD_MEMORIZE);
    /// Controls the end value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, realSpaceYAxisRangeEnd, setRealSpaceYAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);
    /// Controls the normalization of the reciprocal-space correlation function.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(bool{false}, normalizeReciprocalSpace, setNormalizeReciprocalSpace, PROPERTY_FIELD_MEMORIZE);
    /// Type of reciprocal-space plot (lin-lin, log-lin or log-log)
    DECLARE_MODIFIABLE_PROPERTY_FIELD(int{0}, typeOfReciprocalSpacePlot, setTypeOfReciprocalSpacePlot);
    /// Controls the whether the range of the x-axis of the plot should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixReciprocalSpaceXAxisRange, setFixReciprocalSpaceXAxisRange);
    /// Controls the start value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{0}, reciprocalSpaceXAxisRangeStart, setReciprocalSpaceXAxisRangeStart, PROPERTY_FIELD_MEMORIZE);
    /// Controls the end value of the x-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD_FLAGS(FloatType{1}, reciprocalSpaceXAxisRangeEnd, setReciprocalSpaceXAxisRangeEnd, PROPERTY_FIELD_MEMORIZE);
    /// Controls the whether the range of the y-axis of the plot should be fixed.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(bool{false}, fixReciprocalSpaceYAxisRange, setFixReciprocalSpaceYAxisRange);
    /// Controls the start value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{0}, reciprocalSpaceYAxisRangeStart, setReciprocalSpaceYAxisRangeStart);
    /// Controls the end value of the y-axis.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(FloatType{1}, reciprocalSpaceYAxisRangeEnd, setReciprocalSpaceYAxisRangeEnd);
};

}   // End of namespace
