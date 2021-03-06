/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2014 Christian Luginbühl (dinkel@pimprecords.com)
**
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along
** with this program; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
**
**********************************************************************/

#include "lc_makercamsvg.h"

#include "lc_xmlwriterinterface.h"

#include "rs_arc.h"
#include "rs_block.h"
#include "rs_circle.h"
#include "rs_ellipse.h"
#include "rs_hatch.h"
#include "rs_image.h"
#include "rs_insert.h"
#include "rs_layer.h"
#include "rs_leader.h"
#include "rs_line.h"
#include "rs_dimaligned.h"
#include "rs_dimangular.h"
#include "rs_dimdiametric.h"
#include "rs_dimlinear.h"
#include "rs_dimradial.h"
#include "rs_hatch.h"
#include "rs_image.h"
#include "rs_insert.h"
#include "rs_mtext.h"
#include "rs_polyline.h"
#include "rs_point.h"
#include "rs_spline.h"
#include "lc_splinepoints.h"
#include "rs_graphic.h"
#include "rs_system.h"
#include "rs_text.h"
#include "rs_units.h"
#include "rs_utility.h"
#include "rs_math.h"

namespace {
const std::string NAMESPACE_URI_SVG = "http://www.w3.org/2000/svg";
const std::string NAMESPACE_URI_LC = "http://www.librecad.org";
const std::string NAMESPACE_URI_XLINK = "http://www.w3.org/1999/xlink";
}

LC_MakerCamSVG::LC_MakerCamSVG(LC_XMLWriterInterface* xmlWriter,
                               bool writeInvisibleLayers,
                               bool writeConstructionLayers,
							   bool writeBlocksInline,
							   bool convertEllipsesToBeziers):
	writeInvisibleLayers(writeInvisibleLayers)
  ,writeConstructionLayers(writeConstructionLayers)
  ,writeBlocksInline(writeBlocksInline)
  ,convertEllipsesToBeziers(convertEllipsesToBeziers)
  ,xmlWriter(xmlWriter)
  ,offset(0.,0.)
{
    RS_DEBUG->print("RS_MakerCamSVG::RS_MakerCamSVG()");
}

bool LC_MakerCamSVG::generate(RS_Graphic* graphic) {

    write(graphic);

    return true;
}

std::string LC_MakerCamSVG::resultAsString() {

    return xmlWriter->documentAsString();
}

void LC_MakerCamSVG::write(RS_Graphic* graphic) {

    RS_DEBUG->print("RS_MakerCamSVG::write: Writing root node ...");

    graphic->calculateBorders();

    min = graphic->getMin();
    max = graphic->getMax();

    RS2::Unit raw_unit = graphic->getUnit();

    switch (raw_unit) {
        case RS2::Millimeter:
            unit = "mm";
            break;
        case RS2::Centimeter:
            unit = "cm";
            break;
        case RS2::Inch:
            unit = "in";
            break;

        default:
            unit = "";
            break;
    }

    xmlWriter->createRootElement("svg", NAMESPACE_URI_SVG);

    xmlWriter->addNamespaceDeclaration("lc", NAMESPACE_URI_LC);
    xmlWriter->addNamespaceDeclaration("xlink", NAMESPACE_URI_XLINK);

    xmlWriter->addAttribute("width", numXml(max.x - min.x) + unit);
    xmlWriter->addAttribute("height", numXml(max.y - min.y) + unit);
    xmlWriter->addAttribute("viewBox", "0 0 "+ numXml(max.x - min.x) + " " + numXml(max.y - min.y));

    writeBlocks(graphic);
    writeLayers(graphic);
}

void LC_MakerCamSVG::writeBlocks(RS_Document* document) {

    if (!writeBlocksInline) {

        RS_DEBUG->print("RS_MakerCamSVG::writeBlocks: Writing blocks ...");

        RS_BlockList* blocklist = document->getBlockList();

        if (blocklist->count() > 0) {

            xmlWriter->addElement("defs", NAMESPACE_URI_SVG);

            for (int i = 0; i < blocklist->count(); i++) {

                writeBlock(blocklist->at(i));
            }

            xmlWriter->closeElement();
        }

    }
}

