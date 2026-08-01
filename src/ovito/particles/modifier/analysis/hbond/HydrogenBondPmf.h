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
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace Ovito::HydrogenBondPmf {

inline constexpr double MinimumResolvedWellDepth = 0.25;

struct Parameters {
    double distanceMinimum = 0.0;
    double distanceMaximum = 5.0;
    double thetaMinimum = 0.0;
    double thetaMaximum = 180.0;
    int distanceBins = 80;
    int angleBins = 72;
    double distanceBandwidth = 0.1;
    double angleBandwidth = 4.0;
    double referenceShellFraction = 0.2;
};

struct Definition {
    Parameters parameters;
    double boundaryFreeEnergy = 0.0;
    double vicinityCutoff = 0.0;
    double referenceDistanceMinimum = 0.0;
    double referenceDensity = 0.0;
    double minimumFreeEnergy = 0.0;
    double minimumDistance = 0.0;
    double minimumTheta = 0.0;
    double minimumRequiredWellDepth = MinimumResolvedWellDepth;
    size_t basinBinCount = 0;
    size_t populatedBinCount = 0;
    std::vector<int64_t> counts;
    std::vector<double> smoothedReducedDensity;
    std::vector<double> freeEnergy;
    std::vector<char> inBasin;
};

inline size_t linearIndex(int distanceBin, int angleBin, int angleBins)
{
    return static_cast<size_t>(distanceBin) * static_cast<size_t>(angleBins)
         + static_cast<size_t>(angleBin);
}

inline int clampedBinIndex(double value, double minimum, double maximum, int binCount)
{
    if(!(value >= minimum) || value > maximum || !(maximum > minimum) || binCount <= 0)
        return -1;
    const double normalized = std::clamp((value - minimum) / (maximum - minimum),
                                         0.0,
                                         1.0 - std::numeric_limits<double>::epsilon());
    return std::clamp(static_cast<int>(std::floor(normalized * static_cast<double>(binCount))),
                      0,
                      binCount - 1);
}

namespace detail {

inline std::vector<double> gaussianKernel(double sigmaBins, int maximumRadius)
{
    if(!(sigmaBins > 0.0) || maximumRadius <= 0)
        return {1.0};

    const int radius = std::min(maximumRadius, std::max(1, static_cast<int>(std::ceil(4.0 * sigmaBins))));
    std::vector<double> kernel(static_cast<size_t>(2 * radius + 1));
    double sum = 0.0;
    for(int offset = -radius; offset <= radius; ++offset) {
        const double scaledOffset = static_cast<double>(offset) / sigmaBins;
        const double weight = std::exp(-0.5 * scaledOffset * scaledOffset);
        kernel[static_cast<size_t>(offset + radius)] = weight;
        sum += weight;
    }
    for(double& weight : kernel)
        weight /= sum;
    return kernel;
}

inline std::vector<double> convolveAxis(const std::vector<double>& input,
                                        int distanceBins,
                                        int angleBins,
                                        const std::vector<double>& kernel,
                                        bool distanceAxis)
{
    std::vector<double> output(input.size(), 0.0);
    const int radius = static_cast<int>(kernel.size() / 2);
    for(int distanceBin = 0; distanceBin < distanceBins; ++distanceBin) {
        for(int angleBin = 0; angleBin < angleBins; ++angleBin) {
            double sum = 0.0;
            for(int offset = -radius; offset <= radius; ++offset) {
                const int sourceDistanceBin = distanceAxis ? distanceBin + offset : distanceBin;
                const int sourceAngleBin = distanceAxis ? angleBin : angleBin + offset;
                if(sourceDistanceBin < 0 || sourceDistanceBin >= distanceBins
                   || sourceAngleBin < 0 || sourceAngleBin >= angleBins)
                    continue;
                sum += kernel[static_cast<size_t>(offset + radius)]
                     * input[linearIndex(sourceDistanceBin, sourceAngleBin, angleBins)];
            }
            output[linearIndex(distanceBin, angleBin, angleBins)] = sum;
        }
    }
    return output;
}

inline std::vector<double> smooth2d(const std::vector<double>& input,
                                    int distanceBins,
                                    int angleBins,
                                    double distanceSigmaBins,
                                    double angleSigmaBins)
{
    const std::vector<double> distanceKernel = gaussianKernel(distanceSigmaBins, distanceBins - 1);
    const std::vector<double> angleKernel = gaussianKernel(angleSigmaBins, angleBins - 1);
    return convolveAxis(
        convolveAxis(input, distanceBins, angleBins, distanceKernel, true),
        distanceBins,
        angleBins,
        angleKernel,
        false);
}

inline double weightedMedian(std::vector<std::pair<double, double>> values)
{
    values.erase(std::remove_if(values.begin(), values.end(), [](const auto& item) {
        return !std::isfinite(item.first) || !(item.second > 0.0) || !std::isfinite(item.second);
    }), values.end());
    if(values.empty())
        return std::numeric_limits<double>::quiet_NaN();

    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    double totalWeight = 0.0;
    for(const auto& [value, weight] : values)
        totalWeight += weight;
    double cumulativeWeight = 0.0;
    for(const auto& [value, weight] : values) {
        cumulativeWeight += weight;
        if(cumulativeWeight >= 0.5 * totalWeight)
            return value;
    }
    return values.back().first;
}

inline std::vector<int> connectedComponent(const std::vector<double>& freeEnergy,
                                           int distanceBins,
                                           int angleBins,
                                           int seedIndex,
                                           double threshold)
{
    std::vector<int> component;
    if(seedIndex < 0 || seedIndex >= static_cast<int>(freeEnergy.size())
       || !std::isfinite(freeEnergy[static_cast<size_t>(seedIndex)])
       || freeEnergy[static_cast<size_t>(seedIndex)] > threshold)
        return component;

    std::vector<char> visited(freeEnergy.size(), 0);
    std::queue<int> pending;
    pending.push(seedIndex);
    visited[static_cast<size_t>(seedIndex)] = 1;

    while(!pending.empty()) {
        const int index = pending.front();
        pending.pop();
        component.push_back(index);

        const int distanceBin = index / angleBins;
        const int angleBin = index % angleBins;
        for(int distanceOffset = -1; distanceOffset <= 1; ++distanceOffset) {
            for(int angleOffset = -1; angleOffset <= 1; ++angleOffset) {
                if(distanceOffset == 0 && angleOffset == 0)
                    continue;
                const int neighborDistanceBin = distanceBin + distanceOffset;
                const int neighborAngleBin = angleBin + angleOffset;
                if(neighborDistanceBin < 0 || neighborDistanceBin >= distanceBins
                   || neighborAngleBin < 0 || neighborAngleBin >= angleBins)
                    continue;

                const int neighborIndex = neighborDistanceBin * angleBins + neighborAngleBin;
                if(visited[static_cast<size_t>(neighborIndex)]
                   || !std::isfinite(freeEnergy[static_cast<size_t>(neighborIndex)])
                   || freeEnergy[static_cast<size_t>(neighborIndex)] > threshold)
                    continue;
                visited[static_cast<size_t>(neighborIndex)] = 1;
                pending.push(neighborIndex);
            }
        }
    }
    return component;
}

} // namespace detail

inline Definition buildDefinition(std::vector<int64_t> counts, const Parameters& parameters)
{
    if(parameters.distanceMinimum < 0.0 || parameters.distanceMaximum <= parameters.distanceMinimum)
        throw std::invalid_argument("The PMF distance interval is invalid.");
    if(parameters.thetaMinimum < 0.0 || parameters.thetaMaximum > 180.0
       || parameters.thetaMaximum <= parameters.thetaMinimum)
        throw std::invalid_argument("The PMF theta interval is invalid.");
    if(parameters.distanceBins < 4 || parameters.angleBins < 4)
        throw std::invalid_argument("PMF bin counts must be at least 4 in each dimension.");
    if(!(parameters.distanceBandwidth > 0.0) || !(parameters.angleBandwidth > 0.0))
        throw std::invalid_argument("PMF smoothing bandwidths must be positive.");
    if(parameters.referenceShellFraction < 0.05 || parameters.referenceShellFraction > 0.5)
        throw std::invalid_argument("The PMF reference-shell fraction must be in the range [0.05, 0.5].");

    const size_t expectedSize = static_cast<size_t>(parameters.distanceBins)
                              * static_cast<size_t>(parameters.angleBins);
    if(counts.size() != expectedSize)
        throw std::invalid_argument("The PMF count array size does not match the requested grid.");

    Definition definition;
    definition.parameters = parameters;
    definition.counts = std::move(counts);
    definition.smoothedReducedDensity.assign(expectedSize, 0.0);
    definition.freeEnergy.assign(expectedSize, std::numeric_limits<double>::infinity());
    definition.inBasin.assign(expectedSize, 0);
    definition.populatedBinCount = static_cast<size_t>(std::count_if(
        definition.counts.begin(), definition.counts.end(), [](int64_t count) { return count > 0; }));

    if(definition.populatedBinCount == 0)
        throw std::runtime_error("No triplets fall within the PMF interval.");

    const double distanceBinWidth =
        (parameters.distanceMaximum - parameters.distanceMinimum) / static_cast<double>(parameters.distanceBins);
    const double angleBinWidth =
        (parameters.thetaMaximum - parameters.thetaMinimum) / static_cast<double>(parameters.angleBins);
    constexpr double degreesToRadians = 0.017453292519943295769;

    std::vector<double> rawCounts(expectedSize);
    std::vector<double> exposure(expectedSize);
    for(int distanceBin = 0; distanceBin < parameters.distanceBins; ++distanceBin) {
        const double distanceLower = parameters.distanceMinimum + static_cast<double>(distanceBin) * distanceBinWidth;
        const double distanceUpper = distanceLower + distanceBinWidth;
        const double radialExposure =
            (distanceUpper * distanceUpper * distanceUpper
             - distanceLower * distanceLower * distanceLower) / 3.0;
        for(int angleBin = 0; angleBin < parameters.angleBins; ++angleBin) {
            const double angleLower =
                (parameters.thetaMinimum + static_cast<double>(angleBin) * angleBinWidth) * degreesToRadians;
            const double angleUpper = angleLower + angleBinWidth * degreesToRadians;
            const double angularExposure = std::max(0.0, std::cos(angleLower) - std::cos(angleUpper));
            const size_t index = linearIndex(distanceBin, angleBin, parameters.angleBins);
            rawCounts[index] = static_cast<double>(definition.counts[index]);
            exposure[index] = radialExposure * angularExposure;
        }
    }

    const std::vector<double> smoothedCounts = detail::smooth2d(
        rawCounts,
        parameters.distanceBins,
        parameters.angleBins,
        parameters.distanceBandwidth / distanceBinWidth,
        parameters.angleBandwidth / angleBinWidth);
    const std::vector<double> smoothedExposure = detail::smooth2d(
        exposure,
        parameters.distanceBins,
        parameters.angleBins,
        parameters.distanceBandwidth / distanceBinWidth,
        parameters.angleBandwidth / angleBinWidth);

    for(size_t index = 0; index < expectedSize; ++index) {
        if(smoothedCounts[index] > 0.0 && smoothedExposure[index] > 0.0)
            definition.smoothedReducedDensity[index] = smoothedCounts[index] / smoothedExposure[index];
    }

    definition.referenceDistanceMinimum =
        parameters.distanceMaximum
        - parameters.referenceShellFraction * (parameters.distanceMaximum - parameters.distanceMinimum);
    std::vector<std::pair<double, double>> referenceValues;
    referenceValues.reserve(static_cast<size_t>(parameters.angleBins)
                            * static_cast<size_t>(std::ceil(parameters.referenceShellFraction * parameters.distanceBins)));
    for(int distanceBin = 0; distanceBin < parameters.distanceBins; ++distanceBin) {
        const double distanceCenter =
            parameters.distanceMinimum + (static_cast<double>(distanceBin) + 0.5) * distanceBinWidth;
        if(distanceCenter < definition.referenceDistanceMinimum)
            continue;
        for(int angleBin = 0; angleBin < parameters.angleBins; ++angleBin) {
            const size_t index = linearIndex(distanceBin, angleBin, parameters.angleBins);
            if(definition.smoothedReducedDensity[index] > 0.0)
                referenceValues.emplace_back(definition.smoothedReducedDensity[index], smoothedExposure[index]);
        }
    }
    definition.referenceDensity = detail::weightedMedian(std::move(referenceValues));
    if(!(definition.referenceDensity > 0.0) || !std::isfinite(definition.referenceDensity))
        throw std::runtime_error("The outer PMF reference shell contains insufficient data.");

    int minimumIndex = -1;
    definition.minimumFreeEnergy = std::numeric_limits<double>::infinity();
    for(int distanceBin = 0; distanceBin < parameters.distanceBins; ++distanceBin) {
        const double distanceCenter =
            parameters.distanceMinimum + (static_cast<double>(distanceBin) + 0.5) * distanceBinWidth;
        for(int angleBin = 0; angleBin < parameters.angleBins; ++angleBin) {
            const size_t index = linearIndex(distanceBin, angleBin, parameters.angleBins);
            const double density = definition.smoothedReducedDensity[index];
            if(!(density > 0.0))
                continue;
            const double value = -std::log(density / definition.referenceDensity);
            definition.freeEnergy[index] = value;
            if(distanceCenter < definition.referenceDistanceMinimum && value < definition.minimumFreeEnergy) {
                definition.minimumFreeEnergy = value;
                definition.minimumDistance = distanceCenter;
                definition.minimumTheta =
                    parameters.thetaMinimum + (static_cast<double>(angleBin) + 0.5) * angleBinWidth;
                minimumIndex = static_cast<int>(index);
            }
        }
    }

    if(minimumIndex < 0 || !(definition.minimumFreeEnergy < -MinimumResolvedWellDepth))
        throw std::runtime_error(
            "No PMF well at least 0.25 kBT below the noninteracting reference level was resolved.");

    const std::vector<int> basin = detail::connectedComponent(
        definition.freeEnergy,
        parameters.distanceBins,
        parameters.angleBins,
        minimumIndex,
        0.0);
    if(basin.size() < 2)
        throw std::runtime_error("The PMF well is too small to define a resolved hydrogen-bond basin.");

    for(int index : basin) {
        const int distanceBin = index / parameters.angleBins;
        if(distanceBin == parameters.distanceBins - 1)
            throw std::runtime_error(
                "The PMF basin reaches the outer distance boundary; increase the PMF distance maximum.");
        definition.inBasin[static_cast<size_t>(index)] = 1;
        const double distanceUpper =
            parameters.distanceMinimum + static_cast<double>(distanceBin + 1) * distanceBinWidth;
        definition.vicinityCutoff = std::max(definition.vicinityCutoff, distanceUpper);
    }
    definition.boundaryFreeEnergy = 0.0;
    definition.basinBinCount = basin.size();
    return definition;
}

} // namespace Ovito::HydrogenBondPmf
