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

#include <ovito/core/Core.h>
#include "TextPrimitive.h"
#include "SceneRenderer.h"

#include <QTextDocument>
#include <QTextFrame> 
#include <QTextFrameFormat> 
#include <QAbstractTextDocumentLayout> 

namespace Ovito {

/******************************************************************************
* Sets the destination rectangle for rendering the image in viewport coordinates.
******************************************************************************/
void TextPrimitive::setPositionViewport(const SceneRenderer* renderer, const Point2& pos)
{ 
	QSize windowSize = renderer->viewportRect().size();
	Point2 pwin((pos.x() + 1.0) * windowSize.width() / 2.0, (-pos.y() + 1.0) * windowSize.height() / 2.0);
	setPositionWindow(pwin);
}

/******************************************************************************
* Computes the bounding rectangle of the text to be rendered.
******************************************************************************/
QRectF TextPrimitive::queryBounds(const SceneRenderer* renderer, Qt::TextFormat textFormatHint) const
{
	QRectF textBounds;
	Qt::TextFormat resolvedTextFormat = textFormat();
	if(resolvedTextFormat == Qt::AutoText) {
		if(textFormatHint != Qt::AutoText) resolvedTextFormat = textFormatHint;
		else resolvedTextFormat = Qt::mightBeRichText(text()) ? Qt::RichText : Qt::PlainText;
	}
	if(resolvedTextFormat != Qt::RichText) {
		if(!useTightBox()) {
			textBounds = QFontMetricsF(font()).boundingRect(text());
		}
		else {
			QPainterPath textPath;
			textPath.addText(0, 0, font(), text());
			textBounds = textPath.boundingRect();
		}
	}
	else {
		QTextDocument doc;
		doc.setUndoRedoEnabled(false);
		doc.setHtml(text());
		doc.setDefaultFont(font());
		doc.setDocumentMargin(0);
		QTextOption opt = doc.defaultTextOption();
		opt.setAlignment(Qt::Alignment(alignment()));
		doc.setDefaultTextOption(opt);
		textBounds = QRectF(QPointF(0,0), doc.size());
	}
	qreal devicePixelRatio = renderer->devicePixelRatio();
	return QRectF(textBounds.left() * devicePixelRatio, textBounds.top() * devicePixelRatio, textBounds.width() * devicePixelRatio, textBounds.height() * devicePixelRatio);
}

}	// End of namespace
