/****************************************************************************
**
** This file is part of the CADuntu project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (caduntu@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by 
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
** This copyright notice MUST APPEAR in all copies of the script!  
**
**********************************************************************/

#include "rs_actiondimaligned.h"

#include "rs_snapper.h"
#include "rs_constructionline.h"
#include "rs_dialogfactory.h"



RS_ActionDimAligned::RS_ActionDimAligned(RS_EntityContainer& container,
        RS_GraphicView& graphicView)
        :RS_ActionDimension("Draw aligned dimensions",
                    container, graphicView) {

    reset();
}



RS_ActionDimAligned::~RS_ActionDimAligned() {}

QAction* RS_ActionDimAligned::createGUIAction(RS2::ActionType /*type*/, QObject* /*parent*/) {
// RVT_PORT	QAction* action = new QAction(tr("Aligned"), tr("&Aligned"),
//								  QKeySequence(), NULL);
	QAction* action = new QAction(tr("Aligned"),  NULL);
        action->setStatusTip(tr("Aligned Dimension"));

		return action;
}


void RS_ActionDimAligned::reset() {
    RS_ActionDimension::reset();

    edata = RS_DimAlignedData(RS_Vector(false),
                              RS_Vector(false));
    lastStatus = SetExtPoint1;
    if (RS_DIALOGFACTORY!=NULL) {
        RS_DIALOGFACTORY->requestOptions(this, true, true);
    }
}



void RS_ActionDimAligned::trigger() {
    RS_ActionDimension::trigger();

    preparePreview();
    graphicView->moveRelativeZero(data.definitionPoint);

	//data.text = getText();
    RS_DimAligned* dim =
        new RS_DimAligned(container, data, edata);
    dim->setLayerToActive();
    dim->setPenToActive();
    dim->update();
    container->addEntity(dim);

    // upd. undo list:
    if (document!=NULL) {
        document->startUndoCycle();
        document->addUndoable(dim);
        document->endUndoCycle();
    }

    RS_Vector rz = graphicView->getRelativeZero();
	graphicView->redraw(RS2::RedrawDrawing);
    graphicView->moveRelativeZero(rz);

    RS_DEBUG->print("RS_ActionDimAligned::trigger():"
                    " dim added: %d", dim->getId());
}



void RS_ActionDimAligned::preparePreview() {
    RS_Vector dirV;
    dirV.setPolar(100.0,
                  edata.extensionPoint1.angleTo(
                      edata.extensionPoint2)
                  +M_PI/2.0);
    RS_ConstructionLine cl(NULL,
                           RS_ConstructionLineData(
                               edata.extensionPoint2,
                               edata.extensionPoint2+dirV));

    data.definitionPoint =
        cl.getNearestPointOnEntity(data.definitionPoint);
}



void RS_ActionDimAligned::mouseMoveEvent(RS_MouseEvent* e) {
    RS_DEBUG->print("RS_ActionDimAligned::mouseMoveEvent begin");

    RS_Vector mouse = snapPoint(e);

    switch (getStatus()) {
    case SetExtPoint1:
        break;

    case SetExtPoint2:
        if (edata.extensionPoint1.valid) {
            deletePreview();
            preview->addEntity(
                new RS_Line(preview,
                            RS_LineData(edata.extensionPoint1, mouse))
            );
            drawPreview();
        }
        break;

    case SetDefPoint:
        if (edata.extensionPoint1.valid && edata.extensionPoint2.valid) {
            deletePreview();
            data.definitionPoint = mouse;

            preparePreview();

			//data.text = getText();
            RS_DimAligned* dim = new RS_DimAligned(preview, data, edata);
            dim->update();
            preview->addEntity(dim);
            drawPreview();
        }
        break;

	default:
		break;
    }

    RS_DEBUG->print("RS_ActionDimAligned::mouseMoveEvent end");
}



void RS_ActionDimAligned::mouseReleaseEvent(RS_MouseEvent* e) {
    if (RS2::qtToRsButtonState(e->button())==RS2::LeftButton) {
        RS_CoordinateEvent ce(snapPoint(e));
        coordinateEvent(&ce);
    } else if (RS2::qtToRsButtonState(e->button())==RS2::RightButton) {
        deletePreview();
        init(getStatus()-1);
    }
}



void RS_ActionDimAligned::coordinateEvent(RS_CoordinateEvent* e) {
    if (e==NULL) {
        return;
    }

    RS_Vector pos = e->getCoordinate();

    switch (getStatus()) {
    case SetExtPoint1:
        edata.extensionPoint1 = pos;
        graphicView->moveRelativeZero(pos);
        setStatus(SetExtPoint2);
        break;

    case SetExtPoint2:
        edata.extensionPoint2 = pos;
        graphicView->moveRelativeZero(pos);
        setStatus(SetDefPoint);
        break;

    case SetDefPoint:
        data.definitionPoint = pos;
        trigger();
        reset();
        setStatus(SetExtPoint1);
        break;

    default:
        break;
    }
}



void RS_ActionDimAligned::commandEvent(RS_CommandEvent* e) {
    RS_String c = e->getCommand().lower();

    if (checkCommand("help", c)) {
        if (RS_DIALOGFACTORY!=NULL) {
            RS_DIALOGFACTORY->commandMessage(msgAvailableCommands()
                                             + getAvailableCommands().join(", "));
        }
        return;
    }

    switch (getStatus()) {
    case SetText: {
            setText(c);
            if (RS_DIALOGFACTORY!=NULL) {
                RS_DIALOGFACTORY->requestOptions(this, true, true);
            }
            setStatus(lastStatus);
            graphicView->enableCoordinateInput();
        }
        break;

    default:
        if (checkCommand("text", c)) {
            lastStatus = (Status)getStatus();
            graphicView->disableCoordinateInput();
            setStatus(SetText);
        }
        break;
    }
}



RS_StringList RS_ActionDimAligned::getAvailableCommands() {
    RS_StringList cmd;

    switch (getStatus()) {
    case SetExtPoint1:
    case SetExtPoint2:
    case SetDefPoint:
        cmd += command("text");
        break;

    default:
        break;
    }

    return cmd;
}



void RS_ActionDimAligned::updateMouseButtonHints() {
    if (RS_DIALOGFACTORY!=NULL) {
        switch (getStatus()) {
        case SetExtPoint1:
            RS_DIALOGFACTORY->updateMouseWidget(
                tr("Specify first extension line origin"),
                tr("Cancel"));
            break;
        case SetExtPoint2:
            RS_DIALOGFACTORY->updateMouseWidget(
                tr("Specify second extension line origin"),
                tr("Back"));
            break;
        case SetDefPoint:
            RS_DIALOGFACTORY->updateMouseWidget(
                tr("Specify dimension line location"),
                tr("Back"));
            break;
        case SetText:
            RS_DIALOGFACTORY->updateMouseWidget(tr("Enter dimension text:"), "");
            break;
        default:
            RS_DIALOGFACTORY->updateMouseWidget("", "");
            break;
        }
    }
}



void RS_ActionDimAligned::hideOptions() {
    if (RS_DIALOGFACTORY!=NULL) {
        RS_DIALOGFACTORY->requestOptions(this, false);
    }

    RS_ActionDimension::hideOptions();
}



void RS_ActionDimAligned::showOptions() {
    RS_ActionDimension::showOptions();

    if (RS_DIALOGFACTORY!=NULL) {
        RS_DIALOGFACTORY->requestOptions(this, true);
    }
}


// EOF