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

    void setLayerVisibility(const QString &name, bool visible) const;
    void layerRenamed(Tiled::TileLayer *layer);

    QRectF boundingRect(Tiled::MapRenderer *renderer) const;

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

    static bool levelForLayer(Tiled::Layer *layer, int *level);

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

    void setLevel(int level) { mLevelOffset = level; }
    int levelOffset() const { return mLevelOffset; }

    void setVisible(bool visible) { mVisible = visible; }
    bool isVisible() const { return mVisible; }

    void setGroupVisible(bool visible) { mGroupVisible = visible; }
    bool isGroupVisible() const { return mGroupVisible; }

    QRectF boundingRect(Tiled::MapRenderer *renderer, bool forceMapBounds = true) const;

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
};

#endif // MAPCOMPOSITE_H
