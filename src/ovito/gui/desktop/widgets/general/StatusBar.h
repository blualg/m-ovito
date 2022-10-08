////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright 2022 Alexander Stukowski
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
 * \file StatusBar.h
 * \brief Contains the definition of the Ovito::StatusBar class.
 */

#pragma once


#include <ovito/gui/desktop/GUI.h>

namespace Ovito {

/**
 * \brief A status bar widget.
 */
class StatusBar : public QLabel
{
	Q_OBJECT

public:

	/// \brief Constructs a status bar widget.
	/// \param parent The parent widget for the new widget.
	StatusBar(QWidget* parent = nullptr);

	QLabel* overflowWidget() { return _overflowLabel; }

	virtual QSize sizeHint() const override;
	virtual QSize minimumSizeHint() const override { return sizeHint(); }

	QString currentMessage() const { return text(); }

public Q_SLOTS:

	/// Displays the given message for the specified number of milli-seconds
	void showMessage(const QString& message, int timeout = 0);

	/// Removes any message being shown.
	void clearMessage();

protected:

	virtual void resizeEvent(QResizeEvent* event) override;

private:

	QTimer* _timer = nullptr;
	QLabel* _overflowLabel = nullptr;
	mutable int _preferredHeight = 0;
};

}	// End of namespace
