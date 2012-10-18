/*
 * Copyright 2012, Tim Baker <treectrl@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "buildingpreviewwindow.h"
#include "ui_buildingpreviewwindow.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingeditorwindow.h"

#include "mapcomposite.h"
#include "mapmanager.h"
#include "zoomable.h"

#include "isometricrenderer.h"
#include "map.h"
#include "maprenderer.h"
#include "tilelayer.h"
#include "tileset.h"
#include "tilesetmanager.h"
#include "zlevelrenderer.h"

#include <QMessageBox>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingPreviewWindow::BuildingPreviewWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::BuildingPreviewWindow),
    mDocument(0),
    mScene(new BuildingPreviewScene(this))
{
    ui->setupUi(this);

    ui->graphicsView->setScene(mScene);

    ui->graphicsView->zoomable()->connectToComboBox(ui->zoomComboBox);
}

void BuildingPreviewWindow::setDocument(BuildingDocument *doc)
{
    mDocument = doc;

    mScene->setDocument(doc);
}

/////

CompositeLayerGroupItem::CompositeLayerGroupItem(CompositeLayerGroup *layerGroup,
                                                 MapRenderer *renderer,
                                                 QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , mLayerGroup(layerGroup)
    , mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    mBoundingRect = layerGroup->boundingRect(mRenderer);
}

QRectF CompositeLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void CompositeLayerGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    if (mLayerGroup->needsSynch() /*mBoundingRect != mLayerGroup->boundingRect(mRenderer)*/)
        return;

    mRenderer->drawTileLayerGroup(p, mLayerGroup, option->exposedRect);
#ifdef _DEBUG
    p->drawRect(mBoundingRect);

    mRenderer->drawGrid(p, option->exposedRect, Qt::black, mLayerGroup->level());
#endif
}

void CompositeLayerGroupItem::synchWithTileLayers()
{
//    if (layerGroup()->needsSynch())
        layerGroup()->synch();
    update();
}

void CompositeLayerGroupItem::updateBounds()
{
    QRectF bounds = layerGroup()->boundingRect(mRenderer);
    if (bounds != mBoundingRect) {
        prepareGeometryChange();
        mBoundingRect = bounds;
    }
}

/////

BuildingPreviewScene::BuildingPreviewScene(QWidget *parent) :
    QGraphicsScene(parent),
    mDocument(0),
    mMapComposite(0),
    mMap(0),
    mRenderer(0)
{
    setBackgroundBrush(Qt::darkGray);
}

BuildingPreviewScene::~BuildingPreviewScene()
{
    delete mMapComposite;
    if (mMap)
        TilesetManager::instance()->removeReferences(mMap->tilesets());
    delete mMap;
    delete mRenderer;
}

void BuildingPreviewScene::setDocument(BuildingDocument *doc)
{
    mDocument = doc;

    delete mMapComposite;
    if (mMap)
        TilesetManager::instance()->removeReferences(mMap->tilesets());
    delete mMap;
    delete mRenderer;
    qDeleteAll(mLayerGroupItems);
    mLayerGroupItems.clear();

    mMap = new Map(Map::Isometric, doc->building()->width(),
                   doc->building()->height(), 64, 32);

    // Add tilesets from MapBaseXMLLots.txt
    foreach (Tileset *ts, mTilesetByName)
        mMap->addTileset(ts);
    TilesetManager::instance()->addReferences(mMap->tilesets());

    foreach (BuildingFloor *floor, doc->building()->floors()) {
        TileLayer *tl = new TileLayer(tr("%1_Floor").arg(floor->level()), 0, 0,
                                      floor->width(), floor->height());
        mMap->addLayer(tl);

        tl = new TileLayer(tr("%1_Walls").arg(floor->level()), 0, 0,
                           floor->width(), floor->height());
        mMap->addLayer(tl);

        tl = new TileLayer(tr("%1_Frames").arg(floor->level()), 0, 0,
                           floor->width(), floor->height());
        mMap->addLayer(tl);

        tl = new TileLayer(tr("%1_Doors").arg(floor->level()), 0, 0,
                           floor->width(), floor->height());
        mMap->addLayer(tl);

        tl = new TileLayer(tr("%1_Furniture").arg(floor->level()), 0, 0,
                           floor->width(), floor->height());
        mMap->addLayer(tl);
    }

    switch (mMap->orientation()) {
    case Map::Isometric:
        mRenderer = new IsometricRenderer(mMap);
        break;
    case Map::LevelIsometric:
        mRenderer = new ZLevelRenderer(mMap);
        break;
    default:
        return;
    }

    MapInfo *mapInfo = MapManager::instance()->newFromMap(mMap);

    mMapComposite = new MapComposite(mapInfo);

    foreach (CompositeLayerGroup *lg, mMapComposite->sortedLayerGroups()) {
        CompositeLayerGroupItem *item = new CompositeLayerGroupItem(lg,
                                                                    mRenderer);
        mLayerGroupItems[lg->level()] = item;
        item->synchWithTileLayers();
        addItem(item);
    }

    connect(mDocument, SIGNAL(roomAtPositionChanged(BuildingFloor*,QPoint)),
            SLOT(roomAtPositionChanged(BuildingFloor*,QPoint)));
    connect(mDocument, SIGNAL(roomDefinitionChanged()),
            SLOT(roomDefinitionChanged()));
}

