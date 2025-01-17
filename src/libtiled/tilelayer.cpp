/* 
 * tilelayer.cpp
 * Copyright 2008-2011, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 * Copyright 2009, Jeff Bland <jksb@member.fsf.org>
 *
 * This file is part of libtiled.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tilelayer.h"

#include "layer.h"
#include "map.h"
#include "tile.h"
#include "tileset.h"

using namespace Tiled;

TileLayer::TileLayer(const QString &name, int x, int y, int width, int height):
    Layer(TileLayerType, name, x, y, width, height),
    mMaxTileSize(0, 0),
#ifdef ZOMBOID
    mTileLayerGroup(0),
#endif
#if SPARSE_TILELAYER
    mGrid(width, height)
#else
    mGrid(width * height)
#endif
{
    Q_ASSERT(width >= 0);
    Q_ASSERT(height >= 0);
}

QRegion TileLayer::region() const
{
    QRegion region;

    for (int y = 0; y < mHeight; ++y) {
        for (int x = 0; x < mWidth; ++x) {
            if (!cellAt(x, y).isEmpty()) {
                const int rangeStart = x;
                for (++x; x <= mWidth; ++x) {
                    if (x == mWidth || cellAt(x, y).isEmpty()) {
                        const int rangeEnd = x;
                        region += QRect(rangeStart + mX, y + mY,
                                        rangeEnd - rangeStart, 1);
                        break;
                    }
                }
            }
        }
    }

    return region;
}

static QSize maxSize(const QSize &a,
                     const QSize &b)
{
    return QSize(qMax(a.width(), b.width()),
                 qMax(a.height(), b.height()));
}

static QMargins maxMargins(const QMargins &a,
                           const QMargins &b)
{
    return QMargins(qMax(a.left(), b.left()),
                    qMax(a.top(), b.top()),
                    qMax(a.right(), b.right()),
                    qMax(a.bottom(), b.bottom()));
}

void TileLayer::setCell(int x, int y, const Cell &cell)
{
    Q_ASSERT(contains(x, y));

    if (cell.tile) {
        int width = cell.tile->width();
        int height = cell.tile->height();

        if (cell.flippedAntiDiagonally)
            std::swap(width, height);

        const QPoint offset = cell.tile->tileset()->tileOffset();

        mMaxTileSize = maxSize(QSize(width, height), mMaxTileSize);
        mOffsetMargins = maxMargins(QMargins(-offset.x(),
                                             -offset.y(),
                                             offset.x(),
                                             offset.y()),
                                    mOffsetMargins);

        if (mMap)
            mMap->adjustDrawMargins(drawMargins());
    }

#ifdef ZOMBOID
    if (Tile *tile = cellAt(x, y).tile)
        removeReference(tile->tileset());
    if (cell.tile)
        addReference(cell.tile->tileset());
#endif

#if SPARSE_TILELAYER
    mGrid.replace(x, y, cell);
#else
    mGrid[x + y * mWidth] = cell;
#endif
}

TileLayer *TileLayer::copy(const QRegion &region) const
{
    const QRegion area = region.intersected(QRect(0, 0, width(), height()));
    const QRect bounds = region.boundingRect();
    const QRect areaBounds = area.boundingRect();
    const int offsetX = qMax(0, areaBounds.x() - bounds.x());
    const int offsetY = qMax(0, areaBounds.y() - bounds.y());

    TileLayer *copied = new TileLayer(QString(),
                                      0, 0,
                                      bounds.width(), bounds.height());

    for (const QRect &rect : area)
        for (int x = rect.left(); x <= rect.right(); ++x)
            for (int y = rect.top(); y <= rect.bottom(); ++y)
                copied->setCell(x - areaBounds.x() + offsetX,
                                y - areaBounds.y() + offsetY,
                                cellAt(x, y));

    return copied;
}

void TileLayer::merge(const QPoint &pos, const TileLayer *layer)
{
    // Determine the overlapping area
    QRect area = QRect(pos, QSize(layer->width(), layer->height()));
    area &= QRect(0, 0, width(), height());

    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            const Cell &cell = layer->cellAt(x - area.left(),
                                             y - area.top());
            if (!cell.isEmpty())
                setCell(x, y, cell);
        }
    }
}

void TileLayer::setCells(int x, int y, TileLayer *layer,
                         const QRegion &mask)
{
    // Determine the overlapping area
    QRegion area = QRect(x, y, layer->width(), layer->height());
    area &= QRect(0, 0, width(), height());

    if (!mask.isEmpty())
        area &= mask;

    for (const QRect &rect : area)
        for (int _x = rect.left(); _x <= rect.right(); ++_x)
            for (int _y = rect.top(); _y <= rect.bottom(); ++_y)
                setCell(_x, _y, layer->cellAt(_x - x, _y - y));
}

void TileLayer::erase(const QRegion &area)
{
    const Cell emptyCell;
    for (const QRect &rect : area)
        for (int x = rect.left(); x <= rect.right(); ++x)
            for (int y = rect.top(); y <= rect.bottom(); ++y)
                setCell(x, y, emptyCell);
}

#ifdef ZOMBOID
void TileLayer::erase()
{
#if SPARSE_TILELAYER
    mGrid.clear();
#else
    const Cell emptyCell;
    mGrid.fill(emptyCell);
#endif
    mUsedTilesets.clear();
}
#endif

void TileLayer::flip(FlipDirection direction)
{
#if SPARSE_TILELAYER
    SparseTileGrid newGrid(mWidth, mHeight);

    Q_ASSERT(direction == FlipHorizontally || direction == FlipVertically);

    for (int y = 0; y < mHeight; ++y) {
        for (int x = 0; x < mWidth; ++x) {
            if (direction == FlipHorizontally) {
                Cell source = cellAt(mWidth - x - 1, y);
                source.flippedHorizontally = !source.flippedHorizontally;
                newGrid.replace(x, y, source);
            } else if (direction == FlipVertically) {
                Cell source = cellAt(x, mHeight - y - 1);
                source.flippedVertically = !source.flippedVertically;
                newGrid.replace(x, y, source);
            }
        }
    }
#else
    QVector<Cell> newGrid(mWidth * mHeight);

    Q_ASSERT(direction == FlipHorizontally || direction == FlipVertically);

    for (int y = 0; y < mHeight; ++y) {
        for (int x = 0; x < mWidth; ++x) {
            Cell &dest = newGrid[x + y * mWidth];
            if (direction == FlipHorizontally) {
                const Cell &source = cellAt(mWidth - x - 1, y);
                dest = source;
                dest.flippedHorizontally = !source.flippedHorizontally;
            } else if (direction == FlipVertically) {
                const Cell &source = cellAt(x, mHeight - y - 1);
                dest = source;
                dest.flippedVertically = !source.flippedVertically;
            }
        }
    }
#endif

    mGrid = newGrid;
}

void TileLayer::rotate(RotateDirection direction)
{
    static const char rotateRightMask[8] = { 5, 4, 1, 0, 7, 6, 3, 2 };
    static const char rotateLeftMask[8]  = { 3, 2, 7, 6, 1, 0, 5, 4 };

    const char (&rotateMask)[8] =
            (direction == RotateRight) ? rotateRightMask : rotateLeftMask;

    int newWidth = mHeight;
    int newHeight = mWidth;
#if SPARSE_TILELAYER
    SparseTileGrid newGrid(newWidth, newHeight);
#else
    QVector<Cell> newGrid(newWidth * newHeight);
#endif

    for (int y = 0; y < mHeight; ++y) {
        for (int x = 0; x < mWidth; ++x) {
            const Cell &source = cellAt(x, y);
            Cell dest = source;

            unsigned char mask =
                    (dest.flippedHorizontally << 2) |
                    (dest.flippedVertically << 1) |
                    (dest.flippedAntiDiagonally << 0);

            mask = rotateMask[mask];

            dest.flippedHorizontally = (mask & 4) != 0;
            dest.flippedVertically = (mask & 2) != 0;
            dest.flippedAntiDiagonally = (mask & 1) != 0;

#if SPARSE_TILELAYER
            if (direction == RotateRight)
                newGrid.replace(x * newWidth + (mHeight - y - 1), dest);
            else
                newGrid.replace((mWidth - x - 1) * newWidth + y, dest);
#else
            if (direction == RotateRight)
                newGrid[x * newWidth + (mHeight - y - 1)] = dest;
            else
                newGrid[(mWidth - x - 1) * newWidth + y] = dest;
#endif
        }
    }

    std::swap(mMaxTileSize.rwidth(),
              mMaxTileSize.rheight());

    mWidth = newWidth;
    mHeight = newHeight;
    mGrid = newGrid;
}


QSet<Tileset*> TileLayer::usedTilesets() const
{
#ifdef ZOMBOID
    QList<Tileset*> keys = mUsedTilesets.keys();
    //return { keys.begin(), keys.end() };
    return { keys.toSet() };
#else
    QSet<Tileset*> tilesets;

    for (int i = 0, i_end = mGrid.size(); i < i_end; ++i)
        if (const Tile *tile = mGrid.at(i).tile)
            tilesets.insert(tile->tileset());

    return tilesets;
#endif
}

bool TileLayer::referencesTileset(const Tileset *tileset) const
{
#ifdef ZOMBOID
    Tileset *key = const_cast<Tileset*>(tileset); // hmmm
    return mUsedTilesets.contains(key);
#else
    for (int i = 0, i_end = mGrid.size(); i < i_end; ++i) {
        const Tile *tile = mGrid.at(i).tile;
        if (tile && tile->tileset() == tileset)
            return true;
    }
    return false;
#endif
}

QRegion TileLayer::tilesetReferences(Tileset *tileset) const
{
    QRegion region;

    for (int y = 0; y < mHeight; ++y)
        for (int x = 0; x < mWidth; ++x)
            if (const Tile *tile = cellAt(x, y).tile)
                if (tile->tileset() == tileset)
                    region += QRegion(x + mX, y + mY, 1, 1);

    return region;
}

void TileLayer::removeReferencesToTileset(Tileset *tileset)
{
    for (int i = 0, i_end = mGrid.size(); i < i_end; ++i) {
        const Tile *tile = mGrid.at(i).tile;
#ifdef ZOMBOID
        if (tile && tile->tileset() == tileset) {
            removeReference(tileset);
            mGrid.replace(i, Cell());
        }
#else
        if (tile && tile->tileset() == tileset)
            mGrid.replace(i, Cell());
#endif
    }
}

void TileLayer::replaceReferencesToTileset(Tileset *oldTileset,
                                           Tileset *newTileset)
{
    for (int i = 0, i_end = mGrid.size(); i < i_end; ++i) {
        const Tile *tile = mGrid.at(i).tile;
#ifdef ZOMBOID
        if (tile && tile->tileset() == oldTileset) {
            removeReference(oldTileset);
            addReference(newTileset);
        }
#endif
        if (tile && tile->tileset() == oldTileset)
#if SPARSE_TILELAYER
            mGrid.setTile(i, newTileset->tileAt(tile->id()));
#else
            mGrid[i].tile = newTileset->tileAt(tile->id());
#endif
    }
}

void TileLayer::resize(const QSize &size, const QPoint &offset)
{
#if SPARSE_TILELAYER
    SparseTileGrid newGrid(size.width(), size.height());
#else
    QVector<Cell> newGrid(size.width() * size.height());
#endif

    // Copy over the preserved part
    const int startX = qMax(0, -offset.x());
    const int startY = qMax(0, -offset.y());
    const int endX = qMin(mWidth, size.width() - offset.x());
    const int endY = qMin(mHeight, size.height() - offset.y());

#ifdef ZOMBOID
    mUsedTilesets.clear();
#endif

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            const int index = x + offset.x() + (y + offset.y()) * size.width();
#if SPARSE_TILELAYER
            newGrid.replace(index, cellAt(x, y));
#else
            newGrid[index] = cellAt(x, y);
#endif
#ifdef ZOMBOID
            if (Tile *tile = cellAt(x, y).tile)
                addReference(tile->tileset());
#endif
        }
    }

    mGrid = newGrid;
    Layer::resize(size, offset);
}

void TileLayer::offset(const QPoint &offset,
                       const QRect &bounds,
                       bool wrapX, bool wrapY)
{
#if SPARSE_TILELAYER
    SparseTileGrid newGrid(mWidth, mHeight);
#else
    QVector<Cell> newGrid(mWidth * mHeight);
#endif

#ifdef ZOMBOID
    mUsedTilesets.clear();
#endif

    for (int y = 0; y < mHeight; ++y) {
        for (int x = 0; x < mWidth; ++x) {
            // Skip out of bounds tiles
            if (!bounds.contains(x, y)) {
#if SPARSE_TILELAYER
                newGrid.replace(x, y, cellAt(x, y));
#else
                newGrid[x + y * mWidth] = cellAt(x, y);
#endif
#ifdef ZOMBOID
                if (Tile *tile = cellAt(x, y).tile)
                    addReference(tile->tileset());
#endif
                continue;
            }

            // Get position to pull tile value from
            int oldX = x - offset.x();
            int oldY = y - offset.y();

            // Wrap x value that will be pulled from
            if (wrapX && bounds.width() > 0) {
                while (oldX < bounds.left())
                    oldX += bounds.width();
                while (oldX > bounds.right())
                    oldX -= bounds.width();
            }

            // Wrap y value that will be pulled from
            if (wrapY && bounds.height() > 0) {
                while (oldY < bounds.top())
                    oldY += bounds.height();
                while (oldY > bounds.bottom())
                    oldY -= bounds.height();
            }

            // Set the new tile
#if SPARSE_TILELAYER
            if (contains(oldX, oldY) && bounds.contains(oldX, oldY))
                newGrid.replace(x, y, cellAt(oldX, oldY));
            else
                newGrid.replace(x, y, Cell());
#else
            if (contains(oldX, oldY) && bounds.contains(oldX, oldY))
                newGrid[x + y * mWidth] = cellAt(oldX, oldY);
            else
                newGrid[x + y * mWidth] = Cell();
#endif
#ifdef ZOMBOID
            if (contains(oldX, oldY) && bounds.contains(oldX, oldY)) {
                if (Tile *tile = cellAt(oldX, oldY).tile)
                    addReference(tile->tileset());
            }
#endif
        }
    }

    mGrid = newGrid;
}

bool TileLayer::canMergeWith(Layer *other) const
{
    return dynamic_cast<TileLayer*>(other) != 0;
}

Layer *TileLayer::mergedWith(Layer *other) const
{
    Q_ASSERT(canMergeWith(other));

    const TileLayer *o = static_cast<TileLayer*>(other);
    const QRect unitedBounds = bounds().united(o->bounds());
    const QPoint offset = position() - unitedBounds.topLeft();

    TileLayer *merged = static_cast<TileLayer*>(clone());
    merged->resize(unitedBounds.size(), offset);
    merged->merge(o->position() - unitedBounds.topLeft(), o);
    return merged;
}

QRegion TileLayer::computeDiffRegion(const TileLayer *other) const
{
    QRegion ret;

    const int dx = other->x() - mX;
    const int dy = other->y() - mY;
    QRect r = QRect(0, 0, width(), height());
    r &= QRect(dx, dy, other->width(), other->height());

    for (int y = r.top(); y <= r.bottom(); ++y) {
        for (int x = r.left(); x <= r.right(); ++x) {
            if (cellAt(x, y) != other->cellAt(x - dx, y - dy)) {
                const int rangeStart = x;
                while (x <= r.right() &&
                       cellAt(x, y) != other->cellAt(x - dx, y - dy)) {
                    ++x;
                }
                const int rangeEnd = x;
                ret += QRect(rangeStart, y, rangeEnd - rangeStart, 1);
            }
        }
    }

    return ret;
}

bool TileLayer::isEmpty() const
{
#if SPARSE_TILELAYER
    return mGrid.isEmpty();
#else
    for (int i = 0, i_end = mGrid.size(); i < i_end; ++i)
        if (!mGrid.at(i).isEmpty())
            return false;

    return true;
#endif
}

/**
 * Returns a duplicate of this TileLayer.
 *
 * \sa Layer::clone()
 */
Layer *TileLayer::clone() const
{
    return initializeClone(new TileLayer(mName, mX, mY, mWidth, mHeight));
}

TileLayer *TileLayer::initializeClone(TileLayer *clone) const
{
    Layer::initializeClone(clone);
    clone->mGrid = mGrid;
    clone->mMaxTileSize = mMaxTileSize;
    clone->mOffsetMargins = mOffsetMargins;
#ifdef ZOMBOID
    /* don't clone the group! */
#endif
    return clone;
}
