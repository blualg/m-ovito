////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2024 OVITO GmbH, Germany
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

#include <ovito/particles/Particles.h>
#include <ovito/particles/objects/Particles.h>
#include <ovito/stdobj/simcell/SimulationCell.h>
#include <ovito/core/dataset/DataSet.h>
#include <ovito/core/dataset/animation/AnimationSettings.h>
#include <ovito/core/dataset/pipeline/ModificationNode.h>
#include <ovito/core/utilities/units/UnitsManager.h>
#include <ovito/core/utilities/concurrent/ParallelFor.h>
#include <ovito/core/utilities/concurrent/AsynchronousTask.h>
#include <ovito/core/utilities/concurrent/EnumerableThreadSpecific.h>
#include "SpatialCorrelationFunctionModifier.h"

#include <kissfft/kiss_fftnd.h>

namespace Ovito {

IMPLEMENT_CREATABLE_OVITO_CLASS(SpatialCorrelationFunctionModifier);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, sourceProperty1);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, sourceProperty2);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, averagingDirection);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, fftGridSpacing);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, applyWindow);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, doComputeNeighCorrelation);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, neighCutoff);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, numberOfNeighBins);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, normalizeRealSpace);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, normalizeRealSpaceByRDF);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, normalizeRealSpaceByCovariance);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, typeOfRealSpacePlot);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, normalizeReciprocalSpace);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, typeOfReciprocalSpacePlot);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, fixRealSpaceXAxisRange);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, realSpaceXAxisRangeStart);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, realSpaceXAxisRangeEnd);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, fixRealSpaceYAxisRange);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, realSpaceYAxisRangeStart);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, realSpaceYAxisRangeEnd);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, fixReciprocalSpaceXAxisRange);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, reciprocalSpaceXAxisRangeStart);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, reciprocalSpaceXAxisRangeEnd);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, fixReciprocalSpaceYAxisRange);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, reciprocalSpaceYAxisRangeStart);
DEFINE_PROPERTY_FIELD(SpatialCorrelationFunctionModifier, reciprocalSpaceYAxisRangeEnd);
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, sourceProperty1, "First property");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, sourceProperty2, "Second property");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, averagingDirection, "Averaging direction");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, fftGridSpacing, "FFT grid spacing");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, applyWindow, "Apply window function to non-periodic directions");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, doComputeNeighCorrelation, "Direct summation");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, neighCutoff, "Neighbor cutoff radius");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, numberOfNeighBins, "Number of neighbor bins");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, normalizeRealSpace, "Normalize correlation function");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, normalizeRealSpaceByRDF, "Normalize by RDF");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, normalizeRealSpaceByCovariance, "Normalize by covariance");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, normalizeReciprocalSpace, "Normalize correlation function");
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SpatialCorrelationFunctionModifier, fftGridSpacing, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_MINIMUM(SpatialCorrelationFunctionModifier, neighCutoff, WorldParameterUnit, 0);
SET_PROPERTY_FIELD_UNITS_AND_RANGE(SpatialCorrelationFunctionModifier, numberOfNeighBins, IntegerParameterUnit, 4, 100000);
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, fixRealSpaceXAxisRange, "Fix x-range");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, realSpaceXAxisRangeStart, "X-range start");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, realSpaceXAxisRangeEnd, "X-range end");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, fixRealSpaceYAxisRange, "Fix y-range");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, realSpaceYAxisRangeStart, "Y-range start");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, realSpaceYAxisRangeEnd, "Y-range end");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, fixReciprocalSpaceXAxisRange, "Fix x-range");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, reciprocalSpaceXAxisRangeStart, "X-range start");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, reciprocalSpaceXAxisRangeEnd, "X-range end");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, fixReciprocalSpaceYAxisRange, "Fix y-range");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, reciprocalSpaceYAxisRangeStart, "Y-range start");
SET_PROPERTY_FIELD_LABEL(SpatialCorrelationFunctionModifier, reciprocalSpaceYAxisRangeEnd, "Y-range end");

/******************************************************************************
* Constructs the modifier object.
******************************************************************************/
SpatialCorrelationFunctionModifier::SpatialCorrelationFunctionModifier(ObjectInitializationFlags flags) : Modifier(flags),
    _averagingDirection(RADIAL),
    _fftGridSpacing(3.0),
    _applyWindow(true),
    _doComputeNeighCorrelation(false),
    _neighCutoff(5.0),
    _numberOfNeighBins(50),
    _normalizeRealSpace(VALUE_CORRELATION),
    _normalizeRealSpaceByRDF(false),
    _normalizeRealSpaceByCovariance(false),
    _typeOfRealSpacePlot(0),
    _normalizeReciprocalSpace(false),
    _typeOfReciprocalSpacePlot(0),
    _fixRealSpaceXAxisRange(false),
    _realSpaceXAxisRangeStart(0.0),
    _realSpaceXAxisRangeEnd(1.0),
    _fixRealSpaceYAxisRange(false),
    _realSpaceYAxisRangeStart(0.0),
    _realSpaceYAxisRangeEnd(1.0),
    _fixReciprocalSpaceXAxisRange(false),
    _reciprocalSpaceXAxisRangeStart(0.0),
    _reciprocalSpaceXAxisRangeEnd(1.0),
    _fixReciprocalSpaceYAxisRange(false),
    _reciprocalSpaceYAxisRangeStart(0.0),
    _reciprocalSpaceYAxisRangeEnd(1.0)
{
}

