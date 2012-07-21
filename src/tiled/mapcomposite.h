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

#ifndef MAPCOMPOSITE_H
#define MAPCOMPOSITE_H

#include "ztilelayergroup.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QVector>

class MapInfo;

namespace Tiled {
class Layer;
class Map;
}

class MapComposite;

class CompositeLayerGroup : public Tiled::ZTileLayerGroup
{
public:
    CompositeLayerGroup(MapComposite *owner, int level);

    void addTileLayer(Tiled::TileLayer *layer, int index);
    void removeTileLayer(Tiled::TileLayer *layer);

    void prepareDrawing(const Tiled::MapRenderer *renderer, const QRect &rect);
    bool orderedCellsAt(const QPoint &pos, QVector<const Tiled::Cell*>& cells) const;

    QRect bounds() const;
    QMargins drawMargins() const;

    QRectF boundingRect(const Tiled::MapRenderer *renderer) const;

    void setLayerVisibility(const QString &name, bool visible) const;
    void layerRenamed(Tiled::TileLayer *layer);

    MapComposite *owner() const { return mOwner; }
    void synch();

private:
    MapComposite *mOwner;
    bool mAnyVisibleLayers;
    QRect mTileBounds;
    QRect mSubMapTileBounds;
    QMargins mDrawMargins;
    QVector<bool> mEmptyLayers;
    QMap<QString,QVector<Tiled::Layer*> > mLayersByName;

    struct SubMapLayers
    {
        SubMapLayers()
            : mSubMap(0)
            , mLayerGroup(0)
        {
        }
        SubMapLayers(MapComposite *subMap, CompositeLayerGroup *layerGroup)
            : mSubMap(subMap)
            , mLayerGroup(layerGroup)
        {
        }
        MapComposite *mSubMap;
        CompositeLayerGroup *mLayerGroup;
    };

    QVector<SubMapLayers> mPreparedSubMapLayers;
    QVector<SubMapLayers> mVisibleSubMapLayers;
};

class MapComposite : public QObject
{
    Q_OBJECT
public:
    MapComposite(MapInfo *mapInfo, MapComposite *parent = 0, const QPoint &positionInParent = QPoint(), int levelOffset = 0);
    ~MapComposite();

    static bool levelForLayer(Tiled::Layer *layer, int *levelPtr = 0);

    MapComposite *addMap(MapInfo *mapInfo, const QPoint &pos, int levelOffset);
    void removeMap(MapComposite *subMap);
    void moveSubMap(MapComposite *subMap, const QPoint &pos);

    Tiled::Map *map() const { return mMap; }
    MapInfo *mapInfo() const { return mMapInfo; }

    void layerAdded(int index);
    void layerAboutToBeRemoved(int index);
    void layerRenamed(int index);

    int layerGroupCount() const { return mLayerGroups.size(); }
    const QMap<int,CompositeLayerGroup*>& layerGroups() const { return mLayerGroups; }
    CompositeLayerGroup *tileLayersForLevel(int level) const;
    const QList<CompositeLayerGroup*> &sortedLayerGroups() const { return mSortedLayerGroups; }
    CompositeLayerGroup *layerGroupForLayer(Tiled::TileLayer *tl) const;

    const QVector<MapComposite*>& subMaps() const { return mSubMaps; }

    MapComposite *parent() const { return mParent; }

    void setOrigin(const QPoint &origin);
    QPoint origin() const { return mPos; }

    QPoint originRecursive() const;
    int levelRecursive() const;

    void setLevel(int level) { mLevelOffset = level; }
    int levelOffset() const { return mLevelOffset; }

    void setVisible(bool visible) { mVisible = visible; }
    bool isVisible() const { return mVisible; }

    void setGroupVisible(bool visible) { mGroupVisible = visible; }
    bool isGroupVisible() const { return mGroupVisible; }

    /**
      * Hack: when dragging a MapObjectItem representing a Lot, the map is hidden
      * at the start of the drag and shown when dragging is finished.  But I don't
      * want to affect the scene bounds, so instead of calling setVisible(false)
      * I call this.
      */
    void setHiddenDuringDrag(bool hidden) { mHiddenDuringDrag = hidden; }
    bool isHiddenDuringDrag() const { return mHiddenDuringDrag; }

    QRectF boundingRect(Tiled::MapRenderer *renderer, bool forceMapBounds = true) const;

    /**
      * Used when generating map images.
      */
    void saveVisibility();
    void restoreVisibility();

signals:
    void layerGroupAdded(int level);
    void layerAddedToGroup(int index);
    void layerAboutToBeRemovedFromGroup(int index);
    void layerRemovedFromGroup(int index, CompositeLayerGroup *oldGroup);
    void layerLevelChanged(int index, int oldLevel);

private:
    void addLayerToGroup(int index);
    void removeLayerFromGroup(int index);

    MapInfo *mMapInfo;
    Tiled::Map *mMap;
    QVector<MapComposite*> mSubMaps;
    QMap<int,CompositeLayerGroup*> mLayerGroups;
    QList<CompositeLayerGroup*> mSortedLayerGroups;

    MapComposite *mParent;
    QPoint mPos;
    int mLevelOffset;
    int mMinLevel;
    bool mVisible;
    bool mGroupVisible;
    bool mSavedGroupVisible;
    QVector<bool> mSavedLayerVisible;
    bool mHiddenDuringDrag;
};

#endif // MAPCOMPOSITE_H
