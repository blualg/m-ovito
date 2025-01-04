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


#include <ovito/gui/desktop/GUI.h>
#include <ovito/mesh/surface/SurfaceMesh.h>
#include <ovito/stdobj/gui/properties/PropertyInspectionApplet.h>

namespace Ovito {

class SurfaceMeshInspectionApplet;  // defined below

/**
 * \brief Data inspector page for surface meshes vertices.
 */
class SurfaceMeshVertexInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(SurfaceMeshVertexInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(SurfaceMeshInspectionApplet* parentApplet) {
        PropertyInspectionApplet::initializeObject(SurfaceMeshVertices::OOClass());
        _parentApplet = parentApplet;
    }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Determines the list of data objects that are displayed by the applet.
    virtual std::vector<ConstDataObjectPath> getDataObjectPaths() override;

private:

    /// The parent applet for the SurfaceMesh, which hosts this sub-object applet.
    SurfaceMeshInspectionApplet* _parentApplet;
};

/**
 * \brief Data inspector page for surface meshes faces.
 */
class SurfaceMeshFaceInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(SurfaceMeshFaceInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(SurfaceMeshInspectionApplet* parentApplet) {
        PropertyInspectionApplet::initializeObject(SurfaceMeshFaces::OOClass());
        _parentApplet = parentApplet;
    }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Determines the list of data objects that are displayed by the applet.
    virtual std::vector<ConstDataObjectPath> getDataObjectPaths() override;

private:

    /// The parent applet for the SurfaceMesh, which hosts this sub-object applet.
    SurfaceMeshInspectionApplet* _parentApplet;
};

/**
 * \brief Data inspector page for surface meshes regions.
 */
class SurfaceMeshRegionInspectionApplet : public PropertyInspectionApplet
{
    OVITO_CLASS(SurfaceMeshRegionInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject(SurfaceMeshInspectionApplet* parentApplet) {
        PropertyInspectionApplet::initializeObject(SurfaceMeshRegions::OOClass());
        _parentApplet = parentApplet;
    }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Determines the list of data objects that are displayed by the applet.
    virtual std::vector<ConstDataObjectPath> getDataObjectPaths() override;

private:

    /// The parent applet for the SurfaceMesh, which hosts this sub-object applet.
    SurfaceMeshInspectionApplet* _parentApplet;
};

/**
 * \brief Data inspector page for surface meshes.
 */
class SurfaceMeshInspectionApplet : public DataInspectionApplet
{
    OVITO_CLASS(SurfaceMeshInspectionApplet)
    Q_OBJECT

public:

    /// Constructor.
    void initializeObject() {
        DataInspectionApplet::initializeObject(SurfaceMesh::OOClass());
    }

    /// Returns the key value for this applet that is used for ordering the applet tabs.
    virtual int orderingKey() const override { return 220; }

    /// Lets the applet create the UI widget that is to be placed into the data inspector panel.
    virtual QWidget* createWidget() override;

    /// Selects a specific data object in this applet.
    virtual bool selectDataObject(const PipelineNode* createdByNode, const QString& objectIdentifierHint, const QVariant& modeHint) override;

private Q_SLOTS:

    /// Is called when the user selects a different data object from the list.
    void onCurrentDataObjectChanged();

private:

    OORef<SurfaceMeshVertexInspectionApplet> _verticesApplet;
    OORef<SurfaceMeshFaceInspectionApplet> _facesApplet;
    OORef<SurfaceMeshRegionInspectionApplet> _regionsApplet;
    QStackedWidget* _stackedWidget;
    QAction* _switchToVerticesAction;
    QAction* _switchToFacesAction;
    QAction* _switchToRegionsAction;
};

}   // End of namespace