/******************************************************************************
* Asks the modifier whether it can be applied to the given input data.
******************************************************************************/
bool SpatialCorrelationFunctionModifier::OOMetaClass::isApplicableTo(const DataCollection& input) const
{
    return input.containsObject<Particles>();
}

/******************************************************************************
 * Sends an event to all dependents of this RefTarget.
 ******************************************************************************/
void SpatialCorrelationFunctionModifier::notifyDependentsImpl(const ReferenceEvent& event) noexcept
{
    if(event.type() == ReferenceEvent::TargetChanged && event.sender() == this) {
        // Avoid a full recomputation if one of the plotting-related parameters of the modifier change.
        const TargetChangedEvent& changeEvent = static_cast<const TargetChangedEvent&>(event);
        if(changeEvent.field() == PROPERTY_FIELD(fixRealSpaceXAxisRange) ||
                changeEvent.field() == PROPERTY_FIELD(fixRealSpaceYAxisRange) ||
                changeEvent.field() == PROPERTY_FIELD(realSpaceXAxisRangeStart) ||
                changeEvent.field() == PROPERTY_FIELD(realSpaceXAxisRangeEnd) ||
                changeEvent.field() == PROPERTY_FIELD(realSpaceYAxisRangeStart) ||
                changeEvent.field() == PROPERTY_FIELD(realSpaceYAxisRangeEnd) ||
                changeEvent.field() == PROPERTY_FIELD(fixReciprocalSpaceXAxisRange) ||
                changeEvent.field() == PROPERTY_FIELD(fixReciprocalSpaceYAxisRange) ||
                changeEvent.field() == PROPERTY_FIELD(reciprocalSpaceXAxisRangeStart) ||
                changeEvent.field() == PROPERTY_FIELD(reciprocalSpaceXAxisRangeEnd) ||
                changeEvent.field() == PROPERTY_FIELD(reciprocalSpaceYAxisRangeStart) ||
                changeEvent.field() == PROPERTY_FIELD(reciprocalSpaceYAxisRangeEnd) ||
                changeEvent.field() == PROPERTY_FIELD(normalizeRealSpace) ||
                changeEvent.field() == PROPERTY_FIELD(normalizeRealSpaceByRDF) ||
                changeEvent.field() == PROPERTY_FIELD(normalizeRealSpaceByCovariance) ||
                changeEvent.field() == PROPERTY_FIELD(normalizeReciprocalSpace) ||
                changeEvent.field() == PROPERTY_FIELD(typeOfRealSpacePlot) ||
                changeEvent.field() == PROPERTY_FIELD(typeOfReciprocalSpacePlot)) {
            // Changes to these options do not invalidate the modifier's results.
            // Intercept the change event and modify it such that it does not trigger a re-evaluation of the modifier.
            Modifier::notifyDependentsImpl(TargetChangedEvent(this, changeEvent.field(), TimeInterval::infinite()));
            return;
        }
    }
    Modifier::notifyDependentsImpl(event);
}

/******************************************************************************
* This method is called by the system when the modifier has been inserted
* into a pipeline.
******************************************************************************/
void SpatialCorrelationFunctionModifier::initializeModifier(const ModifierInitializationRequest& request)
{
    Modifier::initializeModifier(request);

    // Use the first available particle property from the input state as data source when the modifier is newly created.
    if((sourceProperty1().isNull() || sourceProperty2().isNull()) && ExecutionContext::isInteractive()) {
        const PipelineFlowState& input = request.modificationNode()->evaluateInput(request).result();
        if(const Particles* container = input.getObject<Particles>()) {
            ParticlePropertyReference bestProperty;
            for(const Property* property : container->properties()) {
                bestProperty = ParticlePropertyReference(property, (property->componentCount() > 1) ? 0 : -1);
            }
            if(!bestProperty.isNull()) {
                if(sourceProperty1().isNull())
                    setSourceProperty1(bestProperty);
                if(sourceProperty2().isNull())
                    setSourceProperty2(bestProperty);
            }
        }
    }
}

/******************************************************************************
 * Is called by the pipeline system before a new modifier evaluation begins.
 ******************************************************************************/
void SpatialCorrelationFunctionModifier::preevaluateModifier(const ModifierEvaluationRequest& request, PipelineEvaluationResult::EvaluationTypes& evaluationTypes, TimeInterval& validityInterval) const
{
    // Indicate that we will do different computations depending on whether the pipeline is evaluated in interactive mode or not.
    if(request.interactiveMode())
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Interactive;
    else
        evaluationTypes = PipelineEvaluationResult::EvaluationType::Noninteractive;
}

