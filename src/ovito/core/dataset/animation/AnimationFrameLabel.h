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

/**
 * \brief A data structure that can store a label for one frame of a loaded simulation trajectory.
 */
struct AnimationFrameLabel
{
    enum LabelType {
        None,
        Timestep,
        Time,
        Index,
        Filename,
        FilenameAndFrame,
        String
    };

    LabelType type = LabelType::None;
    FloatType numericLabel = 0; // A timestep number, simulation time, etc.
    QString stringLabel; // A filename, structure name, custom string, etc.

    /// Sets the label to a simulation timestep value.
    void setToTimestep(qlonglong timestep) {
        type = LabelType::Timestep;
        numericLabel = timestep;
        stringLabel.clear();
    }

    /// Sets the label to a simulation time value.
    void setToTime(FloatType simulationTime) {
        type = LabelType::Time;
        numericLabel = simulationTime;
        stringLabel.clear();
    }

    /// Sets the label to a custom string value.
    void setToString(const QString& string) {
        type = LabelType::String;
        stringLabel = string;
        numericLabel = 0;
    }

    /// Sets the label to a filename.
    void setToFilename(const QString& filename) {
        type = LabelType::Filename;
        stringLabel = filename;
        numericLabel = 0;
    }

    /// Sets the label to a filename and frame number.
    void setToFilenameAndFrame(const QString& filename, int frameNumber) {
        type = LabelType::FilenameAndFrame;
        stringLabel = filename;
        numericLabel = frameNumber;
    }

    /// When the label type is a filename, sets frame number within the file.
    void setFrameOfFile(int frameNumber) {
        OVITO_ASSERT(type == LabelType::Filename || type == LabelType::FilenameAndFrame);
        type = LabelType::FilenameAndFrame;
        numericLabel = frameNumber;
    }

    /// Returns a human-readable representation of the label, which can be displayed in the UI.
    QString toDisplayString() const {
        switch(type) {
        case LabelType::Timestep:
            return QStringLiteral("Timestep %1").arg((qlonglong)numericLabel);
        case LabelType::Time:
            return QStringLiteral("Time %1").arg(numericLabel);
        case LabelType::Index:
            return QStringLiteral("Index %1").arg((qlonglong)numericLabel);
        case LabelType::Filename:
        case LabelType::String:
            return stringLabel;
        case LabelType::FilenameAndFrame:
            return QStringLiteral("%1 (Frame %2)").arg(stringLabel).arg((int)numericLabel);
        default:
            return {};
        }
    }

    /// Tries to parse a frame label from its string representation.
    static AnimationFrameLabel parse(const QString& text);

    /// Writes a label to a binary output stream.
    friend inline QDataStream& operator<<(QDataStream& stream, const AnimationFrameLabel& label) {
        return stream << label.type << label.numericLabel << label.stringLabel;
    }

    /// Reads a label from a binary input stream.
    friend inline QDataStream& operator>>(QDataStream& stream, AnimationFrameLabel& label) {
        stream >> label.type >> label.numericLabel >> label.stringLabel;
        return stream;
    }

    // Comparison operator.
    bool operator==(const AnimationFrameLabel& other) const {
        return (type == other.type) && (numericLabel == other.numericLabel) && (stringLabel == other.stringLabel);
    }
};

}   // End of namespace
