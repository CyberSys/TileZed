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

#include "buildingobjects.h"

#include "buildingfloor.h"
#include "buildingtiles.h"
#include "furnituregroups.h"

#include <qmath.h>

using namespace BuildingEditor;

/////

BuildingObject::BuildingObject(BuildingFloor *floor, int x, int y, Direction dir) :
    mFloor(floor),
    mX(x),
    mY(y),
    mDir(dir),
    mTile(0)
{
}

QString BuildingObject::dirString() const
{
    static const char *s[] = { "N", "S", "E", "W" };
    return QLatin1String(s[mDir]);
}

BuildingObject::Direction BuildingObject::dirFromString(const QString &s)
{
    if (s == QLatin1String("N")) return N;
    if (s == QLatin1String("S")) return S;
    if (s == QLatin1String("W")) return W;
    if (s == QLatin1String("E")) return E;
    return Invalid;
}

bool BuildingObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor; // hackery for BaseObjectTool

    // +1 because doors/windows can be on the outside edge of the building
    QRect floorBounds = floor->bounds().adjusted(0, 0, 1, 1);
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void BuildingObject::rotate(bool right)
{
    Q_UNUSED(right)
    mDir = (mDir == N) ? W : N;

    int oldWidth = mFloor->height();
    int oldHeight = mFloor->width();
    if (right) {
        int x = mX;
        mX = oldHeight - mY - 1;
        mY = x;
        if (mDir == N)
            ;
        else
            mX++;
    } else {
        int x = mX;
        mX = mY;
        mY = oldWidth - x - 1;
        if (mDir == N)
            mY++;
        else
            ;
    }
}

void BuildingObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - 1;
        if (mDir == W)
            mX++;
    } else {
        mY = mFloor->height() - mY - 1;
        if (mDir == N)
            mY++;
    }
}

int BuildingObject::index()
{
    return mFloor->indexOf(this);
}

/////

QRect Stairs::bounds() const
{
    if (mDir == N)
        return QRect(mX, mY, 1, 5);
    if (mDir == W)
        return QRect(mX, mY, 5, 1);
    return QRect();
}

void Stairs::rotate(bool right)
{
    BuildingObject::rotate(right);
    if (right) {
        if (mDir == W) // used to be N
            mX -= 5;
    } else {
        if (mDir == N) // used to be W
            mY -= 5;
    }
}

void Stairs::flip(bool horizontal)
{
    BuildingObject::flip(horizontal);
    if (mDir == W && horizontal)
        mX -= 5;
    else if (mDir == N && !horizontal)
        mY -= 5;
}

bool Stairs::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because stairs can't be on the outside edge of the building.
    QRect floorBounds = floor->bounds();
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

int Stairs::getOffset(int x, int y)
{
    if (mDir == N) {
        if (x == mX) {
            int index = y - mY;
            if (index == 1)
                return BTC_Stairs::North3;
            if (index == 2)
                return BTC_Stairs::North2;
            if (index == 3)
                return BTC_Stairs::North1;
        }
    }
    if (mDir == W) {
        if (y == mY) {
            int index = x - mX;
            if (index == 1)
                return BTC_Stairs::West3;
            if (index == 2)
                return BTC_Stairs::West2;
            if (index == 3)
                return BTC_Stairs::West1;
        }
    }
    return -1;
}

FurnitureObject::FurnitureObject(BuildingFloor *floor, int x, int y) :
    BuildingObject(floor, x, y, Invalid),
    mFurnitureTile(0)
{

}

QRect FurnitureObject::bounds() const
{
    return mFurnitureTile ? QRect(pos(), mFurnitureTile->size())
                           : QRect(mX, mY, 1, 1);
}