/******************************************************************************
* Modifies the input data.
******************************************************************************/
Future<PipelineFlowState> SpatialCorrelationFunctionModifier::evaluateModifier(const ModifierEvaluationRequest& request, PipelineFlowState&& state)
{
    // In interactive mode, do not perform a real computation. Instead, reuse an old result from the cached state if available.
    if(request.interactiveMode()) {
        if(PipelineFlowState cachedState = request.modificationNode()->getCachedPipelineNodeOutput(request.time(), true)) {
            state.adoptDataObjectsFrom(cachedState, request.modificationNode());
        }
        return std::move(state);
    }

    // Get the source data.
    if(sourceProperty1().isNull())
        throw Exception(tr("Please select a first input particle property."));
    if(sourceProperty2().isNull())
        throw Exception(tr("please select a second input particle property."));

    // Get the current positions.
    const Particles* particles = state.expectObject<Particles>();
    particles->verifyIntegrity();
    const Property* posProperty = particles->expectProperty(Particles::PositionProperty);

    // Get the current selected properties.
    const Property* property1 = sourceProperty1().findInContainer(particles);
    const Property* property2 = sourceProperty2().findInContainer(particles);
    if(!property1)
        throw Exception(tr("The selected input particle property with the name '%1' does not exist.").arg(sourceProperty1().name()));
    if(!property2)
        throw Exception(tr("The selected input particle property with the name '%1' does not exist.").arg(sourceProperty2().name()));

    // Get simulation cell.
    const SimulationCell* inputCell = state.expectObject<SimulationCell>();
    if((inputCell->is2D() ? inputCell->volume2D() : inputCell->volume3D()) < FLOATTYPE_EPSILON)
        throw Exception(tr("Simulation cell is degenerate. Cannot compute correlation function."));

    // Create engine object. Pass all relevant modifier parameters to the engine as well as the input data.
    auto engine = std::make_shared<CorrelationAnalysisEngine>(
                                                    posProperty,
                                                    property1,
                                                    std::max(0, sourceProperty1().vectorComponent()),
                                                    property2,
                                                    std::max(0, sourceProperty2().vectorComponent()),
                                                    inputCell,
                                                    fftGridSpacing(),
                                                    applyWindow(),
                                                    doComputeNeighCorrelation(),
                                                    neighCutoff(),
                                                    numberOfNeighBins(),
                                                    averagingDirection());

    // Perform the main calculation in a separate thread.
    return AsynchronousTask<PipelineFlowState>::runAsync([state = std::move(state), engine = std::move(engine), createdByNode = request.modificationNode()]() mutable {
        engine->perform();
        engine->applyResults(state, createdByNode);
        return std::move(state);
    }, true);
}

/******************************************************************************
* Map property onto grid.
******************************************************************************/
std::vector<FloatType> SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::mapToSpatialGrid(const Property* property,
                                                                              size_t propertyVectorComponent,
                                                                              const AffineTransformation& reciprocalCellMatrix,
                                                                              int nX, int nY, int nZ,
                                                                              bool applyWindow)
{
    size_t vecComponent = std::max(size_t(0), propertyVectorComponent);
    size_t vecComponentCount = property ? property->componentCount() : 0;
    int numberOfGridPoints = nX * nY * nZ;
    bool is2D = cell()->is2D();

    // Allocate real space grid.
    std::vector<FloatType> gridData(numberOfGridPoints, 0);

    // Get periodic boundary flag.
    const std::array<bool, 3> pbc = cell()->pbcFlagsCorrected();

    if(!property || property->size() > 0) {
        BufferReadAccess<Point3> positionsArray(positions());

        if(!property) {
            for(const Point3& pos : positionsArray) {
                Point3 fractionalPos = reciprocalCellMatrix * pos;
                int binIndexX = int( fractionalPos.x() * nX );
                int binIndexY = int( fractionalPos.y() * nY );
                int binIndexZ = int( fractionalPos.z() * nZ );
                FloatType window = 1;
                if(pbc[0]) binIndexX = SimulationCell::modulo(binIndexX, nX);
                else window *= std::sqrt(FloatType(2./3))*(FloatType(1)-std::cos(2*FLOATTYPE_PI*fractionalPos.x()));
                if(pbc[1]) binIndexY = SimulationCell::modulo(binIndexY, nY);
                else window *= std::sqrt(FloatType(2./3))*(FloatType(1)-std::cos(2*FLOATTYPE_PI*fractionalPos.y()));
                if(is2D) binIndexZ = 0;
                else if(pbc[2]) binIndexZ = SimulationCell::modulo(binIndexZ, nZ);
                else window *= std::sqrt(FloatType(2./3))*(FloatType(1)-std::cos(2*FLOATTYPE_PI*fractionalPos.z()));
                if(!applyWindow) window = 1;
                if(binIndexX >= 0 && binIndexX < nX && binIndexY >= 0 && binIndexY < nY && binIndexZ >= 0 && binIndexZ < nZ) {
                    // Store in row-major format.
                    size_t binIndex = binIndexZ+nZ*(binIndexY+nY*binIndexX);
                    gridData[binIndex] += window;
                }
            }
        }
        else {
            property->forAnyType([&](auto _) {
                using T = decltype(_);
                BufferReadAccess<T*> propertyArray(property);
                const Point3* pos = positionsArray.cbegin();
                for(T v : propertyArray.componentRange(vecComponent)) {
                    if(std::numeric_limits<T>::is_integer || !std::isnan(v)) {
                        Point3 fractionalPos = reciprocalCellMatrix * (*pos);
                        int binIndexX = int( fractionalPos.x() * nX );
                        int binIndexY = int( fractionalPos.y() * nY );
                        int binIndexZ = int( fractionalPos.z() * nZ );
                        FloatType window = 1;
                        if(pbc[0]) binIndexX = SimulationCell::modulo(binIndexX, nX);
                        else window *= std::sqrt(FloatType(2./3))*(FloatType(1)-std::cos(2*FLOATTYPE_PI*fractionalPos.x()));
                        if(pbc[1]) binIndexY = SimulationCell::modulo(binIndexY, nY);
                        else window *= std::sqrt(FloatType(2./3))*(FloatType(1)-std::cos(2*FLOATTYPE_PI*fractionalPos.y()));
                        if(is2D) binIndexZ = 0;
                        else if(pbc[2]) binIndexZ = SimulationCell::modulo(binIndexZ, nZ);
                        else window *= std::sqrt(FloatType(2./3))*(FloatType(1)-std::cos(2*FLOATTYPE_PI*fractionalPos.z()));
                        if(!applyWindow) window = 1;
                        if(binIndexX >= 0 && binIndexX < nX && binIndexY >= 0 && binIndexY < nY && binIndexZ >= 0 && binIndexZ < nZ) {
                            // Store in row-major format.
                            size_t binIndex = binIndexZ+nZ*(binIndexY+nY*binIndexX);
                            gridData[binIndex] += window * v;
                        }
                    }
                    ++pos;
                }
            });
        }
    }
    return gridData;
}