void LC_MakerCamSVG::writeBlock(RS_Block* block) {

    RS_DEBUG->print("RS_MakerCamSVG::writeBlock: Writing block with name '%s'",
                    qPrintable(block->getName()));

    xmlWriter->addElement("g", NAMESPACE_URI_SVG);

    xmlWriter->addAttribute("id", std::to_string(block->getId()));
    xmlWriter->addAttribute("blockname", qPrintable(block->getName()), NAMESPACE_URI_LC);

    writeLayers(block);

    xmlWriter->closeElement();
}

void LC_MakerCamSVG::writeLayers(RS_Document* document) {

    RS_DEBUG->print("RS_MakerCamSVG::writeLayers: Writing layers ...");

    RS_LayerList* layerlist = document->getLayerList();

    for (unsigned int i = 0; i < layerlist->count(); i++) {

        writeLayer(document, layerlist->at(i));
    }
}

void LC_MakerCamSVG::writeLayer(RS_Document* document, RS_Layer* layer) {

    if (writeInvisibleLayers || !layer->isFrozen()) {

        if (writeConstructionLayers || !layer->isConstruction()) {

            RS_DEBUG->print("RS_MakerCamSVG::writeLayer: Writing layer with name '%s'",
                            qPrintable(layer->getName()));

            xmlWriter->addElement("g", NAMESPACE_URI_SVG);

            xmlWriter->addAttribute("layername", qPrintable(layer->getName()), NAMESPACE_URI_LC);
            xmlWriter->addAttribute("is_locked", (layer->isLocked() ? "true" : "false"), NAMESPACE_URI_LC);
            xmlWriter->addAttribute("is_construction", (layer->isConstruction() ? "true" : "false"), NAMESPACE_URI_LC);

            if (layer->isFrozen())
            {
                xmlWriter->addAttribute("style", "display: none;");
            }

            xmlWriter->addAttribute("fill", "none");
            xmlWriter->addAttribute("stroke", "black");
            xmlWriter->addAttribute("stroke-width", "1");

            writeEntities(document, layer);

            xmlWriter->closeElement();
        }
        else {

            RS_DEBUG->print("RS_MakerCamSVG::writeLayer: Omitting construction layer with name '%s'",
                            qPrintable(layer->getName()));
        }
    }
    else {

        RS_DEBUG->print("RS_MakerCamSVG::writeLayer: Omitting invisible layer with name '%s'",
                        qPrintable(layer->getName()));
    }
}

void LC_MakerCamSVG::writeEntities(RS_Document* document, RS_Layer* layer) {

    RS_DEBUG->print("RS_MakerCamSVG::writeEntitiesFromBlock: Writing entities from layer ...");

	for(auto e: *document){

        if (e->getLayer() == layer) {

            if (!(e->getFlag(RS2::FlagUndone))) {

                writeEntity(e);
            }
        }
    }
}

void LC_MakerCamSVG::writeEntity(RS_Entity* entity) {

    RS_DEBUG->print("RS_MakerCamSVG::writeEntity: Found entity ...");

    switch (entity->rtti()) {
        case RS2::EntityInsert:
            writeInsert((RS_Insert*)entity);
            break;
        case RS2::EntityPoint:
            writePoint((RS_Point*)entity);
            break;
        case RS2::EntityLine:
            writeLine((RS_Line*)entity);
            break;
        case RS2::EntityPolyline:
            writePolyline((RS_Polyline*)entity);
            break;
        case RS2::EntityCircle:
            writeCircle((RS_Circle*)entity);
            break;
        case RS2::EntityArc:
            writeArc((RS_Arc*)entity);
            break;
        case RS2::EntityEllipse:
            writeEllipse((RS_Ellipse*)entity);
            break;

        default:
            RS_DEBUG->print("RS_MakerCamSVG::writeEntity: Entity with type '%d' not yet implemented",
                            (int)entity->rtti());
            break;
    }
}