void FurnitureObject::rotate(bool right)
{
    int oldWidth = mFloor->height();
    int oldHeight = mFloor->width();

    FurnitureTile *oldTile = mFurnitureTile;
    FurnitureTile *newTile = mFurnitureTile;
    if (right) {
        int index = FurnitureTiles::orientIndex(oldTile->mOrient) + 1;
        newTile = oldTile->owner()->mTiles[index % 4];
    } else {
        int index = 4 + FurnitureTiles::orientIndex(oldTile->mOrient) - 1;
        newTile = oldTile->owner()->mTiles[index % 4];
    }

    if (right) {
        int x = mX;
        mX = oldHeight - mY - newTile->size().width();
        mY = x;
    } else {
        int x = mX;
        mX = mY;
        mY = oldWidth - x - newTile->size().height();
    }

    // Stop things going out of bounds, which can happen if the furniture
    // is asymmetric.
    if (mX < 0)
        mX = 0;
    if (mX + newTile->size().width() > mFloor->width())
        mX = mFloor->width() - newTile->size().width();
    if (mY < 0)
        mY = 0;
    if (mY + newTile->size().height() > mFloor->height())
        mY = mFloor->height() - newTile->size().height();

    mFurnitureTile = newTile;
}

void FurnitureObject::flip(bool horizontal)
{
    if (horizontal) {
        int oldWidth = mFurnitureTile->size().width();
        if (mFurnitureTile->isW())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureE];
        else if (mFurnitureTile->isE())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureW];
        else if (mFurnitureTile->isNW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNE);
        else if (mFurnitureTile->isNE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNW);
        else if (mFurnitureTile->isSW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSE);
        else if (mFurnitureTile->isSE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSW);
        mX = mFloor->width() - mX - qMax(oldWidth, mFurnitureTile->size().width());
    } else {
        int oldHeight = mFurnitureTile->size().height();
        if (mFurnitureTile->isN())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureS];
        else if (mFurnitureTile->isS())
            mFurnitureTile = mFurnitureTile->owner()->mTiles[FurnitureTile::FurnitureN];
        else if (mFurnitureTile->isNW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSW);
        else if (mFurnitureTile->isSW())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNW);
        else if (mFurnitureTile->isNE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureSE);
        else if (mFurnitureTile->isSE())
            mFurnitureTile = mFurnitureTile->owner()->tile(FurnitureTile::FurnitureNE);
        mY = mFloor->height() - mY - qMax(oldHeight, mFurnitureTile->size().height());
    }
}

bool FurnitureObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because furniture can't be on the outside edge of the building.
    QRect floorBounds = floor->bounds();
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void FurnitureObject::setFurnitureTile(FurnitureTile *tile)
{
    // FIXME: the object might change size and go out of bounds.
    QSize oldSize = mFurnitureTile ? mFurnitureTile->size() : QSize(0, 0);
    QSize newSize = tile ? tile->size() : QSize(0, 0);
    if (oldSize != newSize) {

    }

    mFurnitureTile = tile;
}

/////

RoofObject::RoofObject(BuildingFloor *floor, int x, int y, int width, int height,
                       RoofType type,
                       bool cappedW, bool cappedN, bool cappedE, bool cappedS) :
    BuildingObject(floor, x, y, BuildingObject::Invalid),
    mWidth(width),
    mHeight(height),
    mType(type),
    mDepth(InvalidDepth),
    mCappedW(cappedW),
    mCappedN(cappedN),
    mCappedE(cappedE),
    mCappedS(cappedS),
    mCapTiles(0),
    mSlopeTiles(0),
    mTopTiles(0)
{
    resize(mWidth, mHeight);
}

QRect RoofObject::bounds() const
{
    return QRect(mX, mY, mWidth, mHeight);
}

