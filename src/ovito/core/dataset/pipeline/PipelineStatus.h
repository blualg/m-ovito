////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 OVITO GmbH, Germany
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
 * \brief Stores status information associated with an evaluation of the modification pipeline.
 */
class OVITO_CORE_EXPORT PipelineStatus
{
    Q_GADGET

#ifdef OVITO_QML_GUI
    Q_PROPERTY(QString text READ text)
    Q_PROPERTY(QString shortInfo READ shortInfo)
    Q_PROPERTY(int type READ type)
#endif

public:

    enum StatusType {
        Success,        //< Indicates that the evaluation was successfull.
        Warning,        //< Indicates that a modifier has issued a warning.
        Error           //< Indicates that the evaluation failed.
    };
    Q_ENUM(StatusType);

    /// Default constructor that creates a status object with status StatusType::Success and an empty info text.
    PipelineStatus() = default;

    /// Constructs a status object with the given status and optional text string describing the status.
    PipelineStatus(StatusType t, const QString& text = {}) : _type(t), _text(text) {}

    /// Constructs a status object with success status and a text string describing the status.
    PipelineStatus(const QString& text) : _text(text) {}

    /// Constructs a status object with success status, a text string describing the status, and a short info value.
    template<typename T>
    PipelineStatus(const QString& text, T&& shortInfo) : _text(text), _shortInfo(std::forward<T>(shortInfo)) {}

    /// Constructs a status object with error status and a text string taken from the given exception object.
    PipelineStatus(const Exception& exception, QChar messageSeparator = QChar('\n')) : _type(Error), _text(exception.messages().join(messageSeparator)) {}

    /// Returns the type of status stores in this object.
    StatusType type() const { return _type; }

    /// Changes the type of the status.
    void setType(StatusType type) { _type = type; }

    /// Returns a text string describing the status.
    const QString& text() const { return _text; }

    /// Changes the text string describing the status.
    void setText(const QString& text) { _text = text; }

    /// Returns the information to be displayed next to the pipeline item in the GUI.
    const QVariant& shortInfo() const { return _shortInfo; }

    /// Sets the information displayed next to the pipeline item in the GUI.
    template<typename T>
    void setShortInfo(T&& info) { _shortInfo.setValue(std::forward<T>(info)); }

    /// Tests two status objects for equality.
    bool operator==(const PipelineStatus& other) const {
        return (_type == other._type) && (_text == other._text) && (_shortInfo == other._shortInfo);
    }

    /// Tests two status objects for inequality.
    bool operator!=(const PipelineStatus& other) const { return !(*this == other); }

private:

    /// The status.
    StatusType _type = Success;

    /// A human-readable string describing the status.
    QString _text;

    /// Some information to be displayed in the GUI next to a pipeline item.
    QVariant _shortInfo;

    friend OVITO_CORE_EXPORT SaveStream& operator<<(SaveStream& stream, const PipelineStatus& s);
    friend OVITO_CORE_EXPORT LoadStream& operator>>(LoadStream& stream, PipelineStatus& s);
};

/// \brief Writes a status object to a file stream.
/// \param stream The output stream.
/// \param s The status to write to the output stream \a stream.
/// \return The output stream \a stream.
/// \relates PipelineStatus
OVITO_CORE_EXPORT extern SaveStream& operator<<(SaveStream& stream, const PipelineStatus& s);

/// \brief Reads a status object from a binary input stream.
/// \param stream The input stream.
/// \param s Reference to a variable where the parsed data will be stored.
/// \return The input stream \a stream.
/// \relates PipelineStatus
OVITO_CORE_EXPORT extern LoadStream& operator>>(LoadStream& stream, PipelineStatus& s);

/// \brief Writes a status object to the log stream.
/// \relates PipelineStatus
OVITO_CORE_EXPORT extern QDebug operator<<(QDebug debug, const PipelineStatus& s);

}   // End of namespace
