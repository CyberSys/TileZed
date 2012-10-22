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

#ifndef BUILDINGEDITORWINDOW_H
#define BUILDINGEDITORWINDOW_H

#include <QItemSelection>
#include <QMainWindow>
#include <QMap>
#include <QSettings>
#include <QVector>

class QComboBox;
class QLabel;
class QUndoGroup;

namespace Ui {
class BuildingEditorWindow;
}

namespace Tiled {
class Tile;
class Tileset;
namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BaseTool;
class Building;
class BuildingDocument;
class BuildingFloor;
class BuildingPreviewWindow;
class Door;
class FloorEditor;
class FloorView;
class Room;
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

class BuildingTiles
{
public:
    static BuildingTiles *instance();
    static void deleteInstance();

    class Category
    {
    public:
        Category(const QString &name, const QString &label) :
            mName(name),
            mLabel(label)
        {}

        BuildingTile *add(const QString &tileName)
        {
            QString tilesetName;
            int tileIndex;
            parseTileName(tileName, tilesetName, tileIndex);
            BuildingTile *tile = new BuildingTile(tilesetName, tileIndex);
            Q_ASSERT(!mTileByName.contains(tile->name()));
            mTileByName[tileName] = tile;
            mTiles = mTileByName.values(); // sorted by increasing tileset name and tile index!
            return tile;
        }

        void remove(const QString &tileName)
        {
            if (!mTileByName.contains(tileName))
                return;
            mTileByName.remove(tileName);
            mTiles = mTileByName.values(); // sorted by increasing tileset name and tile index!
        }

        BuildingTile *get(const QString &tileName)
        {
            if (!mTileByName.contains(tileName))
                add(tileName);
            return mTileByName[tileName];
        }

        QString name() const
        { return mName; }

        QString label() const
        { return mLabel; }

        const QList<BuildingTile*> &tiles() const
        { return mTiles; }

        bool usesTile(Tiled::Tile *tile) const;
        QRect categoryBounds() const;

    private:
        QString mName;
        QString mLabel;
        QList<BuildingTile*> mTiles;
        QMap<QString,BuildingTile*> mTileByName;
    };

    ~BuildingTiles();

    Category *addCategory(const QString &categoryName, const QString &label)
    {
        Category *category = this->category(categoryName);
        if (!category) {
            category = new Category(categoryName, label);
            mCategories += category;
            mCategoryByName[categoryName]= category;
        }
        return category;
    }

    BuildingTile *add(const QString &categoryName, const QString &tileName)
    {
        Category *category = this->category(categoryName);
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

    const QList<Category*> &categories() const
    { return mCategories; }

    Category *category(const QString &name)
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

    BuildingTile *tileForDoor(Door *door, const QString &tileName,
                              bool isFrame = false);

    BuildingTile *tileForWindow(Window *window, const QString &tileName);

    BuildingTile *tileForStairs(Stairs *stairs, const QString &tileName);

    void addTileset(Tiled::Tileset *tileset);

    Tiled::Tileset *tilesetFor(const QString &tilesetName)
    { return mTilesetByName[tilesetName]; }

    const QMap<QString,Tiled::Tileset*> &tilesetsMap() const
    { return mTilesetByName; }

    QList<Tiled::Tileset*> tilesets() const
    { return mTilesetByName.values(); }

private:
    static BuildingTiles *mInstance;
    QList<Category*> mCategories;
    QMap<QString,Category*> mCategoryByName;
    QMap<QString,Tiled::Tileset*> mTilesetByName;
};

class BuildingEditorWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit BuildingEditorWindow(QWidget *parent = 0);
    ~BuildingEditorWindow();

    static BuildingEditorWindow *instance;

    void showEvent(QShowEvent *event);
    void closeEvent(QCloseEvent *event);

    bool confirmAllSave();
    bool closeYerself();

    bool Startup();

    bool LoadBuildingTemplates();
    bool LoadBuildingTiles();
    bool LoadMapBaseXMLLots();
    bool validateTile(QString &tileName, const char *key);

    void setCurrentRoom(Room *mRoomComboBox) const; // TODO: move to BuildingDocument
    Room *currentRoom() const;

    BuildingDocument *currentDocument() const
    { return mCurrentDocument; }

    Building *currentBuilding() const;

private:
    void readSettings();
    void writeSettings();

    void updateRoomComboBox();
    void resizeCoordsLabel();

    void setCategoryLists();

private slots:
    void roomIndexChanged(int index);

    void currentEWallChanged(const QItemSelection &selected);
    void currentIWallChanged(const QItemSelection &selected);
    void currentFloorChanged(const QItemSelection &selected);
    void currentDoorChanged(const QItemSelection &selected);
    void currentDoorFrameChanged(const QItemSelection &selected);
    void currentWindowChanged(const QItemSelection &selected);
    void currentStairsChanged(const QItemSelection &selected);

    void upLevel();
    void downLevel();

    void newBuilding();
    void exportTMX();

    void preferences();

    void roomsDialog();
    void roomAdded(Room *room);
    void roomRemoved(Room *room);
    void roomsReordered();
    void roomChanged(Room *room);

    void templatesDialog();
    void tilesDialog();

    void mouseCoordinateChanged(const QPoint &tilePos);

    void updateActions();

private:
    Ui::BuildingEditorWindow *ui;
    BuildingDocument *mCurrentDocument;
    FloorEditor *roomEditor;
    FloorView *mView;
    QComboBox *mRoomComboBox;
    QLabel *mFloorLabel;
    QUndoGroup *mUndoGroup;
    QSettings mSettings;
    QString mError;
    BuildingPreviewWindow *mPreviewWin;
    Tiled::Internal::Zoomable *mZoomable;
    Tiled::Internal::Zoomable *mCategoryZoomable;
};

} // namespace BuildingEditor

#endif // BUILDINGEDITORWINDOW_H
