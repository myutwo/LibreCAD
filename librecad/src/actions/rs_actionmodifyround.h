/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software 
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
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

#ifndef RS_ACTIONMODIFYROUND_H
#define RS_ACTIONMODIFYROUND_H

#include "rs_previewactioninterface.h"
#include "rs_modification.h"


/**
 * This action class can handle user events to round corners.
 *
 * @author Andrew Mustun
 */
class RS_ActionModifyRound : public RS_PreviewActionInterface {
	Q_OBJECT
public:
 /**
  * Action States.
  */
 enum Status {
  SetEntity1,   /**< Choosing the 1st entity. */
  SetEntity2,   /**< Choosing the 2nd entity. */
		SetRadius,   /**< Setting radius in command line. */
		SetTrim    /**< Setting trim flag in command line. */
 };

public:
 RS_ActionModifyRound(RS_EntityContainer& container,
	  RS_GraphicView& graphicView);
 ~RS_ActionModifyRound() = default;

	static QAction* createGUIAction(RS2::ActionType /*type*/, QObject* /*parent*/);
	
	virtual RS2::ActionType rtti() const{
		return RS2::ActionModifyRound;
	}

 virtual void init(int status=0);
 virtual void trigger();

 virtual void mouseMoveEvent(QMouseEvent* e);
 virtual void mouseReleaseEvent(QMouseEvent* e);
	
 virtual void commandEvent(RS_CommandEvent* e);
  virtual QStringList getAvailableCommands();
	
 virtual void hideOptions();
 virtual void showOptions();
	
 virtual void updateMouseButtonHints();
 virtual void updateMouseCursor();
// virtual void updateToolBar();
	
	void setRadius(double r) {
		data.radius = r;
	}

	double getRadius() {
		return data.radius;
	}

	void setTrim(bool t) {
		data.trim = t;
	}

	bool isTrimOn() {
		return data.trim;
	}

private:
	//RS_Vector coord;
	RS_Entity* entity1;
	RS_Entity* entity2;
	RS_Vector coord1;
	RS_Vector coord2;
	RS_RoundData data;
	/** Last status before entering angle. */
	Status lastStatus;
	const std::vector<RS2::EntityType> eType={ RS2::EntityLine , RS2::EntityPolyline , RS2::EntityArc ,
											   RS2::EntityCircle , RS2::EntityEllipse , RS2::EntitySpline};
};

#endif
