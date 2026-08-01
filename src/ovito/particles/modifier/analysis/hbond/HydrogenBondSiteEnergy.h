////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2026 OVITO GmbH, Germany
//
//  This file is part of OVITO (Open Visualization Tool).
//
////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace Ovito::HydrogenBondSiteEnergy {

struct Parameters {
    double coulombConstant = 0.0;
    double relativePermittivity = 1.0;
    double donorEpsilon = 0.0;
    double donorSigma = 0.0;
    double hydrogenEpsilon = 0.0;
    double hydrogenSigma = 0.0;
    double acceptorEpsilon = 0.0;
    double acceptorSigma = 0.0;
};

struct Components {
    double coulomb = 0.0;
    double lennardJones = 0.0;
    double total = 0.0;
};

enum class AutomaticCutoffStatus {
    Success,
    TooFewSamples,
    DegenerateDistribution,
    NoResolvedMinimum
};

struct AutomaticCutoffResult {
    AutomaticCutoffStatus status = AutomaticCutoffStatus::TooFewSamples;
    size_t sampleCount = 0;
    double cutoff = std::numeric_limits<double>::quiet_NaN();
    double lowerEnergyPeak = std::numeric_limits<double>::quiet_NaN();
    double upperEnergyPeak = std::numeric_limits<double>::quiet_NaN();
    double bandwidth = std::numeric_limits<double>::quiet_NaN();
    double valleyToPeakRatio = std::numeric_limits<double>::quiet_NaN();
    double lowerEnergyFraction = std::numeric_limits<double>::quiet_NaN();
    std::vector<double> binCenters;
    std::vector<double> probabilityDensity;
};

inline double sortedQuantile(const std::vector<double>& sortedValues, double probability)
{
    if(sortedValues.empty())
        return std::numeric_limits<double>::quiet_NaN();

    const double position = std::clamp(probability, 0.0, 1.0)
                          * static_cast<double>(sortedValues.size() - 1);
    const size_t lower = static_cast<size_t>(std::floor(position));
    const size_t upper = std::min(lower + 1, sortedValues.size() - 1);
    const double fraction = position - static_cast<double>(lower);
    return sortedValues[lower] * (1.0 - fraction) + sortedValues[upper] * fraction;
}