void RoofObject::rotate(bool right)
{
    int oldFloorWidth = mFloor->height();
    int oldFloorHeight = mFloor->width();

    qSwap(mWidth, mHeight);

    if (right) {
        int x = mX;
        mX = oldFloorHeight - mY - bounds().width();
        mY = x;

        switch (mType) {
        case SlopeW: mType = SlopeN; break;
        case SlopeN: mType = SlopeE; break;
        case SlopeE: mType = SlopeS; break;
        case SlopeS: mType = SlopeW; break;
        case PeakWE: mType = PeakNS; break;
        case PeakNS: mType = PeakWE; break;
        case FlatTop: break;

        case CornerInnerSW: mType = CornerInnerNW; break;
        case CornerInnerNW: mType = CornerInnerNE; break;
        case CornerInnerNE: mType = CornerInnerSE; break;
        case CornerInnerSE: mType = CornerInnerSW; break;

        case CornerOuterSW: mType = CornerOuterNW; break;
        case CornerOuterNW: mType = CornerOuterNE; break;
        case CornerOuterNE: mType = CornerOuterSE; break;
        case CornerOuterSE: mType = CornerOuterSW; break;
        }

        // Rotate caps
        bool w = mCappedW;
        mCappedW = mCappedS;
        mCappedS = mCappedE;
        mCappedE = mCappedN;
        mCappedN = w;

    } else {
        int x = mX;
        mX = mY;
        mY = oldFloorWidth - x - bounds().height();

        switch (mType) {
        case SlopeW: mType = SlopeS; break;
        case SlopeN: mType = SlopeW; break;
        case SlopeE: mType = SlopeN; break;
        case SlopeS: mType = SlopeE; break;
        case PeakWE: mType = PeakNS; break;
        case PeakNS: mType = PeakWE; break;
        case FlatTop: break;

        case CornerInnerSW: mType = CornerInnerSE; break;
        case CornerInnerNW: mType = CornerInnerSW; break;
        case CornerInnerNE: mType = CornerInnerNW; break;
        case CornerInnerSE: mType = CornerInnerNE; break;

        case CornerOuterSW: mType = CornerOuterSE; break;
        case CornerOuterNW: mType = CornerOuterSW; break;
        case CornerOuterNE: mType = CornerOuterNW; break;
        case CornerOuterSE: mType = CornerOuterNE; break;
        }

        // Rotate caps
        bool w = mCappedW;
        mCappedW = mCappedN;
        mCappedN = mCappedE;
        mCappedE = mCappedS;
        mCappedS = w;
    }
}

void RoofObject::flip(bool horizontal)
{
    if (horizontal) {
        mX = mFloor->width() - mX - bounds().width();

        switch (mType) {
        case SlopeW: mType = SlopeE; break;
        case SlopeN:  break;
        case SlopeE: mType = SlopeW; break;
        case SlopeS:  break;
        case PeakWE:  break;
        case PeakNS:  break;
        case FlatTop: break;

        case CornerInnerSW: mType = CornerInnerSE; break;
        case CornerInnerNW: mType = CornerInnerNE; break;
        case CornerInnerNE: mType = CornerInnerNW; break;
        case CornerInnerSE: mType = CornerInnerSW; break;

        case CornerOuterSW: mType = CornerOuterSE; break;
        case CornerOuterNW: mType = CornerOuterNE; break;
        case CornerOuterNE: mType = CornerOuterNW; break;
        case CornerOuterSE: mType = CornerOuterSW; break;
        }

        qSwap(mCappedW, mCappedE);
    } else {
        mY = mFloor->height() - mY - bounds().height();
        switch (mType) {
        case SlopeW:  break;
        case SlopeN: mType = SlopeS; break;
        case SlopeE:  break;
        case SlopeS: mType = SlopeN; break;
        case PeakWE:  break;
        case PeakNS:  break;
        case FlatTop: break;

        case CornerInnerSW: mType = CornerInnerNW; break;
        case CornerInnerNW: mType = CornerInnerSW; break;
        case CornerInnerNE: mType = CornerInnerSE; break;
        case CornerInnerSE: mType = CornerInnerNE; break;

        case CornerOuterSW: mType = CornerOuterNW; break;
        case CornerOuterNW: mType = CornerOuterSW; break;
        case CornerOuterNE: mType = CornerOuterSE; break;
        case CornerOuterSE: mType = CornerOuterNE; break;
        }

        qSwap(mCappedN, mCappedS);
    }
}

bool RoofObject::isValidPos(const QPoint &offset, BuildingFloor *floor) const
{
    if (!floor)
        floor = mFloor;

    // No +1 because roofs can't be on the outside edge of the building.
    // However, the E or S cap wall tiles can be on the outside edge.
    QRect floorBounds = floor->bounds();
    QRect objectBounds = bounds().translated(offset);
    return (floorBounds & objectBounds) == objectBounds;
}

void RoofObject::setTile(BuildingTileEntry *tile, int alternate)
{
    if ((alternate == 0) && (/*tile->isNone() || */tile->asRoofCap()))
        mCapTiles = tile;
    else if ((alternate == 1) && (/*tile->isNone() || */tile->asRoofSlope()))
        mSlopeTiles = tile;
    else if ((alternate == 2) && (/*tile->isNone() || */tile->asRoofTop()))
        mTopTiles = tile;
}

