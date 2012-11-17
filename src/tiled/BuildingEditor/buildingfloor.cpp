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

#include "buildingfloor.h"

#include "building.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

using namespace BuildingEditor;

BuildingFloor::BuildingFloor(Building *building, int level) :
    mBuilding(building),
    mLevel(level)
{
    int w = building->width();
    int h = building->height();

    mRoomAtPos.resize(w);
    mIndexAtPos.resize(w);
    for (int x = 0; x < w; x++) {
        mRoomAtPos[x].resize(h);
        mIndexAtPos[x].resize(h);
        for (int y = 0; y < h; y++) {
            mRoomAtPos[x][y] = 0;
        }
    }
}

BuildingFloor::~BuildingFloor()
{
    qDeleteAll(mObjects);
}

BuildingFloor *BuildingFloor::floorAbove() const
{
    return isTopFloor() ? 0 : mBuilding->floor(mLevel + 1);
}

BuildingFloor *BuildingFloor::floorBelow() const
{
    return isBottomFloor() ? 0 : mBuilding->floor(mLevel - 1);
}

bool BuildingFloor::isTopFloor() const
{
    return mLevel == mBuilding->floorCount() - 1;
}

bool BuildingFloor::isBottomFloor() const
{
    return mLevel == 0;
}

void BuildingFloor::insertObject(int index, BuildingObject *object)
{
    mObjects.insert(index, object);
}

BuildingObject *BuildingFloor::removeObject(int index)
{
    return mObjects.takeAt(index);
}

BuildingObject *BuildingFloor::objectAt(int x, int y)
{
    foreach (BuildingObject *object, mObjects)
        if (object->bounds().contains(x, y))
            return object;
    return 0;
}

void BuildingFloor::setGrid(const QVector<QVector<Room *> > &grid)
{
    mRoomAtPos = grid;

    mIndexAtPos.resize(mRoomAtPos.size());
    for (int x = 0; x < mIndexAtPos.size(); x++)
        mIndexAtPos[x].resize(mRoomAtPos[x].size());
}

static void ReplaceRoofSlope(RoofObject *ro, const QRect &r,
                        QVector<QVector<BuildingFloor::Square> > &squares,
                        RoofObject::RoofTile tile)
{
    if (r.isEmpty()) return;
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->slopeTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++)
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++)
            squares[x][y].ReplaceRoof(ro->slopeTiles(), offset);
}

static void ReplaceRoofGap(RoofObject *ro, const QRect &r,
                           QVector<QVector<BuildingFloor::Square> > &squares,
                           RoofObject::RoofTile tile)
{
    if (r.isEmpty()) return;
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->capTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++)
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++)
            squares[x][y].ReplaceRoofCap(ro->capTiles(), offset);
}

static void ReplaceRoofCap(RoofObject *ro, int x, int y,
                           QVector<QVector<BuildingFloor::Square> > &squares,
                           RoofObject::RoofTile tile)
{
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->capTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QPoint p = QPoint(x, y) + tileOffset;
    if (bounds.contains(p))
        squares[p.x()][p.y()].ReplaceRoofCap(ro->capTiles(), offset);
}

static void ReplaceRoofTop(RoofObject *ro, const QRect &r,
                           QVector<QVector<BuildingFloor::Square> > &squares)
{
    if (r.isEmpty()) return;
    int offset = 0;
    if (ro->depth() == RoofObject::Zero)
        offset = ro->isN() ? BTC_RoofTops::North3 : BTC_RoofTops::West3;
    else if (ro->depth() == RoofObject::One)
        offset = ro->isN() ? BTC_RoofTops::North1 : BTC_RoofTops::West1;
    else if (ro->depth() == RoofObject::Two)
        offset = ro->isN() ? BTC_RoofTops::North2 : BTC_RoofTops::West2;
    else if (ro->depth() == RoofObject::Three)
        offset = ro->isN() ? BTC_RoofTops::North3 : BTC_RoofTops::West3;
    QPoint tileOffset = ro->topTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QRect rOffset = r.translated(tileOffset) & bounds;
    for (int x = rOffset.left(); x <= rOffset.right(); x++)
        for (int y = rOffset.top(); y <= rOffset.bottom(); y++)
            (ro->depth() == RoofObject::Zero || ro->depth() == RoofObject::Three)
                ? squares[x][y].ReplaceFloor(ro->topTiles(), offset)
                : squares[x][y].ReplaceRoofTop(ro->topTiles(), offset);
}