void LC_MakerCamSVG::writeInsert(RS_Insert* insert) {

    RS_Block* block = insert->getBlockForInsert();

    RS_Vector insertionpoint = convertToSvg(insert->getInsertionPoint());

    if (writeBlocksInline) {

        RS_DEBUG->print("RS_MakerCamSVG::writeInsert: Writing insert inline ...");

        offset.set(insertionpoint.x, insertionpoint.y - (max.y - min.y));

        xmlWriter->addElement("g", NAMESPACE_URI_SVG);

        xmlWriter->addAttribute("blockname", qPrintable(block->getName()), NAMESPACE_URI_LC);

        writeLayers(block);

        xmlWriter->closeElement();

        offset.set(0, 0);
    }
    else {

        RS_DEBUG->print("RS_MakerCamSVG::writeInsert: Writing insert as reference to block ...");

        xmlWriter->addElement("use", NAMESPACE_URI_SVG);

        xmlWriter->addAttribute("x", numXml(insertionpoint.x));
        xmlWriter->addAttribute("y", numXml(insertionpoint.y - (max.y - min.y)));
        xmlWriter->addAttribute("href", "#" + std::to_string(block->getId()), NAMESPACE_URI_XLINK);

        xmlWriter->closeElement();
    }
}

void LC_MakerCamSVG::writePoint(RS_Point* point) {

    RS_DEBUG->print("RS_MakerCamSVG::writePoint: Writing point ...");

    // NOTE: There is no "point" element in SVG, therefore creating a circle
    //       with minimal radius.

    RS_Vector center = convertToSvg(point->getPos());

    xmlWriter->addElement("circle", NAMESPACE_URI_SVG);

    xmlWriter->addAttribute("cx", numXml(center.x));
    xmlWriter->addAttribute("cy", numXml(center.y));
    xmlWriter->addAttribute("r", numXml(0.1));

    xmlWriter->closeElement();
}

void LC_MakerCamSVG::writeLine(RS_Line* line) {

    RS_DEBUG->print("RS_MakerCamSVG::writeLine: Writing line ...");

    RS_Vector startpoint = convertToSvg(line->getStartpoint());
    RS_Vector endpoint = convertToSvg(line->getEndpoint());

    xmlWriter->addElement("line", NAMESPACE_URI_SVG);

    xmlWriter->addAttribute("x1", numXml(startpoint.x));
    xmlWriter->addAttribute("y1", numXml(startpoint.y));
    xmlWriter->addAttribute("x2", numXml(endpoint.x));
    xmlWriter->addAttribute("y2", numXml(endpoint.y));

    xmlWriter->closeElement();
}

void LC_MakerCamSVG::writePolyline(RS_Polyline* polyline) {

    RS_DEBUG->print("RS_MakerCamSVG::writePolyline: Writing polyline ...");

    std::string path = svgPathMoveTo(convertToSvg(polyline->getStartpoint()));

	for(auto entity: *polyline){

        if (!entity->isAtomic()) {
            continue;
        }

        if (entity->rtti() == RS2::EntityArc) {

            path += svgPathArc((RS_Arc*)entity);
        }
        else {

            path += svgPathLineTo(convertToSvg(entity->getEndpoint()));
        }
    }

    if (polyline->isClosed()) {

        path += svgPathClose();
    }

    xmlWriter->addElement("path", NAMESPACE_URI_SVG);

    xmlWriter->addAttribute("d", path);

    xmlWriter->closeElement();
}

void LC_MakerCamSVG::writeCircle(RS_Circle* circle) {

    RS_DEBUG->print("RS_MakerCamSVG::writeCircle: Writing circle ...");

    RS_Vector center = convertToSvg(circle->getCenter());

    xmlWriter->addElement("circle", NAMESPACE_URI_SVG);

    xmlWriter->addAttribute("cx", numXml(center.x));
    xmlWriter->addAttribute("cy", numXml(center.y));
    xmlWriter->addAttribute("r", numXml(circle->getRadius()));

    xmlWriter->closeElement();
}

void LC_MakerCamSVG::writeArc(RS_Arc* arc) {

    RS_DEBUG->print("RS_MakerCamSVG::writeArc: Writing arc ...");

    std::string path = svgPathMoveTo(convertToSvg(arc->getStartpoint())) +
                       svgPathArc(arc);

    xmlWriter->addElement("path", NAMESPACE_URI_SVG);

    xmlWriter->addAttribute("d", path);

    xmlWriter->closeElement();
}