std::vector<std::complex<FloatType>> SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::r2cFFT(int nX, int nY, int nZ, std::vector<FloatType> &rData)
{
    OVITO_ASSERT(nX * nY * nZ == rData.size());

    // Convert real-valued input data to complex data type, because KISS FFT expects an array of complex numbers.
    const int dims[3] = { nX, nY, nZ };
    kiss_fftnd_cfg kiss = kiss_fftnd_alloc(dims, cell()->is2D() ? 2 : 3, false, 0, 0);
    std::vector<kiss_fft_cpx> in(nX * nY * nZ);
    auto rDataIter = rData.begin();
    for(kiss_fft_cpx& c : in) {
        c.r = *rDataIter++;
        c.i = 0;
    }

    // Allocate the output buffer.
    std::vector<std::complex<FloatType>> cData(nX * nY * nZ);
    OVITO_STATIC_ASSERT(sizeof(kiss_fft_cpx) == sizeof(std::complex<FloatType>));

    if(!this_task::isCanceled()) {
        // Perform FFT calculation.
        kiss_fftnd(kiss, in.data(), reinterpret_cast<kiss_fft_cpx*>(cData.data()));
    }
    kiss_fft_free(kiss);

    return cData;
}

std::vector<FloatType> SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::c2rFFT(int nX, int nY, int nZ, std::vector<std::complex<FloatType>>& cData)
{
    OVITO_ASSERT(nX * nY * nZ == cData.size());

    const int dims[3] = { nX, nY, nZ };
    kiss_fftnd_cfg kiss = kiss_fftnd_alloc(dims,  cell()->is2D() ? 2 : 3, true, 0, 0);
    std::vector<kiss_fft_cpx> out(nX * nY * nZ);
    OVITO_STATIC_ASSERT(sizeof(kiss_fft_cpx) == sizeof(std::complex<FloatType>));

    // Perform FFT calculation.
    if(!this_task::isCanceled()) {
        kiss_fftnd(kiss, reinterpret_cast<const kiss_fft_cpx*>(cData.data()), out.data());
    }
    kiss_fft_free(kiss);

    // Convert complex values to real values.
    std::vector<FloatType> rData(nX * nY * nZ);
    auto rDataIter = rData.begin();
    for(const kiss_fft_cpx& c : out) {
        *rDataIter++ = c.r;
    }

    return rData;
}

