/*
 * zlotmanager.hpp
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

#ifndef ZLOTMANAGER_H
#define ZLOTMANAGER_H

#include "tiled_global.h"

#include <QObject>
#include <QMap>

namespace Tiled {

class MapObject;
class ZLot;

namespace Internal {
class MapDocument;
}

class ZLotManager : public QObject
{
    Q_OBJECT

public:
    static ZLotManager *instance();

    void handleMapObject(Internal::MapDocument *mapDoc, MapObject *mapObject);

signals:
	void lotAdded(ZLot *lot, Internal::MapDocument *mapDoc, MapObject *mapObject);
	void lotRemoved(ZLot *lot, Internal::MapDocument *mapDoc, MapObject *mapObject);
	void lotUpdated(ZLot *lot, Internal::MapDocument *mapDoc, MapObject *mapObject);

private:
    Q_DISABLE_COPY(ZLotManager)

    ZLotManager();
    ~ZLotManager();

	QMap<QString,ZLot*> mLots; // One ZLot per different .lot file
	QMap<MapObject*,ZLot*> mMapObjectToLot;
    static ZLotManager *mInstance;
};

} // namespace Tiled

#endif // ZLOTMANAGER_H