inline AutomaticCutoffResult findAutomaticCutoff(const std::vector<double>& inputEnergies,
                                                 size_t requestedBinCount = 256)
{
    constexpr size_t minimumSampleCount = 500;
    constexpr double minimumPopulationFraction = 0.05;
    constexpr double maximumValleyRatio = 0.80;
    constexpr double minimumPeakSeparationInBandwidths = 2.5;

    AutomaticCutoffResult result;
    std::vector<double> energies;
    energies.reserve(inputEnergies.size());
    for(double value : inputEnergies) {
        if(std::isfinite(value))
            energies.push_back(value);
    }
    std::sort(energies.begin(), energies.end());
    result.sampleCount = energies.size();
    if(energies.size() < 2)
        return result;

    const double rangeMinimum = sortedQuantile(energies, 0.005);
    const double rangeMaximum = sortedQuantile(energies, 0.995);
    const double range = rangeMaximum - rangeMinimum;
    if(!std::isfinite(range) || range <= std::numeric_limits<double>::epsilon()
       * std::max({1.0, std::abs(rangeMinimum), std::abs(rangeMaximum)})) {
        result.status = AutomaticCutoffStatus::DegenerateDistribution;
        return result;
    }

    const size_t binCount = std::clamp<size_t>(requestedBinCount, 64, 1024);
    const double binWidth = range / static_cast<double>(binCount);
    result.binCenters.resize(binCount);
    std::vector<double> rawDensity(binCount, 0.0);
    for(size_t bin = 0; bin < binCount; ++bin)
        result.binCenters[bin] = rangeMinimum + (static_cast<double>(bin) + 0.5) * binWidth;

    size_t inRangeCount = 0;
    for(double energy : energies) {
        if(energy < rangeMinimum || energy > rangeMaximum)
            continue;
        size_t bin = static_cast<size_t>((energy - rangeMinimum) / binWidth);
        if(bin >= binCount)
            bin = binCount - 1;
        rawDensity[bin] += 1.0;
        inRangeCount++;
    }
    if(inRangeCount < 2) {
        result.status = AutomaticCutoffStatus::DegenerateDistribution;
        return result;
    }
    for(double& value : rawDensity)
        value /= static_cast<double>(inRangeCount) * binWidth;

    const double firstQuartile = sortedQuantile(energies, 0.25);
    const double thirdQuartile = sortedQuantile(energies, 0.75);
    const double robustSigma = (thirdQuartile - firstQuartile) / 1.349;
    double trimmedSum = 0.0;
    size_t deviationCount = 0;
    for(double energy : energies) {
        if(energy >= rangeMinimum && energy <= rangeMaximum) {
            trimmedSum += energy;
            deviationCount++;
        }
    }
    const double trimmedMean = deviationCount > 0
        ? trimmedSum / static_cast<double>(deviationCount)
        : 0.0;
    double squaredDeviationSum = 0.0;
    for(double energy : energies) {
        if(energy < rangeMinimum || energy > rangeMaximum)
            continue;
        const double delta = energy - trimmedMean;
        squaredDeviationSum += delta * delta;
    }
    const double rmsScale = deviationCount > 1
        ? std::sqrt(squaredDeviationSum / static_cast<double>(deviationCount - 1))
        : 0.0;
    double scale = robustSigma > 0.0 && rmsScale > 0.0
        ? std::min(robustSigma, rmsScale)
        : std::max(robustSigma, rmsScale);
    if(!(scale > 0.0) || !std::isfinite(scale))
        scale = range / 6.0;

    // Silverman's robust bandwidth, bounded to preserve real valleys without fitting bin noise.
    result.bandwidth = 0.9 * scale * std::pow(static_cast<double>(energies.size()), -0.2);
    result.bandwidth = std::clamp(result.bandwidth, 1.5 * binWidth, 0.10 * range);
    const double sigmaBins = result.bandwidth / binWidth;
    const int kernelRadius = std::max(1, static_cast<int>(std::ceil(4.0 * sigmaBins)));

    result.probabilityDensity.assign(binCount, 0.0);
    for(size_t targetBin = 0; targetBin < binCount; ++targetBin) {
        double weightedDensity = 0.0;
        double weightSum = 0.0;
        const int firstBin = std::max(0, static_cast<int>(targetBin) - kernelRadius);
        const int lastBin = std::min(static_cast<int>(binCount) - 1,
                                     static_cast<int>(targetBin) + kernelRadius);
        for(int sourceBin = firstBin; sourceBin <= lastBin; ++sourceBin) {
            const double offset = (static_cast<double>(sourceBin) - static_cast<double>(targetBin)) / sigmaBins;
            const double weight = std::exp(-0.5 * offset * offset);
            weightedDensity += rawDensity[static_cast<size_t>(sourceBin)] * weight;
            weightSum += weight;
        }
        result.probabilityDensity[targetBin] = weightSum > 0.0 ? weightedDensity / weightSum : 0.0;
    }

    if(energies.size() < minimumSampleCount) {
        result.status = AutomaticCutoffStatus::TooFewSamples;
        return result;
    }

    const double maximumDensity = *std::max_element(result.probabilityDensity.begin(),
                                                    result.probabilityDensity.end());
    if(!(maximumDensity > 0.0)) {
        result.status = AutomaticCutoffStatus::DegenerateDistribution;
        return result;
    }

    std::vector<size_t> peaks;
    for(size_t bin = 1; bin + 1 < binCount; ++bin) {
        const double previous = result.probabilityDensity[bin - 1];
        const double current = result.probabilityDensity[bin];
        const double next = result.probabilityDensity[bin + 1];
        if(current >= previous && current > next && current >= 0.05 * maximumDensity)
            peaks.push_back(bin);
    }

    double bestScore = -1.0;
    size_t bestLowerPeak = 0;
    size_t bestUpperPeak = 0;
    size_t bestValley = 0;
    double bestLowerFraction = 0.0;
    double bestValleyRatio = 1.0;
    for(size_t lowerIndex = 0; lowerIndex < peaks.size(); ++lowerIndex) {
        for(size_t upperIndex = lowerIndex + 1; upperIndex < peaks.size(); ++upperIndex) {
            const size_t lowerPeak = peaks[lowerIndex];
            const size_t upperPeak = peaks[upperIndex];
            const double separation = result.binCenters[upperPeak] - result.binCenters[lowerPeak];
            if(separation < minimumPeakSeparationInBandwidths * result.bandwidth)
                continue;

            const auto valleyIterator = std::min_element(
                result.probabilityDensity.begin() + static_cast<ptrdiff_t>(lowerPeak + 1),
                result.probabilityDensity.begin() + static_cast<ptrdiff_t>(upperPeak));
            if(valleyIterator == result.probabilityDensity.begin() + static_cast<ptrdiff_t>(upperPeak))
                continue;
            const size_t valley = static_cast<size_t>(
                std::distance(result.probabilityDensity.begin(), valleyIterator));
            const double smallerPeakDensity = std::min(result.probabilityDensity[lowerPeak],
                                                       result.probabilityDensity[upperPeak]);
            if(!(smallerPeakDensity > 0.0))
                continue;
            const double valleyRatio = result.probabilityDensity[valley] / smallerPeakDensity;
            if(valleyRatio > maximumValleyRatio)
                continue;

            const double candidateCutoff = result.binCenters[valley];
            const double lowerFraction = static_cast<double>(
                std::upper_bound(energies.begin(), energies.end(), candidateCutoff) - energies.begin())
                / static_cast<double>(energies.size());
            if(lowerFraction < minimumPopulationFraction
               || lowerFraction > 1.0 - minimumPopulationFraction)
                continue;

            const double score = (1.0 - valleyRatio)
                               * smallerPeakDensity
                               * std::sqrt(lowerFraction * (1.0 - lowerFraction))
                               * (separation / range);
            if(score > bestScore) {
                bestScore = score;
                bestLowerPeak = lowerPeak;
                bestUpperPeak = upperPeak;
                bestValley = valley;
                bestLowerFraction = lowerFraction;
                bestValleyRatio = valleyRatio;
            }
        }
    }

    if(bestScore < 0.0) {
        result.status = AutomaticCutoffStatus::NoResolvedMinimum;
        return result;
    }

    result.status = AutomaticCutoffStatus::Success;
    result.cutoff = result.binCenters[bestValley];
    result.lowerEnergyPeak = result.binCenters[bestLowerPeak];
    result.upperEnergyPeak = result.binCenters[bestUpperPeak];
    result.valleyToPeakRatio = bestValleyRatio;
    result.lowerEnergyFraction = bestLowerFraction;
    return result;
}

