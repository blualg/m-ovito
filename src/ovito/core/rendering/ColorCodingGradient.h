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
    virtual ColorT<double> valueToColor(double t) = 0;

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) = 0;
};

/**
 * \brief Converts a scalar value to a color using the HSV color system.
 */
class OVITO_CORE_EXPORT ColorCodingHSVGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingHSVGradient)
    OVITO_CLASSINFO("DisplayName", "Rainbow");

public:

    /// Constructor.
    explicit ColorCodingHSVGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        return ColorT<double>::fromHSV((1.0 - t) * 0.7, 1.0, 1.0);
    }

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        OVITO_ASSERT(t >= 0.0f && t <= 1.0f);
        return ColorT<float>::fromHSV((1.0f - t) * 0.7f, 1.0f, 1.0f);
    }
};

/**
 * \brief Converts a scalar value to a color using a gray-scale ramp.
 */
class OVITO_CORE_EXPORT ColorCodingGrayscaleGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingGrayscaleGradient)
    OVITO_CLASSINFO("DisplayName", "Grayscale");

public:

    /// Constructor.
    explicit ColorCodingGrayscaleGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        return ColorT<double>(t, t, t);
    }

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        OVITO_ASSERT(t >= 0.0f && t <= 1.0f);
        return ColorT<float>(t, t, t);
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingHotGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingHotGradient)
    OVITO_CLASSINFO("DisplayName", "Hot");

public:

    /// Constructor.
    explicit ColorCodingHotGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        // Interpolation black->red->yellow->white.
        return ColorT<double>(std::min(t / 0.375, 1.0), std::max(0.0, std::min((t-0.375)/0.375, 1.0)), std::max(0.0, t*4.0 - 3.0));
    }

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        OVITO_ASSERT(t >= 0.0f && t <= 1.0f);
        // Interpolation black->red->yellow->white.
        return ColorT<float>(std::min(t / 0.375f, 1.0f), std::max(0.0f, std::min((t-0.375f)/0.375f, 1.0f)), std::max(0.0f, t*4.0f - 3.0f));
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingJetGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingJetGradient)
    OVITO_CLASSINFO("DisplayName", "Jet");

public:

    /// Constructor.
    explicit ColorCodingJetGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        if(t < 0.125) return ColorT<double>(0.0, 0.0, 0.5 + 0.5 * t / 0.125);
        else if(t < 0.125 + 0.25) return ColorT<double>(0.0, (t - 0.125) / 0.25, 1.0);
        else if(t < 0.125 + 0.25 + 0.25) return ColorT<double>((t - 0.375) / 0.25, 1.0, 1.0 - (t - 0.375) / 0.25);
        else if(t < 0.125 + 0.25 + 0.25 + 0.25) return ColorT<double>(1.0, 1.0 - (t - 0.625) / 0.25, 0.0);
        else return ColorT<double>(1.0 - 0.5 * (t - 0.875) / 0.125, 0.0, 0.0);
    }

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        OVITO_ASSERT(t >= 0.0f && t <= 1.0f);
        if(t < 0.125f) return ColorT<float>(0.0f, 0.0f, 0.5f + 0.5f * t / 0.125f);
        else if(t < 0.125f + 0.25f) return ColorT<float>(0.0f, (t - 0.125f) / 0.25f, 1.0f);
        else if(t < 0.125f + 0.25f + 0.25f) return ColorT<float>((t - 0.375f) / 0.25f, 1.0f, 1.0f - (t - 0.375f) / 0.25f);
        else if(t < 0.125f + 0.25f + 0.25f + 0.25f) return ColorT<float>(1.0f, 1.0f - (t - 0.625f) / 0.25f, 0.0f);
        else return ColorT<float>(1.0f - 0.5f * (t - 0.875f) / 0.125f, 0.0f, 0.0f);
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingBlueWhiteRedGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingBlueWhiteRedGradient)
    OVITO_CLASSINFO("DisplayName", "Blue-White-Red");

public:

    /// Constructor.
    explicit ColorCodingBlueWhiteRedGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override {
        OVITO_ASSERT(t >= 0.0 && t <= 1.0);
        if(t <= 0.5)
            return ColorT<double>(t * 2.0, t * 2.0, 1.0);
        else
            return ColorT<double>(1.0, (1.0-t) * 2.0, (1.0-t) * 2.0);
    }

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        OVITO_ASSERT(t >= 0.0f && t <= 1.0f);
        if(t <= 0.5f)
            return ColorT<float>(t * 2.0f, t * 2.0f, 1.0f);
        else
            return ColorT<float>(1.0f, (1.0f-t) * 2.0f, (1.0f-t) * 2.0f);
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingViridisGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingViridisGradient)
    OVITO_CLASSINFO("DisplayName", "Viridis");

public:

    /// Constructor.
    explicit ColorCodingViridisGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override {
        return ColorCodingViridisGradient::valueToColor(static_cast<float>(t)).toDataType<double>();
    }

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        OVITO_ASSERT(t >= 0.0f && t <= 1.0f);
        int index = t * (sizeof(colormap_viridis_data) / sizeof(colormap_viridis_data[0]) - 1);
        OVITO_ASSERT(index >= 0 && index < sizeof(colormap_viridis_data) / sizeof(colormap_viridis_data[0]));
        return ColorT<float>(colormap_viridis_data[index][0], colormap_viridis_data[index][1], colormap_viridis_data[index][2]);
    }
};

/**
 * \brief Converts a scalar value to a color.
 */
class OVITO_CORE_EXPORT ColorCodingMagmaGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingMagmaGradient)
    OVITO_CLASSINFO("DisplayName", "Magma");

public:

    /// Constructor.
    explicit ColorCodingMagmaGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override {
        return ColorCodingMagmaGradient::valueToColor(static_cast<float>(t)).toDataType<double>();
    }

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        OVITO_ASSERT(t >= 0.0f && t <= 1.0f);
        int index = t * (sizeof(colormap_magma_data) / sizeof(colormap_magma_data[0]) - 1);
        OVITO_ASSERT(index >= 0 && index < sizeof(colormap_magma_data) / sizeof(colormap_magma_data[0]));
        return ColorT<float>(colormap_magma_data[index][0], colormap_magma_data[index][1], colormap_magma_data[index][2]);
    }
};

/**
 * \brief Uses a color table to convert scalar values to a color.
 */
class OVITO_CORE_EXPORT ColorCodingTableGradient : public ColorCodingGradient
{
    OVITO_CLASS(ColorCodingTableGradient)
    OVITO_CLASSINFO("DisplayName", "User table");

public:

    /// Constructor.
    explicit ColorCodingTableGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override;

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        return ColorCodingTableGradient::valueToColor(static_cast<double>(t)).toDataType<float>();
    }

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
    OVITO_CLASSINFO("DisplayName", "User image");

public:

    /// Constructor.
    explicit ColorCodingImageGradient(ObjectInitializationFlags flags) : ColorCodingGradient(flags) {}

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<double> valueToColor(double t) override;

    /// \brief Converts a scalar value to a color value.
    /// \param t A value between 0 and 1.
    /// \return The color that visualizes the given scalar value.
    virtual ColorT<float> valueToColor(float t) override {
        return ColorCodingImageGradient::valueToColor(static_cast<double>(t)).toDataType<float>();
    }

    /// Loads the given image file from disk.
    void loadImage(const QString& filename);

private:

    /// The user-defined color map image.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QImage, image, setImage);

    /// The original filesystem path to the user-defined color map image.
    DECLARE_MODIFIABLE_PROPERTY_FIELD(QString, imagePath, setImagePath);
};

}   // End of namespace