/******************************************************************************
* Compute real and reciprocal space correlation function via FFT.
******************************************************************************/
void SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::computeFftCorrelation()
{
    // Get reciprocal cell.
    const AffineTransformation& cellMatrix = cell()->matrix();
    const AffineTransformation& reciprocalCellMatrix = cell()->inverseMatrix();

    // Note: Cell vectors are in columns. Those are 3-vectors.
    int nX = std::max(1, (int)(cellMatrix.column(0).length() / fftGridSpacing()));
    int nY = std::max(1, (int)(cellMatrix.column(1).length() / fftGridSpacing()));
    int nZ = !cell()->is2D() ? std::max(1, (int)(cellMatrix.column(2).length() / fftGridSpacing())) : 1;
    size_t ntotal = (size_t)nX * (size_t)nY * (size_t)nZ;
    // The current version of the KISSFFT library does not support FFT grids with more than 2^31 bins.
    // The current version of the KISSFFT library does not support FFT grids with more than 2^31 bins.
    if(ntotal > (size_t)std::numeric_limits<int>::max())
        throw Exception(tr("FFT grid spacing is too fine for this simulation cell volume. The maximum number of FFT grid cells has been exceeded (%1 x %2 x %3 = %4 cells, limit is %5).").arg(nX).arg(nY).arg(nZ).arg(ntotal).arg(std::numeric_limits<int>::max()));

    // Map all quantities onto a spatial grid.
    std::vector<FloatType> gridProperty1 = mapToSpatialGrid(sourceProperty1().get(),
                     _vecComponent1,
                     reciprocalCellMatrix,
                     nX, nY, nZ,
                     _applyWindow);

    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    std::vector<FloatType> gridProperty2 = mapToSpatialGrid(sourceProperty2().get(),
                     _vecComponent2,
                     reciprocalCellMatrix,
                     nX, nY, nZ,
                     _applyWindow);
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    std::vector<FloatType> gridDensity = mapToSpatialGrid(nullptr,
                     _vecComponent1,
                     reciprocalCellMatrix,
                     nX, nY, nZ,
                     _applyWindow);
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    // FIXME. Apply windowing function in non-periodic directions here.

    // Compute reciprocal-space correlation function from a product in Fourier space.

    // Compute Fourier transform of spatial grid.
    std::vector<std::complex<FloatType>> ftProperty1 = r2cFFT(nX, nY, nZ, gridProperty1);
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    std::vector<std::complex<FloatType>> ftProperty2 = r2cFFT(nX, nY, nZ, gridProperty2);
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    std::vector<std::complex<FloatType>> ftDensity = r2cFFT(nX, nY, nZ, gridDensity);
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    // Note: Reciprocal cell vectors are in rows. Those are 4-vectors.
    Vector4 recCell1 = reciprocalCellMatrix.row(0);
    Vector4 recCell2 = reciprocalCellMatrix.row(1);
    Vector4 recCell3 = reciprocalCellMatrix.row(2);

    // Compute distance of cell faces.
    FloatType cellFaceDistance1 = 1 / std::sqrt(recCell1.x()*recCell1.x() + recCell1.y()*recCell1.y() + recCell1.z()*recCell1.z());
    FloatType cellFaceDistance2 = 1 / std::sqrt(recCell2.x()*recCell2.x() + recCell2.y()*recCell2.y() + recCell2.z()*recCell2.z());
    FloatType cellFaceDistance3 = 1 / std::sqrt(recCell3.x()*recCell3.x() + recCell3.y()*recCell3.y() + recCell3.z()*recCell3.z());

    FloatType minCellFaceDistance = !cell()->is2D()
        ? std::min({cellFaceDistance1, cellFaceDistance2, cellFaceDistance3})
        : std::min({cellFaceDistance1, cellFaceDistance2});

    // Minimum reciprocal space vector is given by the minimum distance of cell faces.
    FloatType minReciprocalSpaceVector = 1 / minCellFaceDistance;
    int numberOfWavevectorBins, dir1, dir2;
    Vector3I n(nX, nY, nZ);

    if(_averagingDirection == RADIAL) {
        numberOfWavevectorBins = 1 / (2 * minReciprocalSpaceVector * fftGridSpacing());
    }
    else {
        OVITO_ASSERT(cell()->is2D() == false);
        dir1 = (_averagingDirection+1) % 3;
        dir2 = (_averagingDirection+2) % 3;
        numberOfWavevectorBins = n[dir1] * n[dir2];
    }

    // Averaged reciprocal space correlation function.
    _reciprocalSpaceCorrelation = DataTable::OOClass().createUserProperty(DataBuffer::Initialized, numberOfWavevectorBins, Property::FloatDefault, 1, tr("C(q)"));
    _reciprocalSpaceCorrelationRange = 2 * FLOATTYPE_PI * minReciprocalSpaceVector * numberOfWavevectorBins;

    std::vector<int> numberOfValues(numberOfWavevectorBins, 0);
    BufferWriteAccess<FloatType, access_mode::read_write> reciprocalSpaceCorrelationData(_reciprocalSpaceCorrelation);

    // Compute Fourier-transformed correlation function and put it on a radial grid.
    int binIndex = 0;
    for(int binIndexX = 0; binIndexX < nX; binIndexX++) {
        for(int binIndexY = 0; binIndexY < nY; binIndexY++) {
            for(int binIndexZ = 0; binIndexZ < nZ; binIndexZ++, binIndex++) {
                // Compute correlation function.
                std::complex<FloatType> corr = ftProperty1[binIndex] * std::conj(ftProperty2[binIndex]);

                // Store correlation function to property1 for back transform.
                ftProperty1[binIndex] = corr;

                // Compute structure factor/radial distribution function.
                ftDensity[binIndex] = ftDensity[binIndex] * std::conj(ftDensity[binIndex]);

                int wavevectorBinIndex;
                if(_averagingDirection == RADIAL) {
                    // Ignore Gamma-point for radial average.
                    if(binIndex == 0 && binIndexY == 0 && binIndexZ == 0)
                        continue;

                    // Compute wavevector.
                    int iX = SimulationCell::modulo(binIndexX+nX/2, nX)-nX/2;
                    int iY = SimulationCell::modulo(binIndexY+nY/2, nY)-nY/2;
                    int iZ = SimulationCell::modulo(binIndexZ+nZ/2, nZ)-nZ/2;
                    OVITO_ASSERT(!cell()->is2D() || iZ == 0);
                    // This is the reciprocal space vector (without a factor of 2*pi).
                    Vector4 wavevector = FloatType(iX)*reciprocalCellMatrix.row(0) +
                                         FloatType(iY)*reciprocalCellMatrix.row(1) +
                                         FloatType(iZ)*reciprocalCellMatrix.row(2);
                    wavevector.w() = 0.0;

                    // Compute bin index.
                    wavevectorBinIndex = int(std::floor(wavevector.length() / minReciprocalSpaceVector));
                }
                else {
                    Vector3I binIndexXYZ(binIndexX, binIndexY, binIndexZ);
                    wavevectorBinIndex = binIndexXYZ[dir2] + n[dir2]*binIndexXYZ[dir1];
                }

                if(wavevectorBinIndex >= 0 && wavevectorBinIndex < numberOfWavevectorBins) {
                    reciprocalSpaceCorrelationData[wavevectorBinIndex] += std::real(corr);
                    numberOfValues[wavevectorBinIndex]++;
                }
            }
        }
        this_task::throwIfCanceled();
    }

    // Compute averages and normalize reciprocal-space correlation function.
    FloatType normalizationFactor = (cell()->is2D() ? cell()->volume2D() : cell()->volume3D()) / (sourceProperty1()->size() * sourceProperty2()->size());
    for(int wavevectorBinIndex = 0; wavevectorBinIndex < numberOfWavevectorBins; wavevectorBinIndex++) {
        if(numberOfValues[wavevectorBinIndex] != 0)
            reciprocalSpaceCorrelationData[wavevectorBinIndex] *= normalizationFactor / numberOfValues[wavevectorBinIndex];
    }
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    // Compute long-ranged part of the real-space correlation function from the FFT convolution.

    // Computer inverse Fourier transform of correlation function.
    gridProperty1 = c2rFFT(nX, nY, nZ, ftProperty1);
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    gridDensity = c2rFFT(nX, nY, nZ, ftDensity);
    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();

    // Determine number of grid points for reciprocal-spacespace correlation function.
    int numberOfDistanceBins = minCellFaceDistance / (2 * fftGridSpacing());
    FloatType gridSpacing = minCellFaceDistance / (2 * numberOfDistanceBins);

    // Radially averaged real space correlation function.
    _realSpaceCorrelation = DataTable::OOClass().createUserProperty(DataBuffer::Initialized, numberOfDistanceBins, DataBuffer::FloatDefault, 1, tr("C(r)"));
    _realSpaceCorrelationRange = minCellFaceDistance / 2;
    _realSpaceRDF = DataTable::OOClass().createUserProperty(DataBuffer::Initialized, numberOfDistanceBins, DataBuffer::FloatDefault, 1, tr("g(r)"));

    numberOfValues = std::vector<int>(numberOfDistanceBins, 0);
    BufferWriteAccess<FloatType, access_mode::read_write> realSpaceCorrelationData(_realSpaceCorrelation);
    BufferWriteAccess<FloatType, access_mode::read_write> realSpaceRDFData(_realSpaceRDF);

    // Put real-space correlation function on a radial grid.
    binIndex = 0;
    for(int binIndexX = 0; binIndexX < nX; binIndexX++) {
        for(int binIndexY = 0; binIndexY < nY; binIndexY++) {
            for(int binIndexZ = 0; binIndexZ < nZ; binIndexZ++, binIndex++) {
                // Ignore origin for radial average (which is just the covariance of the quantities).
                if(binIndex == 0 && binIndexY == 0 && binIndexZ == 0)
                    continue;

                // Compute distance. (FIXME! Check that this is actually correct for even and odd numbers of grid points.)
                FloatType fracX = FloatType(SimulationCell::modulo(binIndexX+nX/2, nX)-nX/2)/nX;
                FloatType fracY = FloatType(SimulationCell::modulo(binIndexY+nY/2, nY)-nY/2)/nY;
                FloatType fracZ = FloatType(SimulationCell::modulo(binIndexZ+nZ/2, nZ)-nZ/2)/nZ;
                OVITO_ASSERT(!cell()->is2D() || fracZ == 0.0);
                // This is the real space vector.
                Vector3 distance = fracX*cellMatrix.column(0) +
                                   fracY*cellMatrix.column(1) +
                                   fracZ*cellMatrix.column(2);

                // Length of real space vector.
                int distanceBinIndex = int(std::floor(distance.length() / gridSpacing));
                if(distanceBinIndex >= 0 && distanceBinIndex < numberOfDistanceBins) {
                    realSpaceCorrelationData[distanceBinIndex] += gridProperty1[binIndex];
                    realSpaceRDFData[distanceBinIndex] += gridDensity[binIndex];
                    numberOfValues[distanceBinIndex]++;
                }
            }
        }
        this_task::throwIfCanceled();
    }

    // Compute averages and normalize real-space correlation function. Note KISS FFT computes an unnormalized transform.
    normalizationFactor = 1.0 / (sourceProperty1()->size() * sourceProperty2()->size());
    for(int distanceBinIndex = 0; distanceBinIndex < numberOfDistanceBins; distanceBinIndex++) {
        if(numberOfValues[distanceBinIndex] != 0) {
            realSpaceCorrelationData[distanceBinIndex] *= normalizationFactor / numberOfValues[distanceBinIndex];
            realSpaceRDFData[distanceBinIndex] *= normalizationFactor / numberOfValues[distanceBinIndex];
        }
    }

    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();
}