static void ReplaceRoofCorner(RoofObject *ro, int x, int y,
                              QVector<QVector<BuildingFloor::Square> > &squares,
                              RoofObject::RoofTile tile)
{
    int offset = ro->getOffset(tile);
    QPoint tileOffset = ro->capTiles()->offset(offset);
    QRect bounds(0, 0, squares.size(), squares[0].size());
    QPoint p = QPoint(x, y) + tileOffset;
    if (bounds.contains(p))
        squares[p.x()][p.y()].ReplaceRoof(ro->slopeTiles(), offset);
}

static void ReplaceFurniture(int x, int y,
                             QVector<QVector<BuildingFloor::Square> > &squares,
                             BuildingTile *btile)
{
    if (!btile)
        return;
    QRect bounds(0, 0, squares.size() - 1, squares[0].size() - 1);
    if (bounds.contains(x, y))
        squares[x][y].ReplaceFurniture(btile);
}

void BuildingFloor::LayoutToSquares()
{
    int w = width() + 1;
    int h = height() + 1;
    // +1 for the outside walls;
    squares.clear();
    squares.resize(w);
    for (int x = 0; x < w; x++)
        squares[x].resize(h);

    BuildingTileEntry *wtype = 0;

    BuildingTileEntry *exteriorWall;
    QVector<BuildingTileEntry*> interiorWalls;
    QVector<BuildingTileEntry*> floors;

    exteriorWall = mBuilding->exteriorWall();

    foreach (Room *room, mBuilding->rooms()) {
        interiorWalls += room->Wall;
        floors += room->Floor;
    }

    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            Room *room = mRoomAtPos[x][y];
            mIndexAtPos[x][y] = room ? mBuilding->indexOf(room) : -1;
        }
    }

    // first put back walls in...

    // N first...

    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height() + 1; y++) {
            if (y == height() && mIndexAtPos[x][y - 1] >= 0) {
                // put N wall here...
                squares[x][y].ReplaceWall(exteriorWall, Square::WallOrientN);
            } else if (y < height() && mIndexAtPos[x][y] < 0 && y > 0 && mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) {
                squares[x][y].ReplaceWall(exteriorWall, Square::WallOrientN);
            } else if (y < height() && (y == 0 || mIndexAtPos[x][y-1] != mIndexAtPos[x][y]) && mIndexAtPos[x][y] >= 0) {
                squares[x][y].ReplaceWall(interiorWalls[mIndexAtPos[x][y]], Square::WallOrientN, false);
            }
        }
    }

    for (int x = 0; x < width()+1; x++)
    {
        for (int y = 0; y < height(); y++)
        {
            if (x == width() && mIndexAtPos[x - 1][y] >= 0)
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientW);
            }
            else if (x < width() && mIndexAtPos[x][y] < 0 && x > 0 && mIndexAtPos[x - 1][y] != mIndexAtPos[x][y])
            {
                wtype = exteriorWall;
                // If already contains a north, put in a west...
                if (squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientW);
            }
            else if (x < width() && mIndexAtPos[x][y] >= 0 && (x == 0 || mIndexAtPos[x - 1][y] != mIndexAtPos[x][y]))
            {
                wtype = interiorWalls[mIndexAtPos[x][y]];
                // If already contains a north, put in a west...
                if(squares[x][y].IsWallOrient(Square::WallOrientN))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientNW, false);
                else
                    // put W wall here...
                    squares[x][y].ReplaceWall(wtype, BuildingFloor::Square::WallOrientW, false);
            }

        }
    }

    for (int x = 0; x < width() + 1; x++)
    {
        for (int y = 0; y < height() + 1; y++)
        {
            if (x > 0 && y > 0)
            {
                if (squares[x][y].mTiles[Square::SectionWall]) // if (squares[x][y].walls.count() > 0)
                    continue;
                if (x < width() && mIndexAtPos[x][y - 1] >= 0)
                    wtype = interiorWalls[mIndexAtPos[x][y - 1]];
                else if (y < height() &&  mIndexAtPos[x-1][y] >= 0)
                    wtype = interiorWalls[mIndexAtPos[x - 1][y]];
                else
                    wtype = exteriorWall;
                // Put in the SE piece...
                if ((squares[x][y - 1].IsWallOrient(Square::WallOrientW) ||
                     squares[x][y - 1].IsWallOrient(Square::WallOrientNW)) &&
                        (squares[x - 1][y].IsWallOrient(Square::WallOrientN) ||
                         squares[x - 1][y].IsWallOrient(Square::WallOrientNW)))
                    squares[x][y].ReplaceWall(wtype, Square::WallOrientSE);
            }

        }
    }

    foreach (BuildingObject *object, mObjects) {
        int x = object->x();
        int y = object->y();
        if (Door *door = object->asDoor()) {
            squares[x][y].ReplaceDoor(door->tile(),
                                      door->isW() ? BTC_Doors::West
                                                  : BTC_Doors::North);
            squares[x][y].ReplaceFrame(door->frameTile(),
                                       door->isW() ? BTC_DoorFrames::West
                                                   : BTC_DoorFrames::North);
        }
        if (Window *window = object->asWindow()) {
            squares[x][y].ReplaceFrame(window->tile(),
                                       window->isW() ? BTC_Windows::West
                                                     : BTC_Windows::North);

            // Window curtains on exterior walls must be *inside* the
            // room.
            if (squares[x][y].mExterior) {
                int dx = window->isW() ? 1 : 0;
                int dy = window->isN() ? 1 : 0;
                squares[x - dx][y - dy].ReplaceCurtains(window, true);
            } else
                squares[x][y].ReplaceCurtains(window, false);
        }
        if (Stairs *stairs = object->asStairs()) {
            // Stair objects are 5 tiles long but only have 3 tiles.
            if (stairs->isN()) {
                for (int i = 1; i <= 3; i++)
                    squares[x][y + i].ReplaceFurniture(stairs->tile(),
                                                       stairs->getOffset(x, y + i));
            } else {
                for (int i = 1; i <= 3; i++)
                    squares[x + i][y].ReplaceFurniture(stairs->tile(),
                                                       stairs->getOffset(x + i, y));
            }
        }
        if (FurnitureObject *fo = object->asFurniture()) {
            FurnitureTile *ftile = fo->furnitureTile()->resolved();
            for (int i = 0; i < ftile->size().height(); i++) {
                for (int j = 0; j < ftile->size().width(); j++) {
                    ReplaceFurniture(x + j, y + i, squares, ftile->tile(j, i));
                }
            }
        }
        if (RoofObject *ro = object->asRoof()) {
            QRect r = ro->bounds();

            QRect se = ro->southEdge();
            switch (ro->depth()) {
            case RoofObject::Point5:
                ReplaceRoofSlope(ro, se, squares, RoofObject::SlopePt5S);
                break;
            case RoofObject::One:
                ReplaceRoofSlope(ro, se, squares, RoofObject::SlopeS1);
                break;
            case RoofObject::OnePoint5:
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-1), squares, RoofObject::SlopeOnePt5S);
                break;
            case RoofObject::Two:
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-1), squares, RoofObject::SlopeS2);
                break;
            case RoofObject::TwoPoint5:
                ReplaceRoofSlope(ro, se.adjusted(0,2,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,-1), squares, RoofObject::SlopeS2);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-2), squares, RoofObject::SlopeTwoPt5S);
                break;
            case RoofObject::Three:
                ReplaceRoofSlope(ro, se.adjusted(0,2,0,0), squares, RoofObject::SlopeS1);
                ReplaceRoofSlope(ro, se.adjusted(0,1,0,-1), squares, RoofObject::SlopeS2);
                ReplaceRoofSlope(ro, se.adjusted(0,0,0,-2), squares, RoofObject::SlopeS3);
                break;
            }

            QRect ee = ro->eastEdge();
            switch (ro->depth()) {
            case RoofObject::Point5:
                ReplaceRoofSlope(ro, ee, squares, RoofObject::SlopePt5E);
                break;
            case RoofObject::One:
                ReplaceRoofSlope(ro, ee, squares, RoofObject::SlopeE1);
                break;
            case RoofObject::OnePoint5:
                ReplaceRoofSlope(ro, ee.adjusted(1,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-1,0), squares, RoofObject::SlopeOnePt5E);
                break;
            case RoofObject::Two:
                ReplaceRoofSlope(ro, ee.adjusted(1,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-1,0), squares, RoofObject::SlopeE2);
                break;
            case RoofObject::TwoPoint5:
                ReplaceRoofSlope(ro, ee.adjusted(2,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(1,0,-1,0), squares, RoofObject::SlopeE2);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-2,0), squares, RoofObject::SlopeTwoPt5E);
                break;
            case RoofObject::Three:
                ReplaceRoofSlope(ro, ee.adjusted(2,0,0,0), squares, RoofObject::SlopeE1);
                ReplaceRoofSlope(ro, ee.adjusted(1,0,-1,0), squares, RoofObject::SlopeE2);
                ReplaceRoofSlope(ro, ee.adjusted(0,0,-2,0), squares, RoofObject::SlopeE3);
                break;
            }

            // Corners
            switch (ro->roofType()) {
            case RoofObject::CornerInnerNW:
                switch (ro->depth()) {
                case RoofObject::Point5:
                    break;
                case RoofObject::One:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Inner1);
                    break;
                case RoofObject::OnePoint5:
                    break;
                case RoofObject::Two:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Inner2);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Inner1);

                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeE2);
                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeS2);
                    break;
                case RoofObject::TwoPoint5:
                    break;
                case RoofObject::Three:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Inner3);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Inner2);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+2, squares, RoofObject::Inner1);

                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeE3);
                    ReplaceRoofCorner(ro, r.left(), r.top()+2, squares, RoofObject::SlopeE3);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+2, squares, RoofObject::SlopeE2);

                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeS3);
                    ReplaceRoofCorner(ro, r.left()+2, r.top(), squares, RoofObject::SlopeS3);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+1, squares, RoofObject::SlopeS2);
                    break;
                }
                break;
            case RoofObject::CornerOuterSE:
                switch (ro->depth()) {
                case RoofObject::Point5:
                    break;
                case RoofObject::One:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Outer1);
                    break;
                case RoofObject::OnePoint5:
                    break;
                case RoofObject::Two:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Outer2);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Outer1);

                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeE1);
                    break;
                case RoofObject::TwoPoint5:
                    break;
                case RoofObject::Three:
                    ReplaceRoofCorner(ro, r.left(), r.top(), squares, RoofObject::Outer3);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+1, squares, RoofObject::Outer2);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+2, squares, RoofObject::Outer1);

                    ReplaceRoofCorner(ro, r.left()+2, r.top(), squares, RoofObject::SlopeE1);
                    ReplaceRoofCorner(ro, r.left()+2, r.top()+1, squares, RoofObject::SlopeE1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top(), squares, RoofObject::SlopeE2);

                    ReplaceRoofCorner(ro, r.left(), r.top()+2, squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left()+1, r.top()+2, squares, RoofObject::SlopeS1);
                    ReplaceRoofCorner(ro, r.left(), r.top()+1, squares, RoofObject::SlopeS2);
                    break;
                }
                break;
            }

            QRect eg = ro->eastGap(RoofObject::Three);
            ReplaceRoofGap(ro, eg, squares, RoofObject::CapGapE3);

            QRect sg = ro->southGap(RoofObject::Three);
            ReplaceRoofGap(ro, sg, squares, RoofObject::CapGapS3);
