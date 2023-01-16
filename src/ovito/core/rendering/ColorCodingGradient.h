////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2023 OVITO GmbH, Germany
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
#include "ColormapsData.h"

namespace Ovito {

/**
 * \brief Abstract base class for color gradients that can be used as a pseudo-color transfer function.
 *
 * Implementations of this class convert a scalar value in the range [0,1] to a color value.
 */
class OVITO_CORE_EXPORT ColorCodingGradient : public RefTarget
{
    OVITO_CLASS(ColorCodingGradient)

protected:

    /// Constructor.
    using RefTarget::RefTarget;

public:

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    Q_INVOKABLE virtual Color valueToColor(FloatType t) = 0;
};

/**
 * \brief Converts a scalar value to a color using the HSV color system.
 */
class OVITO_CORE_EXPORT ColorCodingHSVGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingHSVGradient)
    Q_CLASSINFO("DisplayName", "Rainbow");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingHSVGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override { 
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        return Color::fromHSV((FloatType(1) - t) * FloatType(0.7), 1, 1); 
    }
};

/**
 * \brief Converts a scalar value to a color using a gray-scale ramp.
 */
class OVITO_CORE_EXPORT ColorCodingGrayscaleGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingGrayscaleGradient)
    Q_CLASSINFO("DisplayName", "Grayscale");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingGrayscaleGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override { 
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        return Color(t, t, t); 
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingHotGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingHotGradient)
    Q_CLASSINFO("DisplayName", "Hot");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingHotGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        // Interpolation black->red->yellow->white.
        return Color(std::min(t / FloatType(0.375), FloatType(1)), std::max(FloatType(0), std::min((t-FloatType(0.375))/FloatType(0.375), FloatType(1))), std::max(FloatType(0), t*4 - FloatType(3)));
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingJetGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingJetGradient)
    Q_CLASSINFO("DisplayName", "Jet");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingJetGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        if(t < FloatType(0.125)) return Color(0, 0, FloatType(0.5) + FloatType(0.5) * t / FloatType(0.125));
        else if(t < FloatType(0.125) + FloatType(0.25)) return Color(0, (t - FloatType(0.125)) / FloatType(0.25), 1);
        else if(t < FloatType(0.125) + FloatType(0.25) + FloatType(0.25)) return Color((t - FloatType(0.375)) / FloatType(0.25), 1, FloatType(1) - (t - FloatType(0.375)) / FloatType(0.25));
        else if(t < FloatType(0.125) + FloatType(0.25) + FloatType(0.25) + FloatType(0.25)) return Color(1, FloatType(1) - (t - FloatType(0.625)) / FloatType(0.25), 0);
        else return Color(FloatType(1) - FloatType(0.5) * (t - FloatType(0.875)) / FloatType(0.125), 0, 0);
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingBlueWhiteRedGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingBlueWhiteRedGradient)
    Q_CLASSINFO("DisplayName", "Blue-White-Red");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingBlueWhiteRedGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        if(t <= FloatType(0.5))
            return Color(t * 2, t * 2, 1);
        else
            return Color(1, (FloatType(1)-t) * 2, (FloatType(1)-t) * 2);
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingViridisGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingViridisGradient)
    Q_CLASSINFO("DisplayName", "Viridis");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingViridisGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        int index = t * (sizeof(colormap_viridis_data)/sizeof(colormap_viridis_data[0]) - 1);
        OVITO_ASSERT(t >= 0 && t < sizeof(colormap_viridis_data)/sizeof(colormap_viridis_data[0]));
        return Color(colormap_viridis_data[index][0], colormap_viridis_data[index][1], colormap_viridis_data[index][2]);
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingMagmaGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingMagmaGradient)
    Q_CLASSINFO("DisplayName", "Magma");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingMagmaGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        int index = t * (sizeof(colormap_magma_data)/sizeof(colormap_magma_data[0]) - 1);
        OVITO_ASSERT(t >= 0 && t < sizeof(colormap_magma_data)/sizeof(colormap_magma_data[0]));
        return Color(colormap_magma_data[index][0], colormap_magma_data[index][1], colormap_magma_data[index][2]);
    }
};

/**
 * \brief Uses a color table to convert scalar values to a color.
 */
class OVITO_CORE_EXPORT ColorCodingTableGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingTableGradient)
    Q_CLASSINFO("DisplayName", "User table");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingTableGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override;

private:

    /// The user-defined color table.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(std::vector<Color>, table, setTable);
};

/**
 * \brief Converts a scalar value to a color based on a user-defined image.
 */
class OVITO_CORE_EXPORT ColorCodingImageGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingImageGradient)
    Q_CLASSINFO("DisplayName", "User image");

public:

    /// Constructor.
    Q_INVOKABLE ColorCodingImageGradient(ObjectCreationParams params) : ColorCodingGradient(params) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual Color valueToColor(FloatType t) override;

    /// Loads the given image file from disk.
    void loadImage(const QString& filename);

private:

    /// The user-defined color map image.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QImage, image, setImage);

    /// The original filesystem path to the user-defined color map image.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, imagePath, setImagePath);
};

}   // End of namespace
