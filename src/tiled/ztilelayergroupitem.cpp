/*
 * tilelayeritem.cpp
 * Copyright 2008-2009, Thorbjørn Lindeijer <thorbjorn@lindeijer.nl>
 *
 * This file is part of Tiled.
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

#include "ztilelayergroupitem.h"

#include "tilelayer.h"
#include "ztilelayergroup.h"
#include "map.h"
#include "maprenderer.h"

#include <QPainter>
#include <QStyleOptionGraphicsItem>

using namespace Tiled;
using namespace Tiled::Internal;

ZTileLayerGroupItem::ZTileLayerGroupItem(ZTileLayerGroup *layerGroup, MapRenderer *renderer)
    : mLayerGroup(layerGroup)
    , mRenderer(renderer)
{
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption);

    syncWithTileLayers();
}

void ZTileLayerGroupItem::addTileLayer(TileLayer *layer, int index)
{
    mLayerGroup->addTileLayer(layer, index);
    syncWithTileLayers();
}

void ZTileLayerGroupItem::removeTileLayer(TileLayer *layer)
{
    mLayerGroup->removeTileLayer(layer);
    syncWithTileLayers();
}

// The opacity, visibility, or name of a layer has changed.
void ZTileLayerGroupItem::tileLayerChanged(TileLayer *layer)
{
	if (mLayerGroup->mLayers.contains(layer)) {
		update(); // force a redraw at the old bounds
		syncWithTileLayers(); // update the bounds
		update(); // force a redraw at the new bounds
	}
}

bool ZTileLayerGroupItem::ownsTileLayer(TileLayer *layer)
{
	return mLayerGroup->mLayers.contains(layer);
}

void ZTileLayerGroupItem::syncWithTileLayers()
{
    prepareGeometryChange();
    mBoundingRect.setCoords(0,0,0,0);
    foreach (TileLayer *layer, mLayerGroup->mLayers)
        mBoundingRect |= mRenderer->boundingRect(layer->bounds());
}

QRectF ZTileLayerGroupItem::boundingRect() const
{
    return mBoundingRect;
}

void ZTileLayerGroupItem::paint(QPainter *painter,
                          const QStyleOptionGraphicsItem *option,
                          QWidget *)
{
    mRenderer->drawTileLayerGroup(painter, mLayerGroup, option->exposedRect);
}