#if 0
            // SE corner 'pole'
            if (ro->depth() == RoofObject::Three && eg.isValid() && sg.isValid() &&
                    (eg.adjusted(0,0,0,1).bottomLeft() == sg.adjusted(0,0,1,0).topRight()))
                ReplaceRoofCap(ro, r.right()+1, r.bottom()+1, squares, RoofObject::CapGapE3, 3);
#endif

            // Roof tops with depth of 3 are placed in the floor layer of the
            // floor above.
            if (ro->depth() != RoofObject::Three)
                ReplaceRoofTop(ro, ro->flatTop(), squares);

            // East cap
            if (ro->isCappedE()) {
                switch (ro->roofType()) {
                case RoofObject::PeakWE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.right()+1, ro->y(), squares, RoofObject::PeakPt5E);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::PeakOnePt5E);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::TwoPoint5:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::PeakTwoPt5E);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapFallE3);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-2, squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom()-1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.bottom(), squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                case RoofObject::SlopeN:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapFallE1);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapFallE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapFallE3);
                        break;
                    }
                    break;
                case RoofObject::SlopeS:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapRiseE1);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.right()+1, r.top(), squares, RoofObject::CapRiseE3);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+1, squares, RoofObject::CapRiseE2);
                        ReplaceRoofCap(ro, r.right()+1, r.top()+2, squares, RoofObject::CapRiseE1);
                        break;
                    }
                    break;
                }
            }

            // South cap
            if (ro->isCappedS()) {
                switch (ro->roofType()) {
                case RoofObject::PeakNS:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::PeakPt5S);
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::PeakOnePt5S);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+3, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::TwoPoint5:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::PeakTwoPt5S);
                        ReplaceRoofCap(ro, r.right()-1, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.right(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapRiseS3);
                        ReplaceRoofCap(ro, r.left()+3, r.bottom()+1, squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.left()+4, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+5, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                case RoofObject::SlopeW:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapRiseS1);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapRiseS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapRiseS3);
                        break;
                    }
                    break;
                case RoofObject::SlopeE:
                    switch (ro->depth()) {
                    case RoofObject::Point5:
                        break;
                    case RoofObject::One:
                        ReplaceRoofCap(ro, r.left(), r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::OnePoint5:
                        break;
                    case RoofObject::Two:
                        ReplaceRoofCap(ro, r.left()+0, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    case RoofObject::TwoPoint5:
                        break;
                    case RoofObject::Three:
                        ReplaceRoofCap(ro, r.left()+0, r.bottom()+1, squares, RoofObject::CapFallS3);
                        ReplaceRoofCap(ro, r.left()+1, r.bottom()+1, squares, RoofObject::CapFallS2);
                        ReplaceRoofCap(ro, r.left()+2, r.bottom()+1, squares, RoofObject::CapFallS1);
                        break;
                    }
                    break;
                }
            }
        }
    }

    // Place floors
    for (int x = 0; x < width(); x++) {
        for (int y = 0; y < height(); y++) {
            if (mIndexAtPos[x][y] >= 0)
                squares[x][y].mTiles[Square::SectionFloor] = floors[mIndexAtPos[x][y]];
        }
    }

    // Place flat roof tops above roofs on the floor below
    if (BuildingFloor *floorBelow = this->floorBelow()) {
        foreach (BuildingObject *object, floorBelow->objects()) {
            if (RoofObject *ro = object->asRoof()) {
                if (ro->depth() == RoofObject::Three)
                    ReplaceRoofTop(ro, ro->flatTop(), squares);
            }
        }
    }

    // Nuke floors that have stairs on the floor below.
    if (BuildingFloor *floorBelow = this->floorBelow()) {
        foreach (BuildingObject *object, floorBelow->objects()) {
            if (Stairs *stairs = object->asStairs()) {
                int x = stairs->x(), y = stairs->y();
                if (stairs->isW()) {
                    if (x + 1 < 0 || x + 3 >= width() || y < 0 || y >= height())
                        continue;
                    squares[x+1][y].mTiles[Square::SectionFloor] = 0;
                    squares[x+2][y].mTiles[Square::SectionFloor] = 0;
                    squares[x+3][y].mTiles[Square::SectionFloor] = 0;
                }
                if (stairs->isN()) {
                    if (x < 0 || x >= width() || y + 1 < 0 || y + 3 >= height())
                        continue;
                    squares[x][y+1].mTiles[Square::SectionFloor] = 0;
                    squares[x][y+2].mTiles[Square::SectionFloor] = 0;
                    squares[x][y+3].mTiles[Square::SectionFloor] = 0;
                }
            }
        }
    }
}