/******************************************************************************
* Compute real space correlation function via direction summation over neighbors.
******************************************************************************/
void SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::computeNeighCorrelation()
{
    // Get number of particles.
    size_t particleCount = positions()->size();

    // Allocate neighbor RDF.
    _neighRDF = DataTable::OOClass().createUserProperty(DataBuffer::Initialized, neighCorrelation()->size(), DataBuffer::FloatDefault, 1, tr("Neighbor g(r)"));

    // Prepare the neighbor list.
    CutoffNeighborFinder neighborListBuilder;
    neighborListBuilder.prepare(neighCutoff(), positions(), cell(), {});

    // Get pointers to data.
    RawBufferReadAccess dataAccess1 = sourceProperty1();
    RawBufferReadAccess dataAccess2 = sourceProperty2();

    // Perform analysis on each particle in parallel.
    size_t vecComponent1 = _vecComponent1;
    size_t vecComponent2 = _vecComponent2;
    FloatType gridSpacing = (neighCutoff() + FLOATTYPE_EPSILON) / neighCorrelation()->size();
    this_task::setProgressMaximum(particleCount);
    EnumerableThreadSpecific<std::vector<FloatType>> threadLocalCorrelations;
    EnumerableThreadSpecific<std::vector<int64_t>> threadLocalRDFs;
    parallelForInnerOuter(particleCount, 4096, [&](auto&& iterate) {
        std::vector<FloatType>& threadLocalCorrelation = threadLocalCorrelations.create(neighCorrelation()->size(), 0);
        std::vector<int64_t>& threadLocalRDF = threadLocalRDFs.create(neighCorrelation()->size(), 0);
        iterate([&](size_t i) {
            FloatType data1 = dataAccess1.get<FloatType>(i, vecComponent1);
            for(CutoffNeighborFinder::Query neighQuery(neighborListBuilder, i); !neighQuery.atEnd(); neighQuery.next()) {
                size_t distanceBinIndex = (size_t)(neighQuery.distance() / gridSpacing);
                distanceBinIndex = std::min(distanceBinIndex, threadLocalCorrelation.size() - 1);
                FloatType data2 = dataAccess2.get<FloatType>(neighQuery.current(), vecComponent2);
                threadLocalCorrelation[distanceBinIndex] += data1 * data2;
                threadLocalRDF[distanceBinIndex]++;
            }
        });
    });

    // Combine per-thread RDFs into a set of master histograms.
    BufferWriteAccess<FloatType, access_mode::read_write> neighCorrelationArray(neighCorrelation());
    BufferWriteAccess<FloatType, access_mode::read_write> neighRDFArray(neighRDF());
    threadLocalCorrelations.visitEach([&](const std::vector<FloatType>& r) {
        OVITO_ASSERT(r.size() == neighCorrelationArray.size() * neighCorrelationArray.componentCount());
        auto bin = r.cbegin();
        for(auto iter = neighCorrelationArray.begin(); iter != neighCorrelationArray.end(); ++iter)
            *iter += *bin++;
    });
    threadLocalRDFs.visitEach([&](const std::vector<int64_t>& r) {
        OVITO_ASSERT(r.size() == neighRDFArray.size() * neighRDFArray.componentCount());
        auto bin = r.cbegin();
        for(auto iter = neighRDFArray.begin(); iter != neighRDFArray.end(); ++iter)
            *iter += *bin++;
    });

    this_task::nextProgressSubStep();

    // Normalize short-ranged real-space correlation function.
    if(!cell()->is2D()) {
        FloatType normalizationFactor = 3 * cell()->volume3D() / (4 * FLOATTYPE_PI * sourceProperty1()->size() * sourceProperty2()->size());
        for(size_t distanceBinIndex = 0; distanceBinIndex < neighCorrelation()->size(); distanceBinIndex++) {
            FloatType distance = distanceBinIndex * gridSpacing;
            FloatType distance2 = distance + gridSpacing;
            neighCorrelationArray[distanceBinIndex] *= normalizationFactor / (distance2*distance2*distance2 - distance*distance*distance);
            neighRDFArray[distanceBinIndex] *= normalizationFactor / (distance2*distance2*distance2 - distance*distance*distance);
        }
    }
    else {
        FloatType normalizationFactor = cell()->volume2D() / (FLOATTYPE_PI * sourceProperty1()->size() * sourceProperty2()->size());
        for(size_t distanceBinIndex = 0; distanceBinIndex < neighCorrelation()->size(); distanceBinIndex++) {
            FloatType distance = distanceBinIndex * gridSpacing;
            FloatType distance2 = distance + gridSpacing;
            neighCorrelationArray[distanceBinIndex] *= normalizationFactor / (distance2*distance2 - distance*distance);
            neighRDFArray[distanceBinIndex] *= normalizationFactor / (distance2*distance2 - distance*distance);
        }
    }

    this_task::nextProgressSubStep();
    this_task::throwIfCanceled();
}

