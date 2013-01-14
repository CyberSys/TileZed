/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "tiledeffile.h"

#include "tilesetmanager.h"

#include "tileset.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QImageReader>

using namespace Tiled;
using namespace Tiled::Internal;

TileDefFile::TileDefFile()
{
}

static QString ReadString(QDataStream &in)
{
    QString str;
    quint8 c = ' ';
    while (c != '\n') {
        in >> c;
        if (c != '\n')
            str += QLatin1Char(c);
    }
    return str;
}

bool TileDefFile::read(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("Error opening file for reading.\n%1").arg(fileName);
        return false;
    }

    QDir dir = QFileInfo(fileName).absoluteDir();

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    int numTilesets;
    in >> numTilesets;
    for (int i = 0; i < numTilesets; i++) {
        TileDefTileset *ts = new TileDefTileset;
        ts->mName = ReadString(in);
        ts->mImageSource = ReadString(in);
        qint32 columns, rows;
        in >> columns;
        in >> rows;
        qint32 tileCount;
        in >> tileCount;

        QVector<TileDefTile*> tiles(columns * rows);
        for (int j = 0; j < tileCount; j++) {
            TileDefTile *tile = new TileDefTile;
            qint32 numProperties;
            in >> numProperties;
            for (int k = 0; k < numProperties; k++) {
                QString propertyName = ReadString(in);
                QString propertyValue = ReadString(in);
                tile->mProperties[propertyName] = propertyValue;
            }
            tile->mPropertyUI.FromProperties();
            tiles[j] = tile;
        }

        // Deal with the image being a different size now than it was when the
        // .tiles file was saved.
        QImageReader bmp(dir.filePath(ts->mImageSource));
        ts->mColumns = bmp.size().width() / 64;
        ts->mRows = bmp.size().height() / 128;
        ts->mTiles.resize(ts->mColumns * ts->mRows);
        for (int y = 0; y < qMin(ts->mRows, rows); y++) {
            for (int x = 0; x < qMin(ts->mColumns, columns); x++) {
                ts->mTiles[x + y * ts->mColumns] = tiles[x + y * columns];
            }
        }
        for (int i = 0; i < ts->mTiles.size(); i++) {
            if (!ts->mTiles[i]) {
                ts->mTiles[i] = new TileDefTile;
            }
        }
        mTilesets[ts->mName] = ts;
    }

    mFileName = fileName;

    return true;
}

bool TileDefFile::write(const QString &fileName)
{
    return true;
}

QString TileDefFile::directory() const
{
    return QFileInfo(mFileName).absolutePath();
}

TileDefTileset *TileDefFile::tileset(const QString &name)
{
    if (mTilesets.contains(name))
        return mTilesets[name];
    return 0;
}

/////

TileDefProperties::TileDefProperties()
{
    addBoolean("CollideNorth", "collideN");
    addBoolean("CollideWest", "collideW");

    static const char *DoorStyle[] = { "None", "North", "West", 0 };
    addEnum("Door", "door", DoorStyle);
    addEnum("DoorFrame", "doorFr", DoorStyle);

    addBoolean("IsBed", "bed");
    addBoolean("FloorOverlay");
    addBoolean("IsFloor", "solidfloor");
    addBoolean("IsIndoor", "exterior", true, true);

    static const char *TileBlockStyle[] = {
        "None",
        "Solid",
        "SolidTransparent",
        0
    };
    addEnum("TileBlockStyle", 0, TileBlockStyle);

    static const char *LightPolyStyle[] = {
        "None",
        "WallW",
        "WallN",
        0
    };
    addEnum("LightPolyStyle", 0, LightPolyStyle);

    addString("ContainerType", "container");
    addBoolean("WheelieBin");

    static const char *RoofStyle[] = {
        "None",
        "WestRoofB",
        "WestRoofM",
        "WestRoofT",
        0
    };
    addEnum("RoofStyle", 0, RoofStyle);

    addBoolean("ClimbSheetN", "climbSheetN");
    addBoolean("ClimbSheetW", "climbSheetW");

    static const char *Direction[] = {
        "None",
        "N",
        "NE",
        "E",
        "SE",
        "S",
        "SW",
        "W",
        "NW",
        0
    };
    addEnum("FloorItemShelf", "floor", Direction);
    addEnum("HighItemShelf", "shelf", Direction);
    addEnum("TableItemShelf", "table", Direction);

    static const char *StairStyle[] = {
        "None",
        "BottomW",
        "MiddleW",
        "TopW",
        "BottomN",
        "MiddleN",
        "TopN",
        0
    };
    addEnum("StairStyle", "stairs", StairStyle);

    addBoolean("PreSeen");

    addBoolean("HoppableN");
    addBoolean("HoppableW");
    addBoolean("WallOverlay");
    static const char *WallStyle[] = {
        "None",
        "WestWall",
        "WestWallTrans",
        "WestWindow",
        "WestDoorFrame",
        "NorthWall",
        "NorthWallTrans",
        "NorthWindow",
        "NorthDoorFrame",
        "NorthWestCorner",
        "NorthWestCornerTrans",
        "SouthEastCorner",
        0
    };
    addEnum("WallStyle", 0, WallStyle);

    addInteger("WaterAmount", "waterAmount");
    addInteger("WaterMaxAmount", "waterMaxAmount");
    addBoolean("WaterPiped", "waterPiped");

    addInteger("OpenTileOffset");
    addInteger("SmashedTileOffset");
    addEnum("Window", "window", DoorStyle);
    addBoolean("WindowLocked");
}