BuildingTileEntry *RoofObject::tile(int alternate) const
{
    if (alternate == 0) return mCapTiles;
    if (alternate == 1) return mSlopeTiles;
    if (alternate == 2) return mTopTiles;
    return 0;
}

void RoofObject::setCapTiles(BuildingTileEntry *entry)
{
    if (!entry->asRoofCap()) {
        qFatal("wrong type of tiles passed to RoofObject::setCapTiles");
        return;
    }
    mCapTiles = entry;
}

void RoofObject::setSlopeTiles(BuildingTileEntry *entry)
{
    if (!entry->asRoofSlope()) {
        qFatal("wrong type of tiles passed to RoofObject::setSlopeTiles");
        return;
    }
    mSlopeTiles = entry;
}

void RoofObject::setTopTiles(BuildingTileEntry *entry)
{
    if (!entry->asRoofTop()) {
        qFatal("wrong type of tiles passed to RoofObject::setTopTiles");
        return;
    }
    mTopTiles = entry;
}

void RoofObject::setType(RoofObject::RoofType type)
{
    mType = type;
}

void RoofObject::setWidth(int width)
{
    switch (mType) {
    case SlopeW:
    case SlopeE:
        mWidth = qBound(1, width, 3);
        switch (mWidth) {
        case 1:
            mDepth = One;
            break;
        case 2:
            mDepth = Two;
            break;
        case 3:
            mDepth = Three;
            break;
        }
        break;
    case SlopeN:
    case SlopeS:
    case PeakWE:
        mWidth = width;
        break;
    case FlatTop:
        mWidth = width;
        if (mDepth == InvalidDepth)
            mDepth = Three;
        break;
    case PeakNS:
        mWidth = qBound(1, width, 6);
        switch (mWidth) {
        case 1:
            mDepth = Point5;
            break;
        case 2:
            mDepth = One;
            break;
        case 3:
            mDepth = OnePoint5;
            break;
        case 4:
            mDepth = Two;
            break;
        case 5:
            mDepth = TwoPoint5;
            break;
        default:
            mDepth = Three;
            break;
        }
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
    case CornerOuterSW:
    case CornerOuterNW:
    case CornerOuterNE:
    case CornerOuterSE:
        mWidth = qBound(1, width, 3);
        switch (mWidth) {
        case 1:
            mDepth = One;
            break;
        case 2:
            mDepth = Two;
            break;
        case 3:
            mDepth = Three;
            break;
        }
        break;
    }
}

void RoofObject::setHeight(int height)
{
    switch (mType) {
    case SlopeW:
    case SlopeE:
        mHeight = height;
        break;
    case SlopeN:
    case SlopeS:
        mHeight = qBound(1, height, 3);
        switch (mHeight) {
        case 1:
            mDepth = One;
            break;
        case 2:
            mDepth = Two;
            break;
        case 3:
            mDepth = Three;
            break;
        }
        break;
    case PeakWE:
        mHeight = qBound(1, height, 6);
        switch (mHeight) {
        case 1:
            mDepth = Point5;
            break;
        case 2:
            mDepth = One;
            break;
        case 3:
            mDepth = OnePoint5;
            break;
        case 4:
            mDepth = Two;
            break;
        case 5:
            mDepth = TwoPoint5;
            break;
        default:
            mDepth = Three;
            break;
        }
        break;
    case PeakNS:
        mHeight = height;
        break;
    case FlatTop:
        mHeight = height;
        if (mDepth == InvalidDepth)
            mDepth = Three;
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
    case CornerOuterSW:
    case CornerOuterNW:
    case CornerOuterNE:
    case CornerOuterSE:
        mHeight = qBound(1, height, 3);
        switch (mHeight) {
        case 1:
            mDepth = One;
            break;
        case 2:
            mDepth = Two;
            break;
        case 3:
            mDepth = Three;
            break;
        }
        break;
    }
}

void RoofObject::resize(int width, int height)
{
    if (isCorner()) {
        height = width = qMax(width, height);
    }
    setWidth(width);
    setHeight(height);
}

