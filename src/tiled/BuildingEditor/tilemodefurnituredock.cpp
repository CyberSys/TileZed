#include "tilemodefurnituredock.h"

#include "buildingfloor.h"
#include "buildingtiles.h"
#include "buildingtiletools.h"
#include "furnituregroups.h"
#include "furnitureview.h"

#include <QAction>
#include <QHBoxLayout>
#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>

using namespace BuildingEditor;

TileModeFurnitureDock::TileModeFurnitureDock(QWidget *parent) :
    QDockWidget(parent),
    mGroupList(new QListWidget(this)),
    mFurnitureView(new FurnitureView(this))
{
    setObjectName(QLatin1String("FurnitureDock"));

    QSplitter *splitter = new QSplitter;
    splitter->addWidget(mGroupList);
    splitter->addWidget(mFurnitureView);
    splitter->setStretchFactor(1, 1);

    QWidget *outer = new QWidget(this);
    QHBoxLayout *outerLayout = new QHBoxLayout(outer);
    outerLayout->setSpacing(5);
    outerLayout->setMargin(5);
    outerLayout->addWidget(splitter);
    setWidget(outer);

    connect(mGroupList, SIGNAL(currentRowChanged(int)), SLOT(currentGroupChanged(int)));
    connect(mFurnitureView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(currentFurnitureChanged()));

    retranslateUi();
}

void TileModeFurnitureDock::switchTo()
{
    setGroupsList();
}

void TileModeFurnitureDock::changeEvent(QEvent *e)
{
    QDockWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi();
        break;
    default:
        break;
    }
}

void TileModeFurnitureDock::retranslateUi()
{
    setWindowTitle(tr("Furniture"));
}

void TileModeFurnitureDock::setGroupsList()
{
    mGroupList->clear();
    foreach (FurnitureGroup *group, FurnitureGroups::instance()->groups())
        mGroupList->addItem(group->mLabel);
}

void TileModeFurnitureDock::setFurnitureList()
{
    QList<FurnitureTiles*> ftiles;
    if (mCurrentGroup) {
        ftiles = mCurrentGroup->mTiles;
    }
    mFurnitureView->model()->setTiles(ftiles);
}

void TileModeFurnitureDock::currentGroupChanged(int row)
{
    mCurrentGroup = 0;
    mCurrentTile = 0;
    if (row >= 0)
        mCurrentGroup = FurnitureGroups::instance()->group(row);
    setFurnitureList();
}

void TileModeFurnitureDock::currentFurnitureChanged()
{
    QModelIndexList indexes = mFurnitureView->selectionModel()->selectedIndexes();
    if (indexes.count() == 1) {
        QModelIndex index = indexes.first();
        if (FurnitureTile *ftile = mFurnitureView->model()->tileAt(index)) {
            ftile = ftile->resolved();
            mCurrentTile = ftile;

            if (ftile->size().isNull())
                return;

            QRegion rgn;
            FloorTileGrid *tiles = new FloorTileGrid(ftile->width(), ftile->height());
            for (int x = 0; x < ftile->width(); x++) {
                for (int y = 0; y < ftile->height(); y++) {
                    if (BuildingTile *btile = ftile->tile(x, y)) {
                        if (!btile->isNone()) {
                            tiles->replace(x, y, btile->name());
                            rgn += QRect(x, y, 1, 1);
                        }
                    }
                }
            }
            if (DrawTileTool::instance()->action()->isEnabled())
                DrawTileTool::instance()->setCaptureTiles(tiles, rgn);
        }
    }
}