void LC_MakerCamSVG::writeEllipse(RS_Ellipse* ellipse) {

    RS_Vector center = convertToSvg(ellipse->getCenter());
	const RS_Vector centerTranslation=center - ellipse->getCenter();

    double majorradius = ellipse->getMajorRadius();
    double minorradius = ellipse->getMinorRadius();

    if (convertEllipsesToBeziers) {

        std::string path = "";

        if (ellipse->isArc()) {

            const int segments = 4;

            RS_DEBUG->print("RS_MakerCamSVG::writeEllipse: Writing ellipse arc approximated by 'path' with %d cubic bézier segments (as discussed in https://www.spaceroots.org/documents/ellipse/elliptical-arc.pdf) ...", segments);

            double x_axis_rotation = 2 * M_PI - ellipse->getAngle();

            double start_angle = 2 * M_PI - ellipse->getAngle2();
            double end_angle = 2 * M_PI - ellipse->getAngle1();

            if (ellipse->isReversed()) {
                double temp_angle = start_angle;
                start_angle = end_angle;
                end_angle = temp_angle;
            }

            if (end_angle <= start_angle) {
                end_angle += 2 * M_PI;
            }

            double total_angle = end_angle - start_angle;

            double alpha = calcAlpha(total_angle / segments);

			RS_Vector start_point = centerTranslation + ellipse->getEllipsePoint(start_angle);

            path = svgPathMoveTo(start_point);

            for (int i = 1; i <= segments; i++) {
                double segment_start_angle = start_angle + ((i - 1) / (double)segments) * total_angle;
                double segment_end_angle = start_angle + (i / (double)segments) * total_angle;

				RS_Vector segment_start_point = centerTranslation + ellipse->getEllipsePoint(segment_start_angle);
				RS_Vector segment_end_point = centerTranslation + ellipse->getEllipsePoint(segment_end_angle);

                RS_Vector segment_control_point_1 = segment_start_point + calcEllipsePointDerivative(majorradius, minorradius, x_axis_rotation, segment_start_angle) * alpha;
                RS_Vector segment_control_point_2 = segment_end_point - calcEllipsePointDerivative(majorradius, minorradius, x_axis_rotation, segment_end_angle) * alpha;

                path += svgPathCurveTo(segment_end_point, segment_control_point_1, segment_control_point_2);
            }
        }
        else {

            RS_DEBUG->print("RS_MakerCamSVG::writeEllipse: Writing ellipse approximated by 'path' with 4 cubic bézier segments (as discussed in http://www.tinaja.com/glib/ellipse4.pdf) ...");

            const double kappa = 0.551784;

            RS_Vector major {majorradius, 0.0};
            RS_Vector minor {0.0, minorradius};

            RS_Vector flip_y {1.0, -1.0};

            major.rotate(ellipse->getAngle());
            minor.rotate(ellipse->getAngle());

            major.scale(flip_y);
            minor.scale(flip_y);

            RS_Vector offsetmajor {major * kappa};
            RS_Vector offsetminor {minor * kappa};

            path = svgPathMoveTo(center - major) +
                   svgPathCurveTo((center - minor), (center - major - offsetminor), (center - minor - offsetmajor)) +
                   svgPathCurveTo((center + major), (center - minor + offsetmajor), (center + major - offsetminor)) +
                   svgPathCurveTo((center + minor), (center + major + offsetminor), (center + minor + offsetmajor)) +
                   svgPathCurveTo((center - major), (center + minor - offsetmajor), (center - major + offsetminor)) +
                   svgPathClose();
        }

        xmlWriter->addElement("path", NAMESPACE_URI_SVG);

        xmlWriter->addAttribute("d", path);

        xmlWriter->closeElement();
    }
    else {

        if (ellipse->isArc()) {

            RS_DEBUG->print("RS_MakerCamSVG::writeEllipse: Writing ellipse arc as 'path' with arc segments ...");

            double x_axis_rotation = 180 - (RS_Math::rad2deg(ellipse->getAngle()));

            double startangle = RS_Math::rad2deg(ellipse->getAngle1());
            double endangle = RS_Math::rad2deg(ellipse->getAngle2());

            if (endangle <= startangle) {
                endangle += 360;
            }

            bool large_arc_flag = ((endangle - startangle) > 180);
            bool sweep_flag = false;

            if (ellipse->isReversed()) {
                large_arc_flag = !large_arc_flag;
                sweep_flag = !sweep_flag;
            }

            std::string path = svgPathMoveTo(convertToSvg(ellipse->getStartpoint())) +
                               svgPathArc(convertToSvg(ellipse->getEndpoint()), majorradius, minorradius, x_axis_rotation, large_arc_flag, sweep_flag);

            xmlWriter->addElement("path", NAMESPACE_URI_SVG);

            xmlWriter->addAttribute("d", path);

            xmlWriter->closeElement();
        }
        else {

            RS_DEBUG->print("RS_MakerCamSVG::writeEllipse: Writing full ellipse as 'ellipse' ...");

            double angle = 180 - (RS_Math::rad2deg(ellipse->getAngle()) - 90);

            std::string transform = "translate(" + numXml(center.x) + ", " + numXml(center.y) + ") " +
                                    "rotate(" + numXml(angle) + ")";

            xmlWriter->addElement("ellipse", NAMESPACE_URI_SVG);

            xmlWriter->addAttribute("rx", numXml(minorradius));
            xmlWriter->addAttribute("ry", numXml(majorradius));
            xmlWriter->addAttribute("transform", transform);

            xmlWriter->closeElement();
        }
    }
}