void RoofObject::depthUp()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case Point5: break;
        case One: mDepth = Two; break;
        case OnePoint5: break;
        case Two: mDepth = Three; break;
        case TwoPoint5: break;
        case Three: break;
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case Point5: mDepth = One; break;
        case One: mDepth = OnePoint5; break;
        case OnePoint5: mDepth = Two; break;
        case Two: mDepth = TwoPoint5; break;
        case TwoPoint5: mDepth = Three; break;
        case Three: break;
        }
        break;
    case FlatTop:
#ifdef ROOF_TOPS
        switch (mDepth) {
        case Point5: break;
        case One: mDepth = Two; break;
        case OnePoint5: break;
        case Two: mDepth = Three; break;
        case TwoPoint5: break;
        case Three: break;
        }
#else
        // depth can only be Three
#endif
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case Point5: break;
        case One: mDepth = Two; break;
        case OnePoint5: break;
        case Two: mDepth = Three; break;
        case TwoPoint5: break;
        case Three: break;
        }
        break;
    }
}

void RoofObject::depthDown()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case Point5: break;
        case One: break;
        case OnePoint5: break;
        case Two: mDepth = One; break;
        case TwoPoint5: break;
        case Three: mDepth = Two; break;
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case Point5: break;
        case One: mDepth = Point5; break;
        case OnePoint5: mDepth = One; break;
        case Two: mDepth = OnePoint5; break;
        case TwoPoint5: mDepth = Two; break;
        case Three: mDepth = TwoPoint5; break;
        }
        break;
    case FlatTop:
#ifdef ROOF_TOPS
        switch (mDepth) {
        case Point5: break;
        case One: break;
        case OnePoint5: break;
        case Two: mDepth = One; break;
        case TwoPoint5: break;
        case Three: mDepth = Two; break;
        }
#else
        // depth can only be Three
#endif
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case Point5: break;
        case One: break;
        case OnePoint5: break;
        case Two: mDepth = One; break;
        case TwoPoint5: break;
        case Three: mDepth = Two; break;
        }
        break;
    }
}

bool RoofObject::isDepthMax()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case Point5: break;
        case One: break;
        case OnePoint5: break;
        case Two: break;
        case TwoPoint5: break;
        case Three: return true; ///
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case Point5: break;
        case One: break;
        case OnePoint5: break;
        case Two: break;
        case TwoPoint5: break;
        case Three: return true; ///
        }
        break;
    case FlatTop:
#ifdef ROOF_TOPS
        return mDepth == Three;
#else
        return true; // depth can only be Three
#endif
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case Point5: break;
        case One: break;
        case OnePoint5: break;
        case Two: break;
        case TwoPoint5: break;
        case Three: return true; ///
        }
        break;
    }

    return false;
}

bool RoofObject::isDepthMin()
{
    switch (mType) {
    case SlopeW:
    case SlopeN:
    case SlopeE:
    case SlopeS:
        switch (mDepth) {
        case Point5: break;
        case One: return true; ///
        case OnePoint5: break;
        case Two: break;
        case TwoPoint5: break;
        case Three: break;
        }
        break;
    case PeakWE:
    case PeakNS:
        switch (mDepth) {
        case Point5: return true; ///
        case One: break;
        case OnePoint5: break;
        case Two: break;
        case TwoPoint5: break;
        case Three: break;
        }
        break;
    case FlatTop:
#ifdef ROOF_TOPS
        return mDepth == One;
#else
        return true; // depth can only be Three
#endif
        break;
    case CornerInnerSW:
    case CornerInnerNW:
    case CornerInnerNE:
    case CornerInnerSE:
        switch (mDepth) {
        case Point5: break;
        case One: return true; ///
        case OnePoint5: break;
        case Two: break;
        case TwoPoint5: break;
        case Three: break;
        }
        break;
    }

    return false;
}

int RoofObject::actualWidth() const
{
    return mWidth;
}

int RoofObject::actualHeight() const
{
    return mHeight;
}

void RoofObject::toggleCappedW()
{
    mCappedW = !mCappedW;
}

void RoofObject::toggleCappedN()
{
    mCappedN = !mCappedN;
}

void RoofObject::toggleCappedE()
{
    mCappedE = !mCappedE;
}

void RoofObject::toggleCappedS()
{
    mCappedS = !mCappedS;
}