Door *BuildingFloor::GetDoorAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Door *door = o->asDoor())
            return door;
    }
    return 0;
}

Window *BuildingFloor::GetWindowAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Window *window = o->asWindow())
            return window;
    }
    return 0;
}

Stairs *BuildingFloor::GetStairsAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (Stairs *stairs = o->asStairs())
            return stairs;
    }
    return 0;
}

FurnitureObject *BuildingFloor::GetFurnitureAt(int x, int y)
{
    foreach (BuildingObject *o, mObjects) {
        if (!o->bounds().contains(x, y))
            continue;
        if (FurnitureObject *fo = o->asFurniture())
            return fo;
    }
    return 0;
}

void BuildingFloor::SetRoomAt(int x, int y, Room *room)
{
    mRoomAtPos[x][y] = room;
}

Room *BuildingFloor::GetRoomAt(const QPoint &pos)
{
    return mRoomAtPos[pos.x()][pos.y()];
}

int BuildingFloor::width() const
{
    return mBuilding->width();
}

int BuildingFloor::height() const
{
    return mBuilding->height();
}

QRect BuildingFloor::bounds() const
{
    return mBuilding->bounds();
}

QRegion BuildingFloor::roomRegion(Room *room)
{
    QRegion region;
    for (int y = 0; y < height(); y++) {
    for (int x = 0; x < width(); x++) {
            if (mRoomAtPos[x][y] == room)
                region |= QRegion(x, y, 1, 1);
        }
    }
    return region;
}

