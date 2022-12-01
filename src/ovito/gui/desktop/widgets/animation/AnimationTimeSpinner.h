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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/gui/desktop/widgets/general/SpinnerWidget.h>

namespace Ovito {

/**
 * A spinner control for the current animation time.
 */
class OVITO_GUI_EXPORT AnimationTimeSpinner : public SpinnerWidget
{
	Q_OBJECT

public:

	/// Constructs the spinner control.
	AnimationTimeSpinner(MainWindow& mainWindow, QWidget* parent = 0);

protected Q_SLOTS:

	/// This is called when the current animation time has changed.
	void onCurrentFrameChanged(int newFrame);

	/// This is called when the animation interval has changed.
	void onIntervalChanged(int firstFrame, int lastFrame);

	/// Is called when the spinner value has been changed by the user.
	void onSpinnerValueChanged();

private:

	MainWindow& _mainWindow;
};

}	// End of namespace