void BuildingPreviewScene::BuildingToMap()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        floor->LayoutToSquares();
        BuildingFloorToTileLayers(floor, mMapComposite->tileLayersForLevel(floor->level())->layers());
    }
}

int WallType_getIndexFromSection(WallType *type, BuildingFloor::WallTile::WallSection section)
{
    int index = type->FirstIndex;

    switch (section)
    {
        case BuildingFloor::WallTile::N:
            index += 1;
            break;
        case BuildingFloor::WallTile::NDoor:
            index += 11;
        break;
        case BuildingFloor::WallTile::NW:
            index += 2;
            break;
        case BuildingFloor::WallTile::W:
            break;
        case BuildingFloor::WallTile::WDoor:
            index += 10;

            break;
        case BuildingFloor::WallTile::NWindow:
            index += 9;
            break;
        case BuildingFloor::WallTile::WWindow:
            index += 8;

            break;
        case BuildingFloor::WallTile::SE:
            index += 3;
            break;
    }

    return index;
}

void BuildingPreviewScene::BuildingFloorToTileLayers(BuildingFloor *floor,
                                                     const QVector<TileLayer *> &layers)
{
    int index = 0;
    foreach (TileLayer *tl, layers) {
        tl->erase(QRegion(QRect(0, 0, tl->width(), tl->height())));
        for (int x = 0; x < floor->width(); x++) {
            for (int y = 0; y < floor->height(); y++) {
                if (index == Floor) {
                    BuildingFloor::FloorTile *tile = floor->squares[x][y].floorTile;
                    if (tile && tile->Type) {
                        tl->setCell(x, y, Cell(mTilesetByName[tile->Type->Tilesheet]->tileAt(tile->Type->Index)));
                    }
                }
                if (index == Wall) {
                    if (floor->squares[x][y].walls.count()) {
                        BuildingFloor::WallTile *tile = floor->squares[x][y].walls[0];
                        if (tile && tile->Type) {
                            tl->setCell(x, y, Cell(mTilesetByName[tile->Type->Tilesheet]->tileAt(WallType_getIndexFromSection(tile->Type, tile->Section))));
                        }
                    }
                }
            }
        }
        index++;
    }
}

void BuildingPreviewScene::roomAtPositionChanged(BuildingFloor *floor, const QPoint &pos)
{
    floor->LayoutToSquares();
    BuildingFloorToTileLayers(floor, mMapComposite->tileLayersForLevel(floor->level())->layers());
    mLayerGroupItems[floor->level()]->synchWithTileLayers();
    mLayerGroupItems[floor->level()]->updateBounds();
}

void BuildingPreviewScene::roomDefinitionChanged()
{
    foreach (BuildingFloor *floor, mDocument->building()->floors()) {
        floor->LayoutToSquares();
        BuildingFloorToTileLayers(floor, mMapComposite->tileLayersForLevel(floor->level())->layers());
        mLayerGroupItems[floor->level()]->synchWithTileLayers();
        mLayerGroupItems[floor->level()]->updateBounds();
    }
}

/////

BuildingPreviewView::BuildingPreviewView(QWidget *parent) :
    QGraphicsView(parent),
    mZoomable(new Zoomable(this))
{
    // This enables mouseMoveEvent without any buttons being pressed
    setMouseTracking(true);

    connect(mZoomable, SIGNAL(scaleChanged(qreal)), SLOT(adjustScale(qreal)));
}

void BuildingPreviewView::mouseMoveEvent(QMouseEvent *event)
{
    mLastMousePos = event->globalPos();
    mLastMouseScenePos = mapToScene(viewport()->mapFromGlobal(mLastMousePos));
}

/**
 * Override to support zooming in and out using the mouse wheel.
 */
void BuildingPreviewView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier
        && event->orientation() == Qt::Vertical)
    {
        // No automatic anchoring since we'll do it manually
        setTransformationAnchor(QGraphicsView::NoAnchor);

        mZoomable->handleWheelDelta(event->delta());

        // Place the last known mouse scene pos below the mouse again
        QWidget *view = viewport();
        QPointF viewCenterScenePos = mapToScene(view->rect().center());
        QPointF mouseScenePos = mapToScene(view->mapFromGlobal(mLastMousePos));
        QPointF diff = viewCenterScenePos - mouseScenePos;
        centerOn(mLastMouseScenePos + diff);

        // Restore the centering anchor
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        return;
    }

    QGraphicsView::wheelEvent(event);
}

void BuildingPreviewView::adjustScale(qreal scale)
{
    setTransform(QTransform::fromScale(scale, scale));
    setRenderHint(QPainter::SmoothPixmapTransform,
                  mZoomable->smoothTransform());
}