/******************************************************************************
* Compute means and covariance.
******************************************************************************/
void SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::computeLimits()
{
    // Get pointers to data.
    RawBufferReadAccess dataAccess1 = sourceProperty1();
    RawBufferReadAccess dataAccess2 = sourceProperty2();

    // Compute mean and covariance values.
    FloatType mean1 = 0;
    FloatType mean2 = 0;
    FloatType variance1 = 0;
    FloatType variance2 = 0;
    FloatType covariance = 0;
    size_t particleCount = sourceProperty1()->size();
    for(size_t particleIndex = 0; particleIndex < particleCount; particleIndex++) {
        FloatType data1 = dataAccess1.get<FloatType>(particleIndex, _vecComponent1);
        FloatType data2 = dataAccess2.get<FloatType>(particleIndex, _vecComponent2);
        mean1 += data1;
        mean2 += data2;
        variance1 += data1*data1;
        variance2 += data2*data2;
        covariance += data1*data2;
        this_task::throwIfCanceled();
    }
    mean1 /= particleCount;
    mean2 /= particleCount;
    variance1 /= particleCount;
    variance2 /= particleCount;
    covariance /= particleCount;
    setMoments(mean1, mean2, variance1, variance2, covariance);
}

/******************************************************************************
* Performs the actual computation. This method is executed in a worker thread.
******************************************************************************/
void SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::perform()
{
    this_task::setProgressText(tr("Computing correlation function"));
    this_task::beginProgressSubSteps(neighCorrelation() ? 13 : 11);

    // Compute reciprocal space correlation function and long-ranged part of
    // the real-space correlation function from an FFT.
    computeFftCorrelation();
    this_task::throwIfCanceled();

    // Compute short-ranged part of the real-space correlation function from a direct loop over particle neighbors.
    if(neighCorrelation())
        computeNeighCorrelation();
    this_task::throwIfCanceled();

    computeLimits();
    this_task::endProgressSubSteps();

    // Release data that is no longer needed.
    _positions.reset();
    _sourceProperty1.reset();
    _sourceProperty2.reset();
    _simCell.reset();
}

