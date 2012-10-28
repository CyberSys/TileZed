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

#ifndef BUILDINGTILES_H
#define BUILDINGTILES_H

#include <QMap>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Tiled {
class Tile;
class Tileset;
}

namespace BuildingEditor {

class Door;
class Window;
class Stairs;

class BuildingTile
{
public:
    BuildingTile(const QString &tilesetName, int index) :
        mTilesetName(tilesetName),
        mIndex(index)
    {}

    QString name() const;

    QString mTilesetName;
    int mIndex;

    QVector<BuildingTile*> mAlternates;
};

class BuildingTileCategory
{
public:
    BuildingTileCategory(const QString &name, const QString &label) :
        mName(name),
        mLabel(label)
    {}

    BuildingTile *add(const QString &tileName);

    void remove(const QString &tileName);

    BuildingTile *get(const QString &tileName);

    QString name() const
    { return mName; }

    QString label() const
    { return mLabel; }

    const QList<BuildingTile*> &tiles() const
    { return mTiles; }

    BuildingTile *tileAt(int index) const
    { return mTiles.at(index); }

    bool usesTile(Tiled::Tile *tile) const;
    QRect categoryBounds() const;

private:
    QString mName;
    QString mLabel;
    QList<BuildingTile*> mTiles;
    QMap<QString,BuildingTile*> mTileByName;
};

class BuildingTiles : public QObject
{
    Q_OBJECT
public:
    static BuildingTiles *instance();
    static void deleteInstance();

    BuildingTiles();
    ~BuildingTiles();

    BuildingTileCategory *addCategory(const QString &categoryName, const QString &label)
    {
        BuildingTileCategory *category = this->category(categoryName);
        if (!category) {
            category = new BuildingTileCategory(categoryName, label);
            mCategories += category;
            mCategoryByName[categoryName]= category;
        }
        return category;
    }

    BuildingTile *add(const QString &categoryName, const QString &tileName)
    {
        BuildingTileCategory *category = this->category(categoryName);
#if 0
        if (!category) {
            category = new Category(categoryName);
            mCategories += category;
            mCategoryByName[categoryName]= category;
        }
#endif
        return category->add(tileName);
    }

    void add(const QString &categoryName, const QStringList &tileNames)
    {
        QVector<BuildingTile*> tiles;
        foreach (QString tileName, tileNames)
            tiles += add(categoryName, tileName);
        foreach (BuildingTile *tile, tiles)
            tile->mAlternates = tiles;
    }

    BuildingTile *get(const QString &categoryName, const QString &tileName);

    const QList<BuildingTileCategory*> &categories() const
    { return mCategories; }

    BuildingTileCategory *category(const QString &name) const
    {
        if (mCategoryByName.contains(name))
            return mCategoryByName[name];
        return 0;
    }

    static QString nameForTile(const QString &tilesetName, int index);
    static QString nameForTile(Tiled::Tile *tile);
    static bool parseTileName(const QString &tileName, QString &tilesetName, int &index);
    static QString adjustTileNameIndex(const QString &tileName, int offset);
    static QString normalizeTileName(const QString &tileName);

    Tiled::Tile *tileFor(const QString &tileName);
    Tiled::Tile *tileFor(BuildingTile *tile);

    BuildingTile *fromTiledTile(const QString &categoryName, Tiled::Tile *tile);

    void addTileset(Tiled::Tileset *tileset);

    Tiled::Tileset *tilesetFor(const QString &tilesetName)
    { return mTilesetByName[tilesetName]; }

    const QMap<QString,Tiled::Tileset*> &tilesetsMap() const
    { return mTilesetByName; }

    QList<Tiled::Tileset*> tilesets() const
    { return mTilesetByName.values(); }

    void writeBuildingTilesTxt(QWidget *parent = 0);

    BuildingTile *defaultExteriorWall() const;
    BuildingTile *defaultInteriorWall() const;
    BuildingTile *defaultFloorTile() const;
    BuildingTile *defaultDoorTile() const;
    BuildingTile *defaultDoorFrameTile() const;
    BuildingTile *defaultWindowTile() const;
    BuildingTile *defaultStairsTile() const;

    BuildingTile *getExteriorWall(const QString &tileName);
    BuildingTile *getInteriorWall(const QString &tileName);
    BuildingTile *getFloorTile(const QString &tileName);
    BuildingTile *getDoorTile(const QString &tileName);
    BuildingTile *getDoorFrameTile(const QString &tileName);
    BuildingTile *getWindowTile(const QString &tileName);
    BuildingTile *getStairsTile(const QString &tileName);
    BuildingTile *getFurnitureTile(const QString &tileName);

private:
    static BuildingTiles *mInstance;
    QList<BuildingTileCategory*> mCategories;
    QMap<QString,BuildingTileCategory*> mCategoryByName;
    QMap<QString,Tiled::Tileset*> mTilesetByName;
    BuildingTileCategory *mFurnitureCategory;
};

} // namespace BuildingEditor

#endif // BUILDINGTILES_H