inline double mixedLennardJones(double distance,
                                double epsilon1,
                                double sigma1,
                                double epsilon2,
                                double sigma2)
{
    if(distance <= 0.0 || epsilon1 <= 0.0 || epsilon2 <= 0.0 || sigma1 <= 0.0 || sigma2 <= 0.0)
        return 0.0;

    const double epsilon = std::sqrt(epsilon1 * epsilon2);
    const double sigma = 0.5 * (sigma1 + sigma2);
    const double ratio2 = (sigma * sigma) / (distance * distance);
    const double ratio6 = ratio2 * ratio2 * ratio2;
    return 4.0 * epsilon * (ratio6 * ratio6 - ratio6);
}

inline Components evaluate(double donorAcceptorDistance,
                           double hydrogenAcceptorDistance,
                           double donorCharge,
                           double hydrogenCharge,
                           double acceptorCharge,
                           const Parameters& parameters)
{
    Components result;
    const double chargeFactor = parameters.coulombConstant
                              * acceptorCharge
                              / parameters.relativePermittivity;
    result.coulomb = chargeFactor
                   * (donorCharge / donorAcceptorDistance
                      + hydrogenCharge / hydrogenAcceptorDistance);
    result.lennardJones =
        mixedLennardJones(donorAcceptorDistance,
                          parameters.donorEpsilon,
                          parameters.donorSigma,
                          parameters.acceptorEpsilon,
                          parameters.acceptorSigma)
      + mixedLennardJones(hydrogenAcceptorDistance,
                          parameters.hydrogenEpsilon,
                          parameters.hydrogenSigma,
                          parameters.acceptorEpsilon,
                          parameters.acceptorSigma);
    result.total = result.coulomb + result.lennardJones;
    return result;
}

} // namespace Ovito::HydrogenBondSiteEnergy