TileDefProperties::~TileDefProperties()
{
    qDeleteAll(mProperties);
}

void TileDefProperties::addBoolean(const char *name, const char *shortName,
                                   bool defaultValue, bool reverseLogic)
{
    QLatin1String name2(name);
    QLatin1String shortName2(shortName ? shortName : name);
    mProperties[name2] = new BooleanTileDefProperty(name2, shortName2,
                                                    defaultValue, reverseLogic);
}

void TileDefProperties::addInteger(const char *name, const char *shortName,
                                   int defaultValue)
{
    QLatin1String name2(name);
    QLatin1String shortName2(shortName ? shortName : name);
    mProperties[name2] = new IntegerTileDefProperty(name2, shortName2, defaultValue);
}

void TileDefProperties::addString(const char *name, const char *shortName,
                                  const QString &defaultValue)
{
    QLatin1String name2(name);
    QLatin1String shortName2(shortName ? shortName : name);
    mProperties[name2] = new StringTileDefProperty(name2, shortName2, defaultValue);
}

void TileDefProperties::addEnum(const char *name, const char *shortName,
                                const char *enums[])
{
    QLatin1String name2(name);
    QLatin1String shortName2(shortName ? shortName : name);
    QStringList enums2;
    for (int i = 0; enums[i]; i++)
        enums2 += QLatin1String(enums[i]);
    mProperties[name2] = new EnumTileDefProperty(name2, shortName2, enums2);
}

/////

UIProperties::UIProperties(QMap<QString, QString> &properties)
{
    TileDefProperties props;
    foreach (TileDefProperty *prop, props.mProperties) {
        if (prop->mName == QLatin1String("Door") ||
                prop->mName == QLatin1String("DoorFrame") ||
                prop->mName == QLatin1String("Window")) {
            mProperties[prop->mName] = new PropDoorStyle(prop->mName,
                                                         prop->mShortName,
                                                         properties);
            continue;
        }
        if (prop->mName == QLatin1String("TileBlockStyle")) {
            mProperties[prop->mName] = new PropTileBlockStyle(prop->mName,
                                                              properties);
            continue;
        }
        if (prop->mName == QLatin1String("LightPolyStyle")) {
            mProperties[prop->mName] = new PropLightPolyStyle(prop->mName,
                                                              properties);
            continue;
        }
        if (prop->mName == QLatin1String("RoofStyle")) {
            mProperties[prop->mName] = new PropRoofStyle(prop->mName,
                                                         properties);
            continue;
        }
        if (prop->mName == QLatin1String("StairStyle")) {
            mProperties[prop->mName] = new PropStairStyle(prop->mName,
                                                          prop->mShortName,
                                                          properties);
            continue;
        }
        if (prop->mName.contains(QLatin1String("ItemShelf"))) {
            mProperties[prop->mName] = new PropDirection(prop->mName,
                                                         prop->mShortName,
                                                         properties);
            continue;
        }
        if (prop->mName == QLatin1String("WallStyle")) {
            mProperties[prop->mName] = new PropWallStyle(prop->mName,
                                                         properties);
            continue;
        }
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            mProperties[p->mName] = new PropGenericBoolean(prop->mName,
                                                           p->mShortName,
                                                           properties,
                                                           p->mDefault,
                                                           p->mReverseLogic);
            continue;
        }
        if (IntegerTileDefProperty *p = prop->asInteger()) {
            mProperties[p->mName] = new PropGenericInteger(prop->mName,
                                                           p->mShortName,
                                                           properties,
                                                           p->mDefault);
            continue;
        }
        if (StringTileDefProperty *p = prop->asString()) {
            mProperties[p->mName] = new PropGenericString(prop->mName,
                                                          p->mShortName,
                                                          properties,
                                                          p->mDefault);
            continue;
        }
    }
}