QVector<QVector<Room *> > BuildingFloor::resized(const QSize &newSize) const
{
    QVector<QVector<Room *> > grid = mRoomAtPos;
    grid.resize(newSize.width());
    for (int x = 0; x < newSize.width(); x++)
        grid[x].resize(newSize.height());
    return grid;
}

void BuildingFloor::rotate(bool right)
{
    int oldWidth = mRoomAtPos.size();
    int oldHeight = mRoomAtPos[0].size();

    int newWidth = oldWidth, newHeight = oldHeight;
    qSwap(newWidth, newHeight);

    QVector<QVector<Room*> > roomAtPos;
    roomAtPos.resize(newWidth);
    for (int x = 0; x < newWidth; x++) {
        roomAtPos[x].resize(newHeight);
        for (int y = 0; y < newHeight; y++) {
            roomAtPos[x][y] = 0;
        }
    }

    for (int x = 0; x < oldWidth; x++) {
        for (int y = 0; y < oldHeight; y++) {
            Room *room = mRoomAtPos[x][y];
            if (right)
                roomAtPos[oldHeight - y - 1][x] = room;
            else
                roomAtPos[y][oldWidth - x - 1] = room;
        }
    }

    setGrid(roomAtPos);

    foreach (BuildingObject *object, mObjects)
        object->rotate(right);
}

