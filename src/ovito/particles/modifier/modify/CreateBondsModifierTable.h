////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2025 OVITO GmbH, Germany
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
#include <ovito/particles/objects/ParticleType.h>

namespace Ovito {

namespace {
consteval auto getCovalentRadii()
{
    // Data from doi.org/10.1002/jcc.540120716
    // "Determination of Molecular Topology and Atomic Hybridization States from Heavy Atom Coordinates",
    // Elaine C. Meng* and Richard A. Lewis, 1991
    std::array<FloatType, (size_t)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES> covRadii{
        std::numeric_limits<FloatType>::quiet_NaN()};
    covRadii[(size_t)ParticleType::ChemicalElement::Ac] = 1.88;
    covRadii[(size_t)ParticleType::ChemicalElement::Er] = 1.73;
    covRadii[(size_t)ParticleType::ChemicalElement::Na] = 0.97;
    covRadii[(size_t)ParticleType::ChemicalElement::Sb] = 1.46;
    covRadii[(size_t)ParticleType::ChemicalElement::Ag] = 1.59;
    covRadii[(size_t)ParticleType::ChemicalElement::Eu] = 1.99;
    covRadii[(size_t)ParticleType::ChemicalElement::Nb] = 1.48;
    covRadii[(size_t)ParticleType::ChemicalElement::Sc] = 1.44;
    covRadii[(size_t)ParticleType::ChemicalElement::Al] = 1.35;
    covRadii[(size_t)ParticleType::ChemicalElement::F] = 0.64;
    covRadii[(size_t)ParticleType::ChemicalElement::Nd] = 1.81;
    covRadii[(size_t)ParticleType::ChemicalElement::Se] = 1.22;
    covRadii[(size_t)ParticleType::ChemicalElement::Am] = 1.51;
    covRadii[(size_t)ParticleType::ChemicalElement::Fe] = 1.34;
    covRadii[(size_t)ParticleType::ChemicalElement::Ni] = 1.50;
    covRadii[(size_t)ParticleType::ChemicalElement::Si] = 1.20;
    covRadii[(size_t)ParticleType::ChemicalElement::As] = 1.21;
    covRadii[(size_t)ParticleType::ChemicalElement::Ga] = 1.22;
    covRadii[(size_t)ParticleType::ChemicalElement::Np] = 1.55;
    covRadii[(size_t)ParticleType::ChemicalElement::Sm] = 1.80;
    covRadii[(size_t)ParticleType::ChemicalElement::Au] = 1.50;
    covRadii[(size_t)ParticleType::ChemicalElement::Gd] = 1.79;
    covRadii[(size_t)ParticleType::ChemicalElement::O] = 0.68;
    covRadii[(size_t)ParticleType::ChemicalElement::Sn] = 1.46;
    covRadii[(size_t)ParticleType::ChemicalElement::B] = 0.83;
    covRadii[(size_t)ParticleType::ChemicalElement::Ge] = 1.17;
    covRadii[(size_t)ParticleType::ChemicalElement::Os] = 1.37;
    covRadii[(size_t)ParticleType::ChemicalElement::Sr] = 1.12;
    covRadii[(size_t)ParticleType::ChemicalElement::Ba] = 1.34;
    covRadii[(size_t)ParticleType::ChemicalElement::H] = 0.23;
    covRadii[(size_t)ParticleType::ChemicalElement::P] = 1.05;
    covRadii[(size_t)ParticleType::ChemicalElement::Ta] = 1.43;
    covRadii[(size_t)ParticleType::ChemicalElement::Be] = 0.35;
    covRadii[(size_t)ParticleType::ChemicalElement::Hf] = 1.57;
    covRadii[(size_t)ParticleType::ChemicalElement::Pa] = 1.61;
    covRadii[(size_t)ParticleType::ChemicalElement::Tb] = 1.76;
    covRadii[(size_t)ParticleType::ChemicalElement::Bi] = 1.54;
    covRadii[(size_t)ParticleType::ChemicalElement::Hg] = 1.70;
    covRadii[(size_t)ParticleType::ChemicalElement::Pb] = 1.54;
    covRadii[(size_t)ParticleType::ChemicalElement::Tc] = 1.35;
    covRadii[(size_t)ParticleType::ChemicalElement::Br] = 1.21;
    covRadii[(size_t)ParticleType::ChemicalElement::Ho] = 1.74;
    covRadii[(size_t)ParticleType::ChemicalElement::Pd] = 1.50;
    covRadii[(size_t)ParticleType::ChemicalElement::Te] = 1.47;
    covRadii[(size_t)ParticleType::ChemicalElement::C] = 0.68;
    covRadii[(size_t)ParticleType::ChemicalElement::I] = 1.40;
    covRadii[(size_t)ParticleType::ChemicalElement::Pm] = 1.80;
    covRadii[(size_t)ParticleType::ChemicalElement::Th] = 1.79;
    covRadii[(size_t)ParticleType::ChemicalElement::Ca] = 0.99;
    covRadii[(size_t)ParticleType::ChemicalElement::In] = 1.63;
    covRadii[(size_t)ParticleType::ChemicalElement::Po] = 1.68;
    covRadii[(size_t)ParticleType::ChemicalElement::Ti] = 1.47;
    covRadii[(size_t)ParticleType::ChemicalElement::Cd] = 1.69;
    covRadii[(size_t)ParticleType::ChemicalElement::Ir] = 1.32;
    covRadii[(size_t)ParticleType::ChemicalElement::Pr] = 1.82;
    covRadii[(size_t)ParticleType::ChemicalElement::Tl] = 1.55;
    covRadii[(size_t)ParticleType::ChemicalElement::Ce] = 1.83;
    covRadii[(size_t)ParticleType::ChemicalElement::K] = 1.33;
    covRadii[(size_t)ParticleType::ChemicalElement::Pt] = 1.50;
    covRadii[(size_t)ParticleType::ChemicalElement::Tm] = 1.72;
    covRadii[(size_t)ParticleType::ChemicalElement::Cl] = 0.99;
    covRadii[(size_t)ParticleType::ChemicalElement::La] = 1.87;
    covRadii[(size_t)ParticleType::ChemicalElement::Pu] = 1.53;
    covRadii[(size_t)ParticleType::ChemicalElement::U] = 1.58;
    covRadii[(size_t)ParticleType::ChemicalElement::Co] = 1.33;
    covRadii[(size_t)ParticleType::ChemicalElement::Li] = 0.68;
    covRadii[(size_t)ParticleType::ChemicalElement::Ra] = 1.90;
    covRadii[(size_t)ParticleType::ChemicalElement::V] = 1.33;
    covRadii[(size_t)ParticleType::ChemicalElement::Cr] = 1.35;
    covRadii[(size_t)ParticleType::ChemicalElement::Lu] = 1.72;
    covRadii[(size_t)ParticleType::ChemicalElement::Rb] = 1.47;
    covRadii[(size_t)ParticleType::ChemicalElement::W] = 1.37;
    covRadii[(size_t)ParticleType::ChemicalElement::Cs] = 1.67;
    covRadii[(size_t)ParticleType::ChemicalElement::Mg] = 1.10;
    covRadii[(size_t)ParticleType::ChemicalElement::Re] = 1.35;
    covRadii[(size_t)ParticleType::ChemicalElement::Y] = 1.78;
    covRadii[(size_t)ParticleType::ChemicalElement::Cu] = 1.52;
    covRadii[(size_t)ParticleType::ChemicalElement::Mn] = 1.35;
    covRadii[(size_t)ParticleType::ChemicalElement::Rh] = 1.45;
    covRadii[(size_t)ParticleType::ChemicalElement::Yb] = 1.94;
    covRadii[(size_t)ParticleType::ChemicalElement::Mo] = 1.47;
    covRadii[(size_t)ParticleType::ChemicalElement::Ru] = 1.40;
    covRadii[(size_t)ParticleType::ChemicalElement::Zn] = 1.45;
    covRadii[(size_t)ParticleType::ChemicalElement::D] = 0.23;
    covRadii[(size_t)ParticleType::ChemicalElement::Dy] = 1.75;
    covRadii[(size_t)ParticleType::ChemicalElement::N] = 0.68;
    covRadii[(size_t)ParticleType::ChemicalElement::S] = 1.02;
    covRadii[(size_t)ParticleType::ChemicalElement::Zr] = 1.56;
    return covRadii;
}
inline constexpr auto _covalentRadii = getCovalentRadii();

consteval auto getMaxiumCoordination()
{
    // Adapted from supplementary material of doi.org/10.1002/jcc.24309
    // https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1002%2Fjcc.24309&file=jcc24309-sup-0001-suppinfo.pdf
    // Automatic Molecular Structure Perception for the Universal Force Field
    // Svetlana Artemova, Leonard Jaillet, and Stephane Redon
    // Journal of Computational Chemistry 2016, 37, 1191–1205
    std::array<uint8_t, (size_t)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES> maxCoord{0};
    maxCoord[(size_t)ParticleType::ChemicalElement::H] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::He] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Li] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Be] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::B] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::C] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::N] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::O] = 2;
    maxCoord[(size_t)ParticleType::ChemicalElement::F] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ne] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Na] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Mg] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Al] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Si] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::P] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::S] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Cl] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ar] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::K] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ca] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Sc] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ti] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::V] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Cr] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Mn] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Fe] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Co] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ni] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Cu] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Zn] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ga] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ge] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::As] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Se] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Br] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Kr] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Rb] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Sr] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Y] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Zr] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Nb] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Mo] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Tc] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ru] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Rh] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Pd] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ag] = 2;
    maxCoord[(size_t)ParticleType::ChemicalElement::Cd] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::In] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Sn] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Sb] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Te] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::I] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Xe] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Cs] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ba] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Hf] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ta] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::W] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Re] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Os] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ir] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Pt] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Au] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Hg] = 2;
    maxCoord[(size_t)ParticleType::ChemicalElement::Tl] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Pb] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Bi] = 3;
    maxCoord[(size_t)ParticleType::ChemicalElement::Po] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::At] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Rn] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Fr] = 1;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ra] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::La] = 4;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ce] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Pr] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Nd] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Pm] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Sm] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Eu] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Gd] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Tb] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Dy] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ho] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Er] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Tm] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Yb] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Lu] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Ac] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Th] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Pa] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::U] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Np] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Pu] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Am] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Cm] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Bk] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Cf] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Es] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Fm] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Md] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::No] = 6;
    maxCoord[(size_t)ParticleType::ChemicalElement::Lr] = 6;
    return maxCoord;
}

