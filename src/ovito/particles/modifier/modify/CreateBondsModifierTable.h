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

namespace Ovito {

namespace {

struct AtomicNumber {
    std::string_view name;
    uint8_t number;
};

consteval auto getAtomicNumbers()
{
    auto atomicNumbers = std::to_array({
        AtomicNumber{.name = "H", .number = 1},    AtomicNumber{.name = "He", .number = 2},   AtomicNumber{.name = "Li", .number = 3},
        AtomicNumber{.name = "Be", .number = 4},   AtomicNumber{.name = "B", .number = 5},    AtomicNumber{.name = "C", .number = 6},
        AtomicNumber{.name = "N", .number = 7},    AtomicNumber{.name = "O", .number = 8},    AtomicNumber{.name = "F", .number = 9},
        AtomicNumber{.name = "Ne", .number = 10},  AtomicNumber{.name = "Na", .number = 11},  AtomicNumber{.name = "Mg", .number = 12},
        AtomicNumber{.name = "Al", .number = 13},  AtomicNumber{.name = "Si", .number = 14},  AtomicNumber{.name = "P", .number = 15},
        AtomicNumber{.name = "S", .number = 16},   AtomicNumber{.name = "Cl", .number = 17},  AtomicNumber{.name = "Ar", .number = 18},
        AtomicNumber{.name = "K", .number = 19},   AtomicNumber{.name = "Ca", .number = 20},  AtomicNumber{.name = "Sc", .number = 21},
        AtomicNumber{.name = "Ti", .number = 22},  AtomicNumber{.name = "V", .number = 23},   AtomicNumber{.name = "Cr", .number = 24},
        AtomicNumber{.name = "Mn", .number = 25},  AtomicNumber{.name = "Fe", .number = 26},  AtomicNumber{.name = "Co", .number = 27},
        AtomicNumber{.name = "Ni", .number = 28},  AtomicNumber{.name = "Cu", .number = 29},  AtomicNumber{.name = "Zn", .number = 30},
        AtomicNumber{.name = "Ga", .number = 31},  AtomicNumber{.name = "Ge", .number = 32},  AtomicNumber{.name = "As", .number = 33},
        AtomicNumber{.name = "Se", .number = 34},  AtomicNumber{.name = "Br", .number = 35},  AtomicNumber{.name = "Kr", .number = 36},
        AtomicNumber{.name = "Rb", .number = 37},  AtomicNumber{.name = "Sr", .number = 38},  AtomicNumber{.name = "Y", .number = 39},
        AtomicNumber{.name = "Zr", .number = 40},  AtomicNumber{.name = "Nb", .number = 41},  AtomicNumber{.name = "Mo", .number = 42},
        AtomicNumber{.name = "Tc", .number = 43},  AtomicNumber{.name = "Ru", .number = 44},  AtomicNumber{.name = "Rh", .number = 45},
        AtomicNumber{.name = "Pd", .number = 46},  AtomicNumber{.name = "Ag", .number = 47},  AtomicNumber{.name = "Cd", .number = 48},
        AtomicNumber{.name = "In", .number = 49},  AtomicNumber{.name = "Sn", .number = 50},  AtomicNumber{.name = "Sb", .number = 51},
        AtomicNumber{.name = "Te", .number = 52},  AtomicNumber{.name = "I", .number = 53},   AtomicNumber{.name = "Xe", .number = 54},
        AtomicNumber{.name = "Cs", .number = 55},  AtomicNumber{.name = "Ba", .number = 56},  AtomicNumber{.name = "La", .number = 57},
        AtomicNumber{.name = "Ce", .number = 58},  AtomicNumber{.name = "Pr", .number = 59},  AtomicNumber{.name = "Nd", .number = 60},
        AtomicNumber{.name = "Pm", .number = 61},  AtomicNumber{.name = "Sm", .number = 62},  AtomicNumber{.name = "Eu", .number = 63},
        AtomicNumber{.name = "Gd", .number = 64},  AtomicNumber{.name = "Tb", .number = 65},  AtomicNumber{.name = "Dy", .number = 66},
        AtomicNumber{.name = "Ho", .number = 67},  AtomicNumber{.name = "Er", .number = 68},  AtomicNumber{.name = "Tm", .number = 69},
        AtomicNumber{.name = "Yb", .number = 70},  AtomicNumber{.name = "Lu", .number = 71},  AtomicNumber{.name = "Hf", .number = 72},
        AtomicNumber{.name = "Ta", .number = 73},  AtomicNumber{.name = "W", .number = 74},   AtomicNumber{.name = "Re", .number = 75},
        AtomicNumber{.name = "Os", .number = 76},  AtomicNumber{.name = "Ir", .number = 77},  AtomicNumber{.name = "Pt", .number = 78},
        AtomicNumber{.name = "Au", .number = 79},  AtomicNumber{.name = "Hg", .number = 80},  AtomicNumber{.name = "Tl", .number = 81},
        AtomicNumber{.name = "Pb", .number = 82},  AtomicNumber{.name = "Bi", .number = 83},  AtomicNumber{.name = "Po", .number = 84},
        AtomicNumber{.name = "At", .number = 85},  AtomicNumber{.name = "Rn", .number = 86},  AtomicNumber{.name = "Fr", .number = 87},
        AtomicNumber{.name = "Ra", .number = 88},  AtomicNumber{.name = "Ac", .number = 89},  AtomicNumber{.name = "Th", .number = 90},
        AtomicNumber{.name = "Pa", .number = 91},  AtomicNumber{.name = "U", .number = 92},   AtomicNumber{.name = "Np", .number = 93},
        AtomicNumber{.name = "Pu", .number = 94},  AtomicNumber{.name = "Am", .number = 95},  AtomicNumber{.name = "Cm", .number = 96},
        AtomicNumber{.name = "Bk", .number = 97},  AtomicNumber{.name = "Cf", .number = 98},  AtomicNumber{.name = "Es", .number = 99},
        AtomicNumber{.name = "Fm", .number = 100}, AtomicNumber{.name = "Md", .number = 101}, AtomicNumber{.name = "No", .number = 102},
        AtomicNumber{.name = "Lr", .number = 103}, AtomicNumber{.name = "D", .number = 0},
    });
    std::ranges::sort(atomicNumbers, std::less<>{}, &AtomicNumber::name);
    return atomicNumbers;
}

constexpr auto _atomicNumbers = getAtomicNumbers();
static_assert(std::ranges::is_sorted(_atomicNumbers, std::less<>{}, &AtomicNumber::name));
}  // namespace

namespace AtomicNumbers {
constexpr std::string_view get(uint8_t number)
{
    const auto* const it = std::ranges::find(_atomicNumbers, number, &AtomicNumber::number);
    return it != _atomicNumbers.end() ? it->name : std::string_view();
}

constexpr std::optional<uint8_t> get(std::string_view element)
{
    const auto* const it = std::ranges::lower_bound(_atomicNumbers, element, std::less<>{}, &AtomicNumber::name);
    if(it != _atomicNumbers.end() && it->name == element) {
        return it->number;
    }
    else {
        return std::nullopt;
    }
}

consteval uint8_t cget(std::string_view element)
{
    const auto* const it = std::ranges::lower_bound(_atomicNumbers, element, std::less<>{}, &AtomicNumber::name);
    if(it != _atomicNumbers.end() && it->name == element) {
        return it->number;
    }
    return -1;
}
};  // namespace AtomicNumbers

namespace {
consteval auto getCovalentRadii()
{
    // Data from doi.org/10.1002/jcc.540120716
    // "Determination of Molecular Topology and Atomic Hybridization States from Heavy Atom Coordinates",
    // Elaine C. Meng* and Richard A. Lewis, 1991
    std::array<FloatType, _atomicNumbers.size()> covRadii{std::numeric_limits<FloatType>::quiet_NaN()};
    covRadii[AtomicNumbers::cget("Ac")] = 1.88;
    covRadii[AtomicNumbers::cget("Er")] = 1.73;
    covRadii[AtomicNumbers::cget("Na")] = 0.97;
    covRadii[AtomicNumbers::cget("Sb")] = 1.46;
    covRadii[AtomicNumbers::cget("Ag")] = 1.59;
    covRadii[AtomicNumbers::cget("Eu")] = 1.99;
    covRadii[AtomicNumbers::cget("Nb")] = 1.48;
    covRadii[AtomicNumbers::cget("Sc")] = 1.44;
    covRadii[AtomicNumbers::cget("Al")] = 1.35;
    covRadii[AtomicNumbers::cget("F")] = 0.64;
    covRadii[AtomicNumbers::cget("Nd")] = 1.81;
    covRadii[AtomicNumbers::cget("Se")] = 1.22;
    covRadii[AtomicNumbers::cget("Am")] = 1.51;
    covRadii[AtomicNumbers::cget("Fe")] = 1.34;
    covRadii[AtomicNumbers::cget("Ni")] = 1.50;
    covRadii[AtomicNumbers::cget("Si")] = 1.20;
    covRadii[AtomicNumbers::cget("As")] = 1.21;
    covRadii[AtomicNumbers::cget("Ga")] = 1.22;
    covRadii[AtomicNumbers::cget("Np")] = 1.55;
    covRadii[AtomicNumbers::cget("Sm")] = 1.80;
    covRadii[AtomicNumbers::cget("Au")] = 1.50;
    covRadii[AtomicNumbers::cget("Gd")] = 1.79;
    covRadii[AtomicNumbers::cget("O")] = 0.68;
    covRadii[AtomicNumbers::cget("Sn")] = 1.46;
    covRadii[AtomicNumbers::cget("B")] = 0.83;
    covRadii[AtomicNumbers::cget("Ge")] = 1.17;
    covRadii[AtomicNumbers::cget("Os")] = 1.37;
    covRadii[AtomicNumbers::cget("Sr")] = 1.12;
    covRadii[AtomicNumbers::cget("Ba")] = 1.34;
    covRadii[AtomicNumbers::cget("H")] = 0.23;
    covRadii[AtomicNumbers::cget("P")] = 1.05;
    covRadii[AtomicNumbers::cget("Ta")] = 1.43;
    covRadii[AtomicNumbers::cget("Be")] = 0.35;
    covRadii[AtomicNumbers::cget("Hf")] = 1.57;
    covRadii[AtomicNumbers::cget("Pa")] = 1.61;
    covRadii[AtomicNumbers::cget("Tb")] = 1.76;
    covRadii[AtomicNumbers::cget("Bi")] = 1.54;
    covRadii[AtomicNumbers::cget("Hg")] = 1.70;
    covRadii[AtomicNumbers::cget("Pb")] = 1.54;
    covRadii[AtomicNumbers::cget("Tc")] = 1.35;
    covRadii[AtomicNumbers::cget("Br")] = 1.21;
    covRadii[AtomicNumbers::cget("Ho")] = 1.74;
    covRadii[AtomicNumbers::cget("Pd")] = 1.50;
    covRadii[AtomicNumbers::cget("Te")] = 1.47;
    covRadii[AtomicNumbers::cget("C")] = 0.68;
    covRadii[AtomicNumbers::cget("I")] = 1.40;
    covRadii[AtomicNumbers::cget("Pm")] = 1.80;
    covRadii[AtomicNumbers::cget("Th")] = 1.79;
    covRadii[AtomicNumbers::cget("Ca")] = 0.99;
    covRadii[AtomicNumbers::cget("In")] = 1.63;
    covRadii[AtomicNumbers::cget("Po")] = 1.68;
    covRadii[AtomicNumbers::cget("Ti")] = 1.47;
    covRadii[AtomicNumbers::cget("Cd")] = 1.69;
    covRadii[AtomicNumbers::cget("Ir")] = 1.32;
    covRadii[AtomicNumbers::cget("Pr")] = 1.82;
    covRadii[AtomicNumbers::cget("Tl")] = 1.55;
    covRadii[AtomicNumbers::cget("Ce")] = 1.83;
    covRadii[AtomicNumbers::cget("K")] = 1.33;
    covRadii[AtomicNumbers::cget("Pt")] = 1.50;
    covRadii[AtomicNumbers::cget("Tm")] = 1.72;
    covRadii[AtomicNumbers::cget("Cl")] = 0.99;
    covRadii[AtomicNumbers::cget("La")] = 1.87;
    covRadii[AtomicNumbers::cget("Pu")] = 1.53;
    covRadii[AtomicNumbers::cget("U")] = 1.58;
    covRadii[AtomicNumbers::cget("Co")] = 1.33;
    covRadii[AtomicNumbers::cget("Li")] = 0.68;
    covRadii[AtomicNumbers::cget("Ra")] = 1.90;
    covRadii[AtomicNumbers::cget("V")] = 1.33;
    covRadii[AtomicNumbers::cget("Cr")] = 1.35;
    covRadii[AtomicNumbers::cget("Lu")] = 1.72;
    covRadii[AtomicNumbers::cget("Rb")] = 1.47;
    covRadii[AtomicNumbers::cget("W")] = 1.37;
    covRadii[AtomicNumbers::cget("Cs")] = 1.67;
    covRadii[AtomicNumbers::cget("Mg")] = 1.10;
    covRadii[AtomicNumbers::cget("Re")] = 1.35;
    covRadii[AtomicNumbers::cget("Y")] = 1.78;
    covRadii[AtomicNumbers::cget("Cu")] = 1.52;
    covRadii[AtomicNumbers::cget("Mn")] = 1.35;
    covRadii[AtomicNumbers::cget("Rh")] = 1.45;
    covRadii[AtomicNumbers::cget("Yb")] = 1.94;
    covRadii[AtomicNumbers::cget("Mo")] = 1.47;
    covRadii[AtomicNumbers::cget("Ru")] = 1.40;
    covRadii[AtomicNumbers::cget("Zn")] = 1.45;
    covRadii[AtomicNumbers::cget("D")] = 0.23;
    covRadii[AtomicNumbers::cget("Dy")] = 1.75;
    covRadii[AtomicNumbers::cget("N")] = 0.68;
    covRadii[AtomicNumbers::cget("S")] = 1.02;
    covRadii[AtomicNumbers::cget("Zr")] = 1.56;
    return covRadii;
}
constexpr auto _covalentRadii = getCovalentRadii();

consteval auto getMaxiumCoordination()
{
    // Adapted from supplementary material of doi.org/10.1002/jcc.24309
    // https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1002%2Fjcc.24309&file=jcc24309-sup-0001-suppinfo.pdf
    // Automatic Molecular Structure Perception for the Universal Force Field
    // Svetlana Artemova, Leonard Jaillet, and Stephane Redon
    // Journal of Computational Chemistry 2016, 37, 1191–1205
    std::array<uint8_t, _atomicNumbers.size()> maxCoord{0};
    maxCoord[AtomicNumbers::cget("H")] = 1;
    maxCoord[AtomicNumbers::cget("He")] = 4;
    maxCoord[AtomicNumbers::cget("Li")] = 1;
    maxCoord[AtomicNumbers::cget("Be")] = 4;
    maxCoord[AtomicNumbers::cget("B")] = 4;
    maxCoord[AtomicNumbers::cget("C")] = 4;
    maxCoord[AtomicNumbers::cget("N")] = 4;
    maxCoord[AtomicNumbers::cget("O")] = 2;
    maxCoord[AtomicNumbers::cget("F")] = 1;
    maxCoord[AtomicNumbers::cget("Ne")] = 4;
    maxCoord[AtomicNumbers::cget("Na")] = 1;
    maxCoord[AtomicNumbers::cget("Mg")] = 4;
    maxCoord[AtomicNumbers::cget("Al")] = 4;
    maxCoord[AtomicNumbers::cget("Si")] = 4;
    maxCoord[AtomicNumbers::cget("P")] = 4;
    maxCoord[AtomicNumbers::cget("S")] = 4;
    maxCoord[AtomicNumbers::cget("Cl")] = 1;
    maxCoord[AtomicNumbers::cget("Ar")] = 4;
    maxCoord[AtomicNumbers::cget("K")] = 1;
    maxCoord[AtomicNumbers::cget("Ca")] = 6;
    maxCoord[AtomicNumbers::cget("Sc")] = 4;
    maxCoord[AtomicNumbers::cget("Ti")] = 6;
    maxCoord[AtomicNumbers::cget("V")] = 4;
    maxCoord[AtomicNumbers::cget("Cr")] = 6;
    maxCoord[AtomicNumbers::cget("Mn")] = 6;
    maxCoord[AtomicNumbers::cget("Fe")] = 6;
    maxCoord[AtomicNumbers::cget("Co")] = 6;
    maxCoord[AtomicNumbers::cget("Ni")] = 4;
    maxCoord[AtomicNumbers::cget("Cu")] = 4;
    maxCoord[AtomicNumbers::cget("Zn")] = 4;
    maxCoord[AtomicNumbers::cget("Ga")] = 4;
    maxCoord[AtomicNumbers::cget("Ge")] = 4;
    maxCoord[AtomicNumbers::cget("As")] = 4;
    maxCoord[AtomicNumbers::cget("Se")] = 4;
    maxCoord[AtomicNumbers::cget("Br")] = 1;
    maxCoord[AtomicNumbers::cget("Kr")] = 4;
    maxCoord[AtomicNumbers::cget("Rb")] = 1;
    maxCoord[AtomicNumbers::cget("Sr")] = 6;
    maxCoord[AtomicNumbers::cget("Y")] = 4;
    maxCoord[AtomicNumbers::cget("Zr")] = 4;
    maxCoord[AtomicNumbers::cget("Nb")] = 4;
    maxCoord[AtomicNumbers::cget("Mo")] = 6;
    maxCoord[AtomicNumbers::cget("Tc")] = 6;
    maxCoord[AtomicNumbers::cget("Ru")] = 6;
    maxCoord[AtomicNumbers::cget("Rh")] = 6;
    maxCoord[AtomicNumbers::cget("Pd")] = 4;
    maxCoord[AtomicNumbers::cget("Ag")] = 2;
    maxCoord[AtomicNumbers::cget("Cd")] = 4;
    maxCoord[AtomicNumbers::cget("In")] = 4;
    maxCoord[AtomicNumbers::cget("Sn")] = 4;
    maxCoord[AtomicNumbers::cget("Sb")] = 4;
    maxCoord[AtomicNumbers::cget("Te")] = 4;
    maxCoord[AtomicNumbers::cget("I")] = 1;
    maxCoord[AtomicNumbers::cget("Xe")] = 4;
    maxCoord[AtomicNumbers::cget("Cs")] = 1;
    maxCoord[AtomicNumbers::cget("Ba")] = 6;
    maxCoord[AtomicNumbers::cget("Hf")] = 4;
    maxCoord[AtomicNumbers::cget("Ta")] = 4;
    maxCoord[AtomicNumbers::cget("W")] = 6;
    maxCoord[AtomicNumbers::cget("Re")] = 6;
    maxCoord[AtomicNumbers::cget("Os")] = 6;
    maxCoord[AtomicNumbers::cget("Ir")] = 6;
    maxCoord[AtomicNumbers::cget("Pt")] = 4;
    maxCoord[AtomicNumbers::cget("Au")] = 4;
    maxCoord[AtomicNumbers::cget("Hg")] = 2;
    maxCoord[AtomicNumbers::cget("Tl")] = 1;
    maxCoord[AtomicNumbers::cget("Pb")] = 4;
    maxCoord[AtomicNumbers::cget("Bi")] = 3;
    maxCoord[AtomicNumbers::cget("Po")] = 4;
    maxCoord[AtomicNumbers::cget("At")] = 1;
    maxCoord[AtomicNumbers::cget("Rn")] = 4;
    maxCoord[AtomicNumbers::cget("Fr")] = 1;
    maxCoord[AtomicNumbers::cget("Ra")] = 6;
    maxCoord[AtomicNumbers::cget("La")] = 4;
    maxCoord[AtomicNumbers::cget("Ce")] = 6;
    maxCoord[AtomicNumbers::cget("Pr")] = 6;
    maxCoord[AtomicNumbers::cget("Nd")] = 6;
    maxCoord[AtomicNumbers::cget("Pm")] = 6;
    maxCoord[AtomicNumbers::cget("Sm")] = 6;
    maxCoord[AtomicNumbers::cget("Eu")] = 6;
    maxCoord[AtomicNumbers::cget("Gd")] = 6;
    maxCoord[AtomicNumbers::cget("Tb")] = 6;
    maxCoord[AtomicNumbers::cget("Dy")] = 6;
    maxCoord[AtomicNumbers::cget("Ho")] = 6;
    maxCoord[AtomicNumbers::cget("Er")] = 6;
    maxCoord[AtomicNumbers::cget("Tm")] = 6;
    maxCoord[AtomicNumbers::cget("Yb")] = 6;
    maxCoord[AtomicNumbers::cget("Lu")] = 6;
    maxCoord[AtomicNumbers::cget("Ac")] = 6;
    maxCoord[AtomicNumbers::cget("Th")] = 6;
    maxCoord[AtomicNumbers::cget("Pa")] = 6;
    maxCoord[AtomicNumbers::cget("U")] = 6;
    maxCoord[AtomicNumbers::cget("Np")] = 6;
    maxCoord[AtomicNumbers::cget("Pu")] = 6;
    maxCoord[AtomicNumbers::cget("Am")] = 6;
    maxCoord[AtomicNumbers::cget("Cm")] = 6;
    maxCoord[AtomicNumbers::cget("Bk")] = 6;
    maxCoord[AtomicNumbers::cget("Cf")] = 6;
    maxCoord[AtomicNumbers::cget("Es")] = 6;
    maxCoord[AtomicNumbers::cget("Fm")] = 6;
    maxCoord[AtomicNumbers::cget("Md")] = 6;
    maxCoord[AtomicNumbers::cget("No")] = 6;
    maxCoord[AtomicNumbers::cget("Lr")] = 6;
    return maxCoord;
}

constexpr auto _maximumCoordination = getMaxiumCoordination();
};  // namespace

namespace CovalentRadii {
constexpr std::optional<FloatType> get(uint8_t element)
{
    return (element == 0 || element >= _covalentRadii.size() || std::isnan(_covalentRadii[element]))
               ? std::nullopt
               : std::optional<FloatType>{_covalentRadii[element]};
}
constexpr std::optional<FloatType> getCleaveDistance(uint8_t elementA, uint8_t elementB)
{
    const std::optional<FloatType> radiusA = get(elementA);
    const std::optional<FloatType> radiusB = get(elementB);
    if(!radiusA || !radiusB) {
        return std::nullopt;
    }
    if((elementA == AtomicNumbers::cget("Al") && elementB == AtomicNumbers::cget("Al")) ||
       (elementA == AtomicNumbers::cget("B") && elementB == AtomicNumbers::cget("B"))) {
        return *radiusA + *radiusB - 0.2;
    }
    return *radiusA + *radiusB;
}
};  // namespace CovalentRadii

namespace MaximumCoordination {
constexpr uint8_t max() noexcept { return *std::ranges::max_element(_maximumCoordination); }
constexpr std::optional<uint8_t> get(uint8_t element, std::optional<std::pair<uint8_t, uint8_t>> neighs = std::nullopt)
{
    if(neighs && element == AtomicNumbers::cget("H")) {
        if(neighs->first == AtomicNumbers::cget("B") && neighs->second == AtomicNumbers::cget("B")) {
            return 2;
        }
    }
    if(element >= _covalentRadii.size()) {
        return std::nullopt;
    }
    const uint8_t maxCoordination = _maximumCoordination[element];
    return maxCoordination == 0 ? std::nullopt : std::optional<uint8_t>{maxCoordination};
}
};  // namespace MaximumCoordination

namespace MaximumValence {
// Adapted from supplementary material of doi.org/10.1002/jcc.24309
// https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1002%2Fjcc.24309&file=jcc24309-sup-0001-suppinfo.pdf
// Automatic Molecular Structure Perception for the Universal Force Field
// Svetlana Artemova, Leonard Jaillet, and Stephane Redon
// Journal of Computational Chemistry 2016, 37, 1191–1205
constexpr std::optional<uint8_t> get(uint8_t element, uint8_t coordination)
{
    if(element == AtomicNumbers::cget("H")) {
        if(coordination == 2)
            return 2;
        else
            return 1;
    }
    if(element == AtomicNumbers::cget("P")) {
        if(coordination < 3) {
            return 4;
        }
        else {
            return 5;
        }
    }
    if(element == AtomicNumbers::cget("S")) {
        if(coordination == 2) {
            return 4;
        }
        else {
            return 6;
        }
    }
    if(element == AtomicNumbers::cget("C")) {
        return 4;
    }
    if(element == AtomicNumbers::cget("N")) {
        return 4;
    }
    if(element == AtomicNumbers::cget("O")) {
        return 3;
    }
    if(element == AtomicNumbers::cget("Si")) {
        return 4;
    }
    return MaximumCoordination::get(element);
}
};  // namespace MaximumValence

namespace AtomicPenaltyScore {

enum class FunctionalGroup : uint8_t
{
    NONE,
    AMIDE,
    AROMATIC,
    CARBOXYLATE,    // R-COO
    CARBOXYLATE_C,  // Central atom in COO group
    NITRO,          // R-NOO
    NITRO_C,        // Central atom in NOO group
};

template<typename OrderType>
uint32_t computeAtomicPenaltyScore(uint8_t atomicNumber, size_t coordination, OrderType valence, FunctionalGroup group)
{
    uint8_t val;
    if constexpr(std::is_floating_point_v<OrderType>) {
        OVITO_ASSERT(valence >= 0 && valence <= 255);
        val = std::round(valence);
    }
    else {
        OVITO_ASSERT(valence >= 0 && valence <= 255);
        val = valence;
    }

    if(atomicNumber == AtomicNumbers::cget("C")) {
        if(group == FunctionalGroup::CARBOXYLATE) {
            if(valence == 3) {
                return 64;
            }
            if(valence == 4) {
                return 0;
            }
        }
        else {
            if(valence == 1) {
                return 128;
            }
            if(valence == 2) {
                return 64;
            }
            if(valence == 3) {
                return 32;
            }
            if(valence == 4) {
                return 0;
            }
        }
    }
    else if(atomicNumber == AtomicNumbers::cget("Si")) {
        if(valence == 1) {
            return 8;
        }
        if(valence == 2) {
            return 4;
        }
        if(valence == 3) {
            return 2;
        }
        if(valence == 4) {
            return 0;
        }
    }
    else if(atomicNumber == AtomicNumbers::cget("N")) {
        if(group == FunctionalGroup::NITRO) {
            if(valence == 3) {
                return 64;
            }
            if(valence == 4) {
                return 0;
            }
        }
        else if(coordination == 1) {
            if(valence == 1) {
                return 64;
            }
            if(valence == 2) {
                return 2;
            }
            if(valence == 3) {
                return 0;
            }
        }
        else if(coordination == 2) {
            if(valence == 2) {
                return 4;
            }
            if(valence == 3) {
                return 0;
            }
            if(valence == 4) {
                return 2;
            }
        }
        else if(coordination == 3) {
            if(valence == 3) {
                return 0;
            }
            if(valence == 4) {
                return 1;
            }
        }
        else if(coordination == 4) {
            if(valence == 4) {
                return 0;
            }
        }
    }
    else if(atomicNumber == AtomicNumbers::cget("O")) {
        if(coordination == 1) {
            if(valence == 1) {
                return 2;
            }
            if(valence == 2) {
                return 0;
            }
            if(valence == 3) {
                return 16;
            }
        }
        else if(coordination == 2) {
            if(valence == 2) {
                return 0;
            }
            if(valence == 3) {
                return 16;
            }
        }
        else if(coordination == 3) {
            if(valence == 3) {
                return 0;
            }
        }
    }
    else if(atomicNumber == AtomicNumbers::cget("P")) {
        if(coordination == 1) {
            if(valence == 1) {
                return 64;
            }
            if(valence == 2) {
                return 2;
            }
            if(valence == 3) {
                return 0;
            }
        }
        else if(coordination == 2) {
            if(valence == 2) {
                return 4;
            }
            if(valence == 3) {
                return 0;
            }
            if(valence == 4) {
                return 2;
            }
        }
        else if(coordination == 3) {
            if(valence == 3) {
                return 0;
            }
            if(valence == 4) {
                return 1;
            }
            if(valence == 5) {
                return 2;
            }
        }
        else if(coordination == 4) {
            if(valence == 4) {
                return 1;
            }
            if(valence == 5) {
                return 0;
            }
        }
    }
    else if(atomicNumber == AtomicNumbers::cget("S")) {
        if(coordination == 1) {
            if(valence == 1) {
                return 2;
            }
            if(valence == 2) {
                return 0;
            }
            if(valence == 3) {
                return 64;
            }
        }
        else if(coordination == 2) {
            if(valence == 2) {
                return 0;
            }
            if(valence == 3) {
                return 32;
            }
            if(valence == 4) {
                return 1;
            }
        }
        else if(coordination == 3) {
            if(valence == 3) {
                return 1;
            }
            if(valence == 4) {
                return 0;
            }
            if(valence == 5) {
                return 2;
            }
            if(valence == 6) {
                return 2;
            }
        }
        else if(coordination == 4) {
            if(valence == 4) {
                return 4;
            }

            if(valence == 5) {
                return 2;
            }

            if(valence == 6) {
                return 0;
            }
        }
    }
    return 0;
}
}  // namespace AtomicPenaltyScore

};  // namespace Ovito