void BuildingFloor::flip(bool horizontal)
{
    if (horizontal) {
        for (int x = 0; x < width() / 2; x++)
            mRoomAtPos[x].swap(mRoomAtPos[width() - x - 1]);
    } else {
        for (int x = 0; x < width(); x++)
            for (int y = 0; y < height() / 2; y++)
                qSwap(mRoomAtPos[x][y], mRoomAtPos[x][height() - y - 1]);
    }

    foreach (BuildingObject *object, mObjects)
        object->flip(horizontal);
}

/////

BuildingFloor::Square::Square() :
    mTiles(MaxSection, 0),
    mTileOffset(MaxSection, 0),
    mExterior(false)
{
    mFurniture[0] = mFurniture[1] = 0;
}


BuildingFloor::Square::~Square()
{
    // mTiles are owned by BuildingTiles
//    for (int i = 0; i < MaxSection; i++)
    //        delete mTiles[i];
}

void BuildingFloor::Square::ReplaceFloor(BuildingTileEntry *tile, int offset)
{
    mTiles[SectionFloor] = tile;
    mTileOffset[SectionFloor] = offset;
}

void BuildingFloor::Square::ReplaceWall(BuildingTileEntry *tile,
                                        Square::WallOrientation orient,
                                        bool exterior)
{
    mTiles[SectionWall] = tile;
    mWallOrientation = orient; // Must set this before getWallOffset() is called
    mTileOffset[SectionWall] = getWallOffset();
    mExterior = exterior;
}

