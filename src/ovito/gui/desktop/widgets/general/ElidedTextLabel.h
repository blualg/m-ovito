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

/**
 * \file ElidedTextLabel.h
 * \brief Contains the definition of the Ovito::ElidedTextLabel class.
 */

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A QLabel-like widget that display a line of text, which is shortened if necessary to fit the available space.
 */
class OVITO_GUI_EXPORT ElidedTextLabel : public QLabel
{
	Q_OBJECT

public:

	/// \brief Constructs an empty label.
	/// \param elideMode Controls where the text gets shortened if necessary.
	/// \param parent The parent widget for the new widget.
	/// \param f Flags to be passed to the QLabel constructor.
	ElidedTextLabel(Qt::TextElideMode elideMode, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(parent, f), _elideMode(elideMode) {}

	/// \brief Constructs a label with text.
	/// \param elideMode Controls where the text gets shortened if necessary.
	/// \param text The text string to display.
	/// \param parent The parent widget for the new widget.
	/// \param f Flags to be passed to the QLabel constructor.
	ElidedTextLabel(Qt::TextElideMode elideMode, const QString& string, QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : QLabel(string, parent, f), _elideMode(elideMode) {}

protected:

	/// Returns the area that is available for us to draw the document.
	QRect documentRect() const;

	/// Paints the widget.
	void paintEvent(QPaintEvent *) override;

	/// The elide mode. 
	Qt::TextElideMode _elideMode;
};

}	// End of namespace
