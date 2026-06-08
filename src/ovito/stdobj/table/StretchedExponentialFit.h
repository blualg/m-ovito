////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
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

#include <ovito/core/dataset/data/DataCollection.h>
#include <ovito/core/dataset/pipeline/PipelineFlowState.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace Ovito {

struct StretchedExponentialFitResult {
    bool valid = false;
    QString status;
    QString curveLabel;
    double amplitude = std::numeric_limits<double>::quiet_NaN();
    double tau = std::numeric_limits<double>::quiet_NaN();
    double beta = std::numeric_limits<double>::quiet_NaN();
    double offset = std::numeric_limits<double>::quiet_NaN();
    double integratedTime = std::numeric_limits<double>::quiet_NaN();
    double rSquared = std::numeric_limits<double>::quiet_NaN();
    double rmse = std::numeric_limits<double>::quiet_NaN();
    double fitStart = std::numeric_limits<double>::quiet_NaN();
    double fitEnd = std::numeric_limits<double>::quiet_NaN();
    int pointCount = 0;
};

inline bool isFiniteFitValue(double value)
{
    return std::isfinite(value);
}

inline QString formatFitNumber(double value, int precision = 4)
{
    return std::isfinite(value) ? QString::number(value, 'g', precision) : QStringLiteral("n/a");
}

