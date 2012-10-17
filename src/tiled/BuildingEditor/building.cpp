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

#include "building.h"

#include "buildingfloor.h"

using namespace BuildingEditor;

Building::Building(int width, int height) :
    mWidth(width),
    mHeight(height)
{
}

void Building::recreate(const QList<Layout *> &layouts, WallType *wallType)
{
#if 0
    qDeleteAll(mFloors);
    mFloors.clear();

    foreach (Layout *layout, layouts)
        mFloors += new BuildingFloor(layout);
#endif
}

void Building::insertFloor(int index, BuildingFloor *floor)
{
    mFloors.insert(index, floor);
}

BuildingFloor *Building::removeFloor(int index)
{
    return mFloors.takeAt(index);
}
