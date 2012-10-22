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

#ifndef BUILDING_H
#define BUILDING_H

#include <QList>
#include <QString>

namespace BuildingEditor {

class BuildingFloor;
class BuildingTemplate;
class Room;

class Building
{
public:
    Building(int width, int height, BuildingTemplate *btemplate);

    int width() const { return mWidth; }
    int height() const { return mHeight; }

    const QList<BuildingFloor*> &floors() const
    { return mFloors; }

    BuildingFloor *floor(int index)
    { return mFloors.at(index); }

    int floorCount() const
    { return mFloors.count(); }

    void insertFloor(int index, BuildingFloor *floor);
    BuildingFloor *removeFloor(int index);

    const QList<Room*> &rooms() const
    { return mRooms; }

    Room *room(int index) const
    { return mRooms.at(index); }

    int roomCount() const
    { return mRooms.count(); }

    int indexOf(Room *room) const
    { return mRooms.indexOf(room); }

    void insertRoom(int index, Room *room);
    Room *removeRoom(int index);

    QString exteriorWall() const
    { return mExteriorWall; }

    void setExteriorWall(const QString &tileName)
    { mExteriorWall = tileName; }

    QString doorTile() const
    { return mDoorTile; }

    void setDoorTile(const QString &tileName)
    { mDoorTile = tileName; }

    QString doorFrameTile() const
    { return mDoorFrameTile; }

    void setDoorFrameTile(const QString &tileName)
    { mDoorFrameTile = tileName; }

    QString windowTile() const
    { return mWindowTile; }

    void setWindowTile(const QString &tileName)
    { mWindowTile = tileName; }

    QString stairsTile() const
    { return mStairsTile; }

    void setStairsTile(const QString &tileName)
    { mStairsTile = tileName; }

private:
    int mWidth, mHeight;
    QList<BuildingFloor*> mFloors;
    QList<Room*> mRooms;
    QString mExteriorWall;
    QString mDoorTile;
    QString mDoorFrameTile;
    QString mWindowTile;
    QString mStairsTile;
};

} // namespace BuildingEditor

#endif // BUILDING_H