inline StretchedExponentialFitResult fitStretchedExponentialDecay(const std::vector<double>& xValues,
                                                                  const std::vector<double>& yValues,
                                                                  const QString& curveLabel = {})
{
    StretchedExponentialFitResult result;
    result.curveLabel = curveLabel;

    if(xValues.size() != yValues.size() || xValues.empty()) {
        result.status = QStringLiteral("curve has no valid samples");
        return result;
    }

    std::vector<size_t> finiteIndices;
    finiteIndices.reserve(xValues.size());
    for(size_t i = 0; i < xValues.size(); ++i) {
        if(isFiniteFitValue(xValues[i]) && isFiniteFitValue(yValues[i]) && xValues[i] >= 0.0)
            finiteIndices.push_back(i);
    }
    if(finiteIndices.size() < 6) {
        result.status = QStringLiteral("fewer than 6 finite lag points");
        return result;
    }

    const size_t firstIndex = finiteIndices.front();
    const double x0 = xValues[firstIndex];
    const double y0 = yValues[firstIndex];
    if(!isFiniteFitValue(y0)) {
        result.status = QStringLiteral("zero-lag value is invalid");
        return result;
    }

    const size_t tailCount = std::max<size_t>(3, finiteIndices.size() / 5);
    double tailSum = 0.0;
    size_t usedTailCount = 0;
    for(size_t k = finiteIndices.size() - std::min(tailCount, finiteIndices.size()); k < finiteIndices.size(); ++k) {
        tailSum += yValues[finiteIndices[k]];
        usedTailCount++;
    }
    const double tailMean = usedTailCount ? tailSum / static_cast<double>(usedTailCount) : yValues[finiteIndices.back()];
    const double amplitudeGuess = y0 - tailMean;
    const double scale = std::max({std::abs(y0), std::abs(tailMean), 1.0});
    if(!(amplitudeGuess > scale * 1e-5)) {
        result.status = QStringLiteral("curve is not a positive decay");
        return result;
    }

    double minY = std::numeric_limits<double>::max();
    for(size_t index : finiteIndices)
        minY = std::min(minY, yValues[index]);

    const double offsetLow = tailMean - 0.75 * amplitudeGuess;
    double offsetHigh = std::min(tailMean + 0.35 * amplitudeGuess, y0 - scale * 1e-8);
    offsetHigh = std::min(offsetHigh, minY - scale * 1e-8);
    if(!(offsetLow < offsetHigh)) {
        result.status = QStringLiteral("no positive decay branch to fit");
        return result;
    }

    struct Candidate {
        double sse = std::numeric_limits<double>::infinity();
        double tau = std::numeric_limits<double>::quiet_NaN();
        double beta = std::numeric_limits<double>::quiet_NaN();
        double offset = std::numeric_limits<double>::quiet_NaN();
        double amplitude = std::numeric_limits<double>::quiet_NaN();
        double fitStart = std::numeric_limits<double>::quiet_NaN();
        double fitEnd = std::numeric_limits<double>::quiet_NaN();
        int pointCount = 0;
        double rSquared = std::numeric_limits<double>::quiet_NaN();
        double rmse = std::numeric_limits<double>::quiet_NaN();
    };

    Candidate best;
    constexpr int OffsetGridCount = 81;
    for(int gridIndex = 0; gridIndex < OffsetGridCount; ++gridIndex) {
        const double t = static_cast<double>(gridIndex) / static_cast<double>(OffsetGridCount - 1);
        const double offset = offsetLow + t * (offsetHigh - offsetLow);
        const double amplitude = y0 - offset;
        if(!(amplitude > scale * 1e-8) || !isFiniteFitValue(amplitude))
            continue;

        std::vector<double> logLag;
        std::vector<double> logLogDecay;
        std::vector<size_t> fitIndices;
        logLag.reserve(finiteIndices.size());
        logLogDecay.reserve(finiteIndices.size());
        fitIndices.reserve(finiteIndices.size());

        for(size_t index : finiteIndices) {
            const double lag = xValues[index] - x0;
            if(lag <= 0.0)
                continue;

            const double ratio = (yValues[index] - offset) / amplitude;
            if(!(ratio > 1e-6 && ratio < 0.995) || !isFiniteFitValue(ratio))
                continue;

            logLag.push_back(std::log(lag));
            logLogDecay.push_back(std::log(-std::log(ratio)));
            fitIndices.push_back(index);
        }

        if(logLag.size() < 5)
            continue;

        const double meanX = std::accumulate(logLag.begin(), logLag.end(), 0.0) / static_cast<double>(logLag.size());
        const double meanY = std::accumulate(logLogDecay.begin(), logLogDecay.end(), 0.0) / static_cast<double>(logLogDecay.size());
        double numerator = 0.0;
        double denominator = 0.0;
        for(size_t i = 0; i < logLag.size(); ++i) {
            const double dx = logLag[i] - meanX;
            numerator += dx * (logLogDecay[i] - meanY);
            denominator += dx * dx;
        }
        if(!(denominator > 0.0))
            continue;

        const double beta = numerator / denominator;
        if(!(beta >= 0.20 && beta <= 3.0) || !isFiniteFitValue(beta))
            continue;

        const double intercept = meanY - beta * meanX;
        const double tau = std::exp(-intercept / beta);
        if(!(tau > 0.0) || !isFiniteFitValue(tau))
            continue;

        std::vector<double> observed;
        observed.reserve(fitIndices.size() + 1);
        observed.push_back(y0);
        double sse = 0.0;
        for(size_t index : fitIndices) {
            const double lag = xValues[index] - x0;
            const double model = offset + amplitude * std::exp(-std::pow(lag / tau, beta));
            if(!isFiniteFitValue(model))
                continue;
            const double residual = yValues[index] - model;
            sse += residual * residual;
            observed.push_back(yValues[index]);
        }
        if(observed.size() < 6)
            continue;

        const double meanObserved = std::accumulate(observed.begin(), observed.end(), 0.0) / static_cast<double>(observed.size());
        double tss = 0.0;
        for(double value : observed) {
            const double diff = value - meanObserved;
            tss += diff * diff;
        }
        if(!(tss > scale * scale * 1e-12))
            continue;

        Candidate candidate;
        candidate.sse = sse;
        candidate.tau = tau;
        candidate.beta = beta;
        candidate.offset = offset;
        candidate.amplitude = amplitude;
        candidate.pointCount = static_cast<int>(observed.size());
        candidate.fitStart = xValues[firstIndex];
        candidate.fitEnd = xValues[fitIndices.back()];
        candidate.rSquared = 1.0 - sse / tss;
        candidate.rmse = std::sqrt(sse / static_cast<double>(observed.size()));

        if(candidate.sse < best.sse)
            best = candidate;
    }

    if(!isFiniteFitValue(best.sse)) {
        result.status = QStringLiteral("could not fit a positive decay branch");
        return result;
    }
    if(best.rSquared < 0.80) {
        result.status = QStringLiteral("poor stretched-exponential fit quality");
        result.rSquared = best.rSquared;
        result.rmse = best.rmse;
        return result;
    }

    result.valid = true;
    result.status = QStringLiteral("ok");
    result.amplitude = best.amplitude;
    result.tau = best.tau;
    result.beta = best.beta;
    result.offset = best.offset;
    result.integratedTime = best.tau / best.beta * std::tgamma(1.0 / best.beta);
    result.rSquared = best.rSquared;
    result.rmse = best.rmse;
    result.fitStart = best.fitStart;
    result.fitEnd = best.fitEnd;
    result.pointCount = best.pointCount;
    return result;
}