inline constexpr auto _maximumCoordination = getMaxiumCoordination();
};  // namespace

namespace CovalentRadii {
constexpr std::optional<FloatType> get(ParticleType::ChemicalElement element)
{
    OVITO_ASSERT((int)element >= 0 && (int)element < (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES);
    return (element == ParticleType::ChemicalElement::X || std::isnan(_covalentRadii[(size_t)element]))
               ? std::nullopt
               : std::optional<FloatType>{_covalentRadii[(size_t)element]};
}
constexpr std::optional<FloatType> getCleaveDistance(ParticleType::ChemicalElement elementA, ParticleType::ChemicalElement elementB)
{
    const std::optional<FloatType> radiusA = get(elementA);
    const std::optional<FloatType> radiusB = get(elementB);
    if(!radiusA || !radiusB) {
        return std::nullopt;
    }
    if((elementA == ParticleType::ChemicalElement::Al && elementB == ParticleType::ChemicalElement::Al) ||
       (elementA == ParticleType::ChemicalElement::B && elementB == ParticleType::ChemicalElement::B)) {
        return *radiusA + *radiusB - 0.2;
    }
    return *radiusA + *radiusB;
}
};  // namespace CovalentRadii

namespace MaximumCoordination {
consteval uint_fast8_t max() noexcept { return *std::ranges::max_element(_maximumCoordination); }

constexpr std::optional<uint_fast8_t> get(
    ParticleType::ChemicalElement element,
    std::optional<std::pair<ParticleType::ChemicalElement, ParticleType::ChemicalElement>> neighs = std::nullopt)
{
    if(neighs && element == ParticleType::ChemicalElement::H) {
        if(neighs->first == ParticleType::ChemicalElement::B && neighs->second == ParticleType::ChemicalElement::B) {
            return 2;
        }
    }
    OVITO_ASSERT((int)element >= 0 && (int)element < (int)ParticleType::ChemicalElement::NUMBER_OF_PREDEFINED_CHEMICAL_TYPES);
    const uint_fast8_t maxCoordination = _maximumCoordination[(size_t)element];
    return maxCoordination == 0 ? std::nullopt : std::optional<uint_fast8_t>{maxCoordination};
}
};  // namespace MaximumCoordination

namespace MaximumValence {
// Adapted from supplementary material of doi.org/10.1002/jcc.24309
// https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1002%2Fjcc.24309&file=jcc24309-sup-0001-suppinfo.pdf
// Automatic Molecular Structure Perception for the Universal Force Field
// Svetlana Artemova, Leonard Jaillet, and Stephane Redon
// Journal of Computational Chemistry 2016, 37, 1191–1205
constexpr std::optional<uint_fast8_t> get(ParticleType::ChemicalElement element, uint_fast8_t coordination)
{
    if(element == ParticleType::ChemicalElement::H) {
        if(coordination == 2)
            return 2;
        else
            return 1;
    }
    if(element == ParticleType::ChemicalElement::P) {
        if(coordination < 3) {
            return 4;
        }
        else {
            return 5;
        }
    }
    if(element == ParticleType::ChemicalElement::S) {
        if(coordination == 2) {
            return 4;
        }
        else {
            return 6;
        }
    }
    if(element == ParticleType::ChemicalElement::C) {
        return 4;
    }
    if(element == ParticleType::ChemicalElement::N) {
        return 4;
    }
    if(element == ParticleType::ChemicalElement::O) {
        return 3;
    }
    if(element == ParticleType::ChemicalElement::Si) {
        return 4;
    }
    return MaximumCoordination::get(element);
}
};  // namespace MaximumValence

};  // namespace Ovito