std::string LC_MakerCamSVG::numXml(double value) {

    return RS_Utility::doubleToString(value, 8).toStdString();
}

RS_Vector LC_MakerCamSVG::convertToSvg(RS_Vector vector) {

    RS_Vector translated((vector.x - min.x), (max.y - vector.y));

    return translated + offset;
}

std::string LC_MakerCamSVG::svgPathClose() {

    return "Z ";
}

std::string LC_MakerCamSVG::svgPathCurveTo(RS_Vector point, RS_Vector controlpoint1, RS_Vector controlpoint2) {

    return "C" + numXml(controlpoint1.x) + "," + numXml(controlpoint1.y) + " " +
           numXml(controlpoint2.x) + "," + numXml(controlpoint2.y) + " " +
           numXml(point.x) + "," + numXml(point.y) + " ";
}

std::string LC_MakerCamSVG::svgPathLineTo(RS_Vector point) {

    return "L" + numXml(point.x) + "," + numXml(point.y) + " ";
}

std::string LC_MakerCamSVG::svgPathMoveTo(RS_Vector point) {

    return "M" + numXml(point.x) + "," + numXml(point.y) + " ";
}

std::string LC_MakerCamSVG::svgPathArc(RS_Arc* arc) {

    RS_Vector endpoint = convertToSvg(arc->getEndpoint());
    double radius = arc->getRadius();

    double startangle = RS_Math::rad2deg(arc->getAngle1());
    double endangle = RS_Math::rad2deg(arc->getAngle2());


    if (endangle <= startangle) {
        endangle += 360;
    }

    bool large_arc_flag = ((endangle - startangle) > 180);
    bool sweep_flag = false;

    if (arc->isReversed())
    {
        large_arc_flag = !large_arc_flag;
        sweep_flag = !sweep_flag;
    }

    return svgPathArc(endpoint, radius, radius, 0.0, large_arc_flag, sweep_flag);
}

std::string LC_MakerCamSVG::svgPathArc(RS_Vector point, double radius_x, double radius_y, double x_axis_rotation, bool large_arc_flag, bool sweep_flag) {

    return "A" + numXml(radius_x) + "," + numXml(radius_y) + " " +
           numXml(x_axis_rotation) + " " +
           (large_arc_flag ? "1" : "0") + "," +
           (sweep_flag ? "1" : "0") + " " +
           numXml(point.x) + "," + numXml(point.y) + " ";
}

RS_Vector LC_MakerCamSVG::calcEllipsePointDerivative(double majorradius, double minorradius, double x_axis_rotation, double angle) {

    RS_Vector vector((-majorradius * cos(x_axis_rotation) * sin(angle)) - (minorradius * sin(x_axis_rotation) * cos(angle)),
                     (-majorradius * sin(x_axis_rotation) * sin(angle)) + (minorradius * cos(x_axis_rotation) * cos(angle)));

    return vector;
}

double LC_MakerCamSVG::calcAlpha(double angle) {

    return sin(angle) * ((sqrt(4.0 + 3.0 * pow(tan(angle / 2), 2.0)) - 1.0) / 3.0);
}