inline void setStretchedExponentialFitAttributes(DataCollection* collection,
                                                 const QString& prefix,
                                                 const StretchedExponentialFitResult& result,
                                                 const OOWeakRef<const PipelineNode>& createdByNode)
{
    if(!collection)
        return;

    auto setAttribute = [&](const QString& suffix, QVariant value) {
        const QString key = prefix + suffix;
        collection->setAttribute(QStringView(key), std::move(value), createdByNode);
    };

    setAttribute(QStringLiteral(".fit_valid"), result.valid ? 1.0 : 0.0);
    setAttribute(QStringLiteral(".fit_status"), result.status);
    setAttribute(QStringLiteral(".fit_curve"), result.curveLabel);
    setAttribute(QStringLiteral(".fit_point_count"), static_cast<double>(result.pointCount));
    if(!result.valid)
        return;

    setAttribute(QStringLiteral(".fit_tau"), result.tau);
    setAttribute(QStringLiteral(".fit_beta"), result.beta);
    setAttribute(QStringLiteral(".fit_offset"), result.offset);
    setAttribute(QStringLiteral(".fit_amplitude"), result.amplitude);
    setAttribute(QStringLiteral(".fit_integrated_time"), result.integratedTime);
    setAttribute(QStringLiteral(".fit_r_squared"), result.rSquared);
    setAttribute(QStringLiteral(".fit_rmse"), result.rmse);
    setAttribute(QStringLiteral(".fit_start_lag"), result.fitStart);
    setAttribute(QStringLiteral(".fit_end_lag"), result.fitEnd);
}

inline QString stretchedExponentialFitSummary(const PipelineFlowState& state,
                                              const PipelineNode* createdByNode,
                                              const QString& prefix,
                                              const QString& timeUnitLabel = QStringLiteral("frames"))
{
    auto attribute = [&](const QString& suffix) {
        const QString key = prefix + suffix;
        return state.getAttributeValue(createdByNode, QStringView(key));
    };

    const QVariant valid = attribute(QStringLiteral(".fit_valid"));
    if(!valid.isValid())
        return {};

    const QString curve = attribute(QStringLiteral(".fit_curve")).toString();
    const QString curveLabel = curve.isEmpty() ? QString{} : QStringLiteral(" (%1)").arg(curve);
    if(valid.toDouble() == 0.0) {
        const QString status = attribute(QStringLiteral(".fit_status")).toString();
        return status.isEmpty()
            ? QStringLiteral("Stretched-exp fit%1: not reliable").arg(curveLabel)
            : QStringLiteral("Stretched-exp fit%1: not reliable (%2)").arg(curveLabel, status);
    }

    const double tau = attribute(QStringLiteral(".fit_tau")).toDouble();
    const double beta = attribute(QStringLiteral(".fit_beta")).toDouble();
    const double offset = attribute(QStringLiteral(".fit_offset")).toDouble();
    const double integratedTime = attribute(QStringLiteral(".fit_integrated_time")).toDouble();
    const double rSquared = attribute(QStringLiteral(".fit_r_squared")).toDouble();
    const double startLag = attribute(QStringLiteral(".fit_start_lag")).toDouble();
    const double endLag = attribute(QStringLiteral(".fit_end_lag")).toDouble();

    return QStringLiteral("Stretched-exp fit%1: tau=%2 %3; beta=%4; Cinf=%5; tau_int=%6 %3; R2=%7\nFit range: %8-%9 %3")
        .arg(curveLabel)
        .arg(formatFitNumber(tau))
        .arg(timeUnitLabel)
        .arg(formatFitNumber(beta))
        .arg(formatFitNumber(offset))
        .arg(formatFitNumber(integratedTime))
        .arg(formatFitNumber(rSquared, 3))
        .arg(formatFitNumber(startLag))
        .arg(formatFitNumber(endLag));
}

}  // namespace Ovito