void BuildingFloor::Square::ReplaceDoor(BuildingTileEntry *tile, int offset)
{
    mTiles[SectionDoor] = tile;
    mTileOffset[SectionDoor] = offset;
    mTileOffset[SectionWall] = getWallOffset();
}

void BuildingFloor::Square::ReplaceFrame(BuildingTileEntry *tile, int offset)
{
    mTiles[SectionFrame] = tile;
    mTileOffset[SectionFrame] = offset;
    mTileOffset[SectionWall] = getWallOffset();
}

void BuildingFloor::Square::ReplaceCurtains(Window *window, bool exterior)
{
    mTiles[exterior ? SectionCurtains2 : SectionCurtains] = window->curtainsTile();
    mTileOffset[exterior ? SectionCurtains2 : SectionCurtains] = window->isW()
            ? (exterior ? BTC_Curtains::East : BTC_Curtains::West)
            : (exterior ? BTC_Curtains::South : BTC_Curtains::North);
}

void BuildingFloor::Square::ReplaceFurniture(BuildingTileEntry *tile, int offset)
{
    if (offset < 0) { // see getStairsOffset
        mTiles[SectionFurniture] = 0;
        mTileOffset[SectionFurniture] = 0;
        return;
    }
    if (mTiles[SectionFurniture] && !mTiles[SectionFurniture]->isNone()) {
        mTiles[SectionFurniture2] = tile;
        mTileOffset[SectionFurniture2] = offset;
        return;
    }
    mTiles[SectionFurniture] = tile;
    mTileOffset[SectionFurniture] = offset;
}

void BuildingFloor::Square::ReplaceFurniture(BuildingTile *tile)
{
    if (mFurniture[0] && !mFurniture[0]->isNone()) {
        mFurniture[1] = tile;
        mTileOffset[SectionFurniture2] = 0;
        return;
    }
    mFurniture[0] = tile;
    mTileOffset[SectionFurniture] = 0;
}

void BuildingFloor::Square::ReplaceRoof(BuildingTileEntry *tile, int offset)
{
    if (mTiles[SectionRoof] && !mTiles[SectionRoof]->isNone()) {
        mTiles[SectionRoof2] = tile;
        mTileOffset[SectionRoof2] = offset;
        return;
    }
    mTiles[SectionRoof] = tile;
    mTileOffset[SectionRoof] = offset;
}

void BuildingFloor::Square::ReplaceRoofCap(BuildingTileEntry *tile, int offset)
{
    if (mTiles[SectionRoofCap] && !mTiles[SectionRoofCap]->isNone()) {
        mTiles[SectionRoofCap2] = tile;
        mTileOffset[SectionRoofCap2] = offset;
        return;
    }
    mTiles[SectionRoofCap] = tile;
    mTileOffset[SectionRoofCap] = offset;
}

void BuildingFloor::Square::ReplaceRoofTop(BuildingTileEntry *tile, int offset)
{
    mTiles[SectionRoofTop] = tile;
    mTileOffset[SectionRoofTop] = offset;
}

int BuildingFloor::Square::getWallOffset()
{
    BuildingTileEntry *tile = mTiles[SectionWall];
    if (!tile)
        return -1;

    int offset = BTC_Walls::West;

    switch (mWallOrientation) {
    case WallOrientN:
        if (mTiles[SectionDoor] != 0)
            offset = BTC_Walls::NorthDoor;
        else if (mTiles[SectionFrame] != 0)
            offset = BTC_Walls::NorthWindow;
        else
            offset = BTC_Walls::North;
        break;
    case  WallOrientNW:
        offset = BTC_Walls::NorthWest;
        break;
    case  WallOrientW:
        if (mTiles[SectionDoor] != 0)
            offset = BTC_Walls::WestDoor;
        else if (mTiles[SectionFrame] != 0)
            offset = BTC_Walls::WestWindow;
        break;
    case  WallOrientSE:
        offset = BTC_Walls::SouthEast;
        break;
    }

    return offset;
}
