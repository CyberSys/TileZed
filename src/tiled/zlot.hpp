/*
 * zlot.hpp
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

#ifndef ZLOT_H
#define ZLOT_H

#include "map.h"

#include <QMap>
#include <QVector>
#include <QPoint>
#include "ztilelayergroup.h"

namespace Tiled {

class Cell;
class TileLayer;

namespace Internal {
}

class ZLot;
class ZLotTileLayerGroup : public ZTileLayerGroup
{
public:
	ZLotTileLayerGroup(int level, ZLot *owner);

	// ZTileLayerGroup
	virtual QRect bounds() const { return mBounds; }
	virtual QMargins drawMargins() const { return mMargins; }
	virtual bool orderedCellsAt(const QPoint &point, QVector<const Cell*>& cells) const;
    virtual void prepareDrawing(const MapRenderer *renderer, const QRect &rect)
    {
        Q_UNUSED(renderer)
        Q_UNUSED(rect)
    }

	QRect _bounds() { return ZTileLayerGroup::bounds(); }
	QMargins _drawMargins() { return ZTileLayerGroup::drawMargins(); }

	ZLot *mLot;

	// These never change
	QRect mBounds;
	QMargins mMargins;
};

class ZLot
{
public:
	ZLot(Map *map, Map::Orientation orient);
	~ZLot();

	bool groupForTileLayer(TileLayer *tl, uint *group);
	bool orderedCellsAt(int level, const QPoint &point, QVector<const Cell*>& cells) const;
	const ZTileLayerGroup *tileLayersForLevel(int level) const;
	void setLayerVisibility(const QString &name, bool visible) const;
	Map *map() const { return mMap; }
	int minLevel() const { return mMinLevel; }

private:
	Map *mMap;
	QMap<int,ZLotTileLayerGroup*> mLevelToTileLayers;
	QMap<QString,QVector<Layer*>> mLayersByName;
	Map::Orientation mOrientation;
	int mMinLevel;
};

} // namespace Tiled

#endif // ZLOT_H