int RoofObject::getOffset(RoofObject::RoofTile tile) const
{
    static const BTC_RoofSlopes::TileEnum mapSlope[] = {
        BTC_RoofSlopes::SlopeS1, BTC_RoofSlopes::SlopeS2, BTC_RoofSlopes::SlopeS3,
        BTC_RoofSlopes::SlopeE1, BTC_RoofSlopes::SlopeE2, BTC_RoofSlopes::SlopeE3,
        BTC_RoofSlopes::SlopePt5S, BTC_RoofSlopes::SlopePt5E,
        BTC_RoofSlopes::SlopeOnePt5S, BTC_RoofSlopes::SlopeOnePt5E,
        BTC_RoofSlopes::SlopeTwoPt5S, BTC_RoofSlopes::SlopeTwoPt5E,
    #if 0
        BTC_RoofSlopes::FlatTopW1, BTC_RoofSlopes::FlatTopW2, BTC_RoofSlopes::FlatTopW3,
        BTC_RoofSlopes::FlatTopN1, BTC_RoofSlopes::FlatTopN2, BTC_RoofSlopes::FlatTopN3,
    #endif
        BTC_RoofSlopes::Inner1, BTC_RoofSlopes::Inner2, BTC_RoofSlopes::Inner3,
        BTC_RoofSlopes::Outer1, BTC_RoofSlopes::Outer2, BTC_RoofSlopes::Outer3
    };

    static const BTC_RoofCaps::TileEnum mapCap[] = {
        BTC_RoofCaps::CapRiseE1, BTC_RoofCaps::CapRiseE2, BTC_RoofCaps::CapRiseE3,
        BTC_RoofCaps::CapFallE1, BTC_RoofCaps::CapFallE2, BTC_RoofCaps::CapFallE3,
        BTC_RoofCaps::CapRiseS1, BTC_RoofCaps::CapRiseS2, BTC_RoofCaps::CapRiseS3,
        BTC_RoofCaps::CapFallS1, BTC_RoofCaps::CapFallS2, BTC_RoofCaps::CapFallS3,
        BTC_RoofCaps::PeakPt5S, BTC_RoofCaps::PeakPt5E,
        BTC_RoofCaps::PeakOnePt5S, BTC_RoofCaps::PeakOnePt5E,
        BTC_RoofCaps::PeakTwoPt5S, BTC_RoofCaps::PeakTwoPt5E,
        BTC_RoofCaps::CapGapS1, BTC_RoofCaps::CapGapS2, BTC_RoofCaps::CapGapS3,
        BTC_RoofCaps::CapGapE1, BTC_RoofCaps::CapGapE2, BTC_RoofCaps::CapGapE3
    };

    if (tile >= CapRiseE1)
        return mapCap[tile - CapRiseE1];

    return mapSlope[tile];
}

QRect RoofObject::westEdge()
{
    QRect r = bounds();
    if (mType == SlopeW)
        return QRect(r.left(), r.top(),
                     actualWidth(), r.height());
    if (mType == PeakNS) {
        int slopeThickness = qCeil(qreal(mWidth) / 2);
        return QRect(r.left(), r.top(),
                     slopeThickness, r.height());
    }
    return QRect();
}

QRect RoofObject::northEdge()
{
    QRect r = bounds();
    if (mType == SlopeN)
        return QRect(r.left(), r.top(),
                     r.width(), actualHeight());
    if (mType == PeakWE) {
        int slopeThickness = qCeil(qreal(mHeight) / 2);
        return QRect(r.left(), r.top(),
                     r.width(), slopeThickness);
    }
    return QRect();
}

QRect RoofObject::eastEdge()
{
    QRect r = bounds();
    if (mType == SlopeE || mType == CornerInnerSW/*hack*/)
        return QRect(r.right() - actualWidth() + 1, r.top(),
                     actualWidth(), r.height());
    if (mType == PeakNS) {
        int slopeThickness = qCeil(qreal(mWidth) / 2);
        return QRect(r.right() - slopeThickness + 1, r.top(),
                     slopeThickness, r.height());
    }
    return QRect();
}

QRect RoofObject::southEdge()
{
    QRect r = bounds();
    if (mType == SlopeS || mType == CornerInnerNE/*hack*/)
        return QRect(r.left(), r.bottom() - actualHeight() + 1,
                     r.width(), actualHeight());
    if (mType == PeakWE) {
        int slopeThickness = qCeil(qreal(mHeight) / 2);
        return QRect(r.left(), r.bottom() - slopeThickness + 1,
                     r.width(), slopeThickness);
    }
    return QRect();
}

