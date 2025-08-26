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

#include <ovito/core/Core.h>
#include "AnimationFrameLabel.h"

namespace Ovito {

/******************************************************************************
* Tries to parse a frame label from its string representation.
******************************************************************************/
AnimationFrameLabel AnimationFrameLabel::parse(const QString& text)
{
    if(text.startsWith("Timestep ")) {
        bool ok;
        qlonglong timestep = text.mid(9).toLongLong(&ok);
        if(ok)
            return { LabelType::Timestep, static_cast<FloatType>(timestep) };
    }
    else if(text.startsWith("Index ")) {
        bool ok;
        qlonglong index = text.mid(6).toLongLong(&ok);
        if(ok)
            return { LabelType::Index, static_cast<FloatType>(index) };
    }
    else if(text.startsWith("Time ")) {
        bool ok;
        double time = text.mid(5).toDouble(&ok);
        if(ok)
            return { LabelType::Time, static_cast<FloatType>(time) };
    }
    else if(auto idx = text.indexOf(" (Frame "); idx > 0 && text.endsWith(QChar(')'))) {
        bool ok;
        qlonglong frame = text.mid(idx + 8).toLongLong(&ok);
        if(ok)
            return { LabelType::FilenameAndFrame, static_cast<FloatType>(frame), text.left(idx) };
    }
    else if(!text.isEmpty()) {
        // Note: We cannot disambiguate between filename and string label types.
        return { LabelType::String, FloatType(0), text };
    }
    return {};
}

}   // End of namespace
