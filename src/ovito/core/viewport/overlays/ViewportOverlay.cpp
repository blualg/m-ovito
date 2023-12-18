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

#include <ovito/core/Core.h>
#include <ovito/core/viewport/overlays/ViewportOverlay.h>

namespace Ovito {

IMPLEMENT_ABSTRACT_OVITO_CLASS(ViewportOverlay);

/******************************************************************************
* Constructor.
******************************************************************************/
ViewportOverlay::ViewportOverlay(ObjectInitializationFlags flags) : ActiveObject(flags)
{
}

/******************************************************************************
* Helper method that checks whether the given Qt alignment value contains exactly one horizontal and one vertical alignment flag.
******************************************************************************/
void ViewportOverlay::checkAlignmentParameterValue(int alignment) const
{
    int horizontalAlignment = alignment & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter);
    int verticalAlignment = alignment & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter);

    if(horizontalAlignment == 0)
        throw Exception(tr("No horizontal alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));

    if(horizontalAlignment != Qt::AlignLeft && horizontalAlignment != Qt::AlignRight && horizontalAlignment != Qt::AlignHCenter)
        throw Exception(tr("More than one horizontal alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));

    if(verticalAlignment == 0)
        throw Exception(tr("No vertical alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));

    if(verticalAlignment != Qt::AlignTop && verticalAlignment != Qt::AlignBottom && verticalAlignment != Qt::AlignVCenter)
        throw Exception(tr("More than one vertical alignment flag was specified for the %1. Please check the value you provided for the alignment parameter. It must be a combination of exactly one horizontal and one vertical alignment flag.")
            .arg(getOOMetaClass().name()));
}

}   // End of namespace