/******************************************************************************
* Injects the computed results of the engine into the data pipeline.
******************************************************************************/
void SpatialCorrelationFunctionModifier::CorrelationAnalysisEngine::applyResults(PipelineFlowState& state, const OOWeakRef<const PipelineNode>& createdByNode)
{
    // Output real-space correlation function to the pipeline as a data table.
    DataTable* realSpaceCorrelationObj = state.createObject<DataTable>(QStringLiteral("correlation-real-space"), createdByNode, DataTable::Line, tr("Real-space correlation"), realSpaceCorrelation());
    realSpaceCorrelationObj->setAxisLabelX(tr("Distance r"));
    realSpaceCorrelationObj->setIntervalStart(0);
    realSpaceCorrelationObj->setIntervalEnd(_realSpaceCorrelationRange);

    // Output real-space RDF to the pipeline as a data table.
    DataTable* realSpaceRDFObj = state.createObject<DataTable>(QStringLiteral("correlation-real-space-rdf"), createdByNode, DataTable::Line, tr("Real-space RDF"), realSpaceRDF());
    realSpaceRDFObj->setAxisLabelX(tr("Distance r"));
    realSpaceRDFObj->setIntervalStart(0);
    realSpaceRDFObj->setIntervalEnd(_realSpaceCorrelationRange);

    // Output short-ranged part of the real-space correlation function to the pipeline as a data table.
    if(neighCorrelation()) {
        DataTable* neighCorrelationObj = state.createObject<DataTable>(QStringLiteral("correlation-neighbor"), createdByNode, DataTable::Line, tr("Neighbor correlation"), neighCorrelation());
        neighCorrelationObj->setAxisLabelX(tr("Distance r"));
        neighCorrelationObj->setIntervalStart(0);
        neighCorrelationObj->setIntervalEnd(neighCutoff());
    }

    // Output short-ranged part of the RDF to the pipeline as a data table.
    if(neighRDF()) {
        DataTable* neighRDFObj = state.createObject<DataTable>(QStringLiteral("correlation-neighbor-rdf"), createdByNode, DataTable::Line, tr("Neighbor RDF"), neighRDF());
        neighRDFObj->setAxisLabelX(tr("Distance r"));
        neighRDFObj->setIntervalStart(0);
        neighRDFObj->setIntervalEnd(neighCutoff());
    }

    // Output reciprocal-space correlation function to the pipeline as a data table.
    DataTable* reciprocalSpaceCorrelationObj = state.createObject<DataTable>(QStringLiteral("correlation-reciprocal-space"), createdByNode, DataTable::Line, tr("Reciprocal-space correlation"), reciprocalSpaceCorrelation());
    reciprocalSpaceCorrelationObj->setAxisLabelX(tr("Wavevector q"));
    reciprocalSpaceCorrelationObj->setIntervalStart(0);
    reciprocalSpaceCorrelationObj->setIntervalEnd(_reciprocalSpaceCorrelationRange);

    // Output global attributes.
    state.addAttribute(QStringLiteral("CorrelationFunction.mean1"), QVariant::fromValue(mean1()), createdByNode);
    state.addAttribute(QStringLiteral("CorrelationFunction.mean2"), QVariant::fromValue(mean2()), createdByNode);
    state.addAttribute(QStringLiteral("CorrelationFunction.variance1"), QVariant::fromValue(variance1()), createdByNode);
    state.addAttribute(QStringLiteral("CorrelationFunction.variance2"), QVariant::fromValue(variance2()), createdByNode);
    state.addAttribute(QStringLiteral("CorrelationFunction.covariance"), QVariant::fromValue(covariance()), createdByNode);
}

}   // End of namespace
