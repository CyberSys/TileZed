#ifndef BUILDINGTILEMODEWIDGET_H
#define BUILDINGTILEMODEWIDGET_H

#include <QWidget>

class QLabel;
class QListWidgetItem;
class QToolBar;

namespace Ui {
class BuildingTileModeWidget;
}

namespace Tiled {
class Tile;
class Tileset;

namespace Internal {
class Zoomable;
}
}

namespace BuildingEditor {

class BuildingDocument;
class BuildingTileModeView;
class TileModeFurnitureDock;

class BuildingTileModeWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit BuildingTileModeWidget(QWidget *parent = 0);
    ~BuildingTileModeWidget();

    BuildingTileModeView *view();

    void setDocument(BuildingDocument *doc);
    void clearDocument();

    void switchTo();
    void switchAway();

    QToolBar *toolBar() const
    { return mToolBar; }
    
private:
    void setLayersList();

    void setTilesetList();
    void setTilesList();

    void switchLayerForTile(Tiled::Tile *tile);

    void writeSettings();

    typedef Tiled::Tileset Tileset;

private slots:
    void currentLayerChanged(int row);
    void currentTilesetChanged(int row);
    void tileSelectionChanged();

    void opacityChanged(int value);
    void layerItemChanged(QListWidgetItem *item);

    void tilesetAdded(Tiled::Tileset *tileset);
    void tilesetAboutToBeRemoved(Tiled::Tileset *tileset);
    void tilesetChanged(Tileset *tileset);

    void autoSwitchLayerChanged(bool autoSwitch);

    void currentFloorChanged();
    void currentLayerChanged();

    void upLevel();
    void downLevel();

    void tileScaleChanged(qreal scale);

    void updateActions();

private:
    Ui::BuildingTileModeWidget *ui;
    QToolBar *mToolBar;
    TileModeFurnitureDock *mFurnitureDock;
    BuildingDocument *mDocument;
    Tiled::Tileset *mCurrentTileset;
    Tiled::Internal::Zoomable *mZoomable;
    QLabel *mFloorLabel;
    bool mSynching;
    bool mFirstTimeSeen;
};

} // BuildingEditor

#endif // BUILDINGTILEMODEWIDGET_H
