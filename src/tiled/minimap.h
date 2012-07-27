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

#ifndef MINIMAP_H
#define MINIMAP_H

#include <QGraphicsItem>
#include <QGraphicsView>

class MapComposite;
namespace Tiled {
class Layer;
class MapObject;
class MapRenderer;

namespace Internal {
class MapScene;
class MapView;
class ZomboidScene;
}
}

/**
  * The MiniMap
  *
  * In the first implementation, the mini-map displayed the actual MapScene,
  * i.e., the map being edited. This is possible because a single
  * QGraphicsScene can be displayed by multiple QGraphicsViews. However, this
  * displayed everything, including the grid, level-highlight rectangle, map
  * objects, and was updated when layers were hidden/shown and during editing.
  *
  * In the second implementation, a new scene was created that only showed the
  * tile layers in the map being edited, and those layers were always fully
  * visible. Changes during editing were reflected right away in the mini-map.
  * It looks good, but displaying a full 300x300 map with 100 tile layers
  * scaled down is slow at times. I tried using a QGLWidget, but having 2
  * OpenGL widgets (1 for the map scene, 1 for the mini-map) was very slow.
  *
  * The third (and current) implementation uses a map image. The image is
  * updated when the map is edited and when lots are added/removed/moved.
  */

/**
  * MiniMap item for drawing a map image.
  */
class MiniMapItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    MiniMapItem(Tiled::Internal::ZomboidScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = 0);

    void updateImage(const QRectF &dirtyRect = QRectF());
    void updateImageBounds();
    void recreateImage();

    typedef Tiled::Layer Layer; // hack for signals/slots
private slots:
    void sceneRectChanged(const QRectF &sceneRect);

    void layerAdded(int index);
    void layerRemoved(int index);

    void onLotAdded(MapComposite *lot, Tiled::MapObject *mapObject);
    void onLotRemoved(MapComposite *lot, Tiled::MapObject *mapObject);
    void onLotUpdated(MapComposite *lot, Tiled::MapObject *mapObject);

    void regionAltered(const QRegion &region, Layer *layer);

private:
    Tiled::Internal::ZomboidScene *mScene;
    Tiled::MapRenderer *mRenderer;
    QImage *mMapImage;
    QRectF mMapImageBounds;
    QRectF mLevelZeroBounds;
    MapComposite *mMapComposite;
    QMap<MapComposite*,QRectF> mLotBounds;
};

class MiniMap : public QGraphicsView
{
    Q_OBJECT
public:
    MiniMap(Tiled::Internal::MapView *parent);

    void setMapScene(Tiled::Internal::MapScene *scene);
    void viewRectChanged();
    qreal scale();

    void setExtraItem(MiniMapItem *item);

public slots:
    void sceneRectChanged(const QRectF &sceneRect);
    void bigger();
    void smaller();
    void updateImage();
    void widthChanged(int width);

protected:
    bool event(QEvent *event);

private:
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    Tiled::Internal::MapView *mMapView;
    Tiled::Internal::MapScene *mMapScene;
    QFrame *mButtons;
    int mWidth;
    QGraphicsPolygonItem *mViewportItem;
    MiniMapItem *mExtraItem;
};

#endif // MINIMAP_H
