////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2021 OVITO GmbH, Germany
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

#include <ovito/core/Core.h>
#include "ColorCodingGradient.h"

namespace Ovito {

IMPLEMENT_OVITO_CLASS(ColorCodingGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingHSVGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingGrayscaleGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingHotGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingJetGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingBlueWhiteRedGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingViridisGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingMagmaGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingTableGradient);
IMPLEMENT_OVITO_CLASS(ColorCodingImageGradient);

DEFINE_PROPERTY_FIELD(ColorCodingImageGradient, image);
DEFINE_PROPERTY_FIELD(ColorCodingTableGradient, table);

/******************************************************************************
* Converts a scalar value to a color value.
******************************************************************************/
Color ColorCodingTableGradient::valueToColor(FloatType t)
{
	OVITO_ASSERT(t >= 0.0 && t <= 1.0);
	if(table().empty()) return Color(0,0,0);
	if(table().size() == 1) return table()[0];
	t *= (table().size() - 1);
	FloatType t0 = std::floor(t);
	const Color& c1 = table()[(size_t)t0];
	const Color& c2 = table()[(size_t)std::ceil(t)];
	return c1 * (FloatType(1) - (t - t0)) + c2 * (t - t0);
}

/******************************************************************************
* Loads the given image file from disk.
******************************************************************************/
void ColorCodingImageGradient::loadImage(const QString& filename)
{
	QImage image(filename);
	if(image.isNull())
		throw Exception(tr("Could not load image file '%1'.").arg(filename));
	setImage(image);
}

/******************************************************************************
* Converts a scalar value to a color value.
******************************************************************************/
Color ColorCodingImageGradient::valueToColor(FloatType t)
{
	OVITO_ASSERT(t >= 0.0 && t <= 1.0);
	if(image().isNull()) 
		return Color(0,0,0);
	QPoint p;
	if(image().width() > image().height())
		p = QPoint(std::min((int)(t * image().width()), image().width()-1), 0);
	else
		p = QPoint(0, std::min((int)(t * image().height()), image().height()-1));
	return Color(image().pixel(p));
}

}	// End of namespace