QRect RoofObject::eastGap(RoofDepth depth)
{
    if (depth != mDepth || !mCappedE)
        return QRect();
    QRect r = bounds();
    if (mType == SlopeW || mType == FlatTop
            || mType == CornerInnerSE
            || mType == CornerInnerNE) {
        return QRect(r.right() + 1, r.top(), 1, r.height());
    }
    return QRect();
}

QRect RoofObject::southGap(RoofDepth depth)
{
    if (depth != mDepth || !mCappedS)
        return QRect();
    QRect r = bounds();
    if (mType == SlopeN || mType == FlatTop
            || mType == CornerInnerSE
            || mType == CornerInnerSW) {
        return QRect(r.left(), r.bottom() + 1, r.width(), 1);
    }
    return QRect();
}

QRect RoofObject::flatTop()
{
    QRect r = bounds();
    if (mType == FlatTop)
        return r;
    return QRect();
}

QString RoofObject::typeToString(RoofObject::RoofType type)
{
    switch (type) {
    case SlopeW: return QLatin1String("SlopeW");
    case SlopeN: return QLatin1String("SlopeN");
    case SlopeE: return QLatin1String("SlopeE");
    case SlopeS: return QLatin1String("SlopeS");

    case PeakWE: return QLatin1String("PeakWE");
    case PeakNS: return QLatin1String("PeakNS");

    case FlatTop: return QLatin1String("FlatTop");

    case CornerInnerSW: return QLatin1String("CornerInnerSW");
    case CornerInnerNW: return QLatin1String("CornerInnerNW");
    case CornerInnerNE: return QLatin1String("CornerInnerNE");
    case CornerInnerSE: return QLatin1String("CornerInnerSE");

    case CornerOuterSW: return QLatin1String("CornerOuterSW");
    case CornerOuterNW: return QLatin1String("CornerOuterNW");
    case CornerOuterNE: return QLatin1String("CornerOuterNE");
    case CornerOuterSE: return QLatin1String("CornerOuterSE");
    }

    return QLatin1String("Invalid");
}

RoofObject::RoofType RoofObject::typeFromString(const QString &s)
{
    if (s == QLatin1String("SlopeW")) return SlopeW;
    if (s == QLatin1String("SlopeN")) return SlopeN;
    if (s == QLatin1String("SlopeE")) return SlopeE;
    if (s == QLatin1String("SlopeS")) return SlopeS;

    if (s == QLatin1String("PeakWE")) return PeakWE;
    if (s == QLatin1String("PeakNS")) return PeakNS;

    if (s == QLatin1String("FlatTop")) return FlatTop;

    if (s == QLatin1String("CornerInnerSW")) return CornerInnerSW;
    if (s == QLatin1String("CornerInnerNW")) return CornerInnerNW;
    if (s == QLatin1String("CornerInnerNE")) return CornerInnerNE;
    if (s == QLatin1String("CornerInnerSE")) return CornerInnerSE;

    if (s == QLatin1String("CornerOuterSW")) return CornerOuterSW;
    if (s == QLatin1String("CornerOuterNW")) return CornerOuterNW;
    if (s == QLatin1String("CornerOuterNE")) return CornerOuterNE;
    if (s == QLatin1String("CornerOuterSE")) return CornerOuterSE;

    return InvalidType;
}

QString RoofObject::depthToString(RoofObject::RoofDepth depth)
{
    switch (depth) {
    case Point5: return QLatin1String("Point5");
    case One: return QLatin1String("One");
    case OnePoint5: return QLatin1String("OnePoint5");
    case Two: return QLatin1String("Two");
    case Three: return QLatin1String("Three");
    }

    return QLatin1String("Invalid");
}

RoofObject::RoofDepth RoofObject::depthFromString(const QString &s)
{
    if (s == QLatin1String("Point5")) return Point5;
    if (s == QLatin1String("One")) return One;
    if (s == QLatin1String("OnePoint5")) return OnePoint5;
    if (s == QLatin1String("Two")) return Two;
    if (s == QLatin1String("Three")) return Three;

    return InvalidDepth;
}

/////
