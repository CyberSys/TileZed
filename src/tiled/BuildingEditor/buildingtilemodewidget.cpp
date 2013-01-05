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

#include "buildingtilemodewidget.h"
#include "ui_buildingtilemodewidget.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingpreferences.h"
#include "buildingtilemodeview.h"
#include "buildingtiles.h"
#include "buildingtiletools.h"

#include "mapcomposite.h"
#include "preferences.h"
#include "tilemetainfomgr.h"
#include "tilesetmanager.h"
#include "zoomable.h"

#include "tilelayer.h"
#include "tileset.h"

#include <QScrollBar>
#include <QToolBar>

using namespace BuildingEditor;
using namespace Tiled;
using namespace Tiled::Internal;

BuildingTileModeWidget::BuildingTileModeWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BuildingTileModeWidget),
    mDocument(0),
    mCurrentTileset(0),
    mZoomable(new Zoomable(this)),
    mFloorLabel(new QLabel),
    mSynching(false),
    mFirstTimeSeen(true)
{
    ui->setupUi(this);

    mToolBar = new QToolBar;
    mToolBar->setObjectName(QString::fromUtf8("ToolBar"));
    mToolBar->addAction(ui->actionPecil);
    mToolBar->addAction(ui->actionSelectTiles);

    mToolBar->addSeparator();
    mFloorLabel->setMinimumWidth(90);
    mFloorLabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    mToolBar->addWidget(mFloorLabel);

    mToolBar->addAction(ui->actionUpLevel);
    mToolBar->addAction(ui->actionDownLevel);

    ui->view->setScene(new BuildingTileModeScene(this));

    mZoomable->setScale(BuildingPreferences::instance()->tileScale());
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);
    connect(mZoomable, SIGNAL(scaleChanged(qreal)),
            BuildingPreferences::instance(), SLOT(setTileScale(qreal)));
    connect(BuildingPreferences::instance(), SIGNAL(tileScaleChanged(qreal)),
            SLOT(tileScaleChanged(qreal)));

    ui->dockLayers->hide();
    ui->dockTilesets->hide();
    dynamic_cast<QMainWindow*>(parent)->addDockWidget(Qt::RightDockWidgetArea,
                                                      ui->dockLayers);
    dynamic_cast<QMainWindow*>(parent)->addDockWidget(Qt::RightDockWidgetArea,
                                                      ui->dockTilesets);

    connect(ui->actionPecil, SIGNAL(triggered()),
            DrawTileTool::instance(), SLOT(makeCurrent()));
    DrawTileTool::instance()->setEditor(view()->scene());
    DrawTileTool::instance()->setAction(ui->actionPecil);

    connect(ui->actionSelectTiles, SIGNAL(triggered()),
            SelectTileTool::instance(), SLOT(makeCurrent()));
    SelectTileTool::instance()->setEditor(view()->scene());
    SelectTileTool::instance()->setAction(ui->actionSelectTiles);

    connect(ui->actionUpLevel, SIGNAL(triggered()), SLOT(upLevel()));
    connect(ui->actionDownLevel, SIGNAL(triggered()), SLOT(downLevel()));

    connect(ui->opacity, SIGNAL(valueChanged(int)), SLOT(opacityChanged(int)));

    connect(ui->layers, SIGNAL(currentRowChanged(int)),
            SLOT(currentLayerChanged(int)));
    connect(ui->layers, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(layerItemChanged(QListWidgetItem*)));

    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));

    ui->tiles->model()->setShowHeaders(false);
    connect(ui->tiles->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));
    connect(Preferences::instance(), SIGNAL(autoSwitchLayerChanged(bool)),
            SLOT(autoSwitchLayerChanged(bool)));

    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAdded(Tiled::Tileset*)),
            SLOT(tilesetAdded(Tiled::Tileset*)));
    connect(TileMetaInfoMgr::instance(), SIGNAL(tilesetAboutToBeRemoved(Tiled::Tileset*)),
            SLOT(tilesetAboutToBeRemoved(Tiled::Tileset*)));

    connect(TilesetManager::instance(), SIGNAL(tilesetChanged(Tileset*)),
            SLOT(tilesetChanged(Tileset*)));

    updateActions();
}

BuildingTileModeWidget::~BuildingTileModeWidget()
{
    writeSettings();
    delete ui;
}

BuildingTileModeView *BuildingTileModeWidget::view()
{
    return ui->view;
}

void BuildingTileModeWidget::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;
    view()->scene()->setDocument(doc);

    if (mDocument) {
        connect(mDocument, SIGNAL(currentFloorChanged()),
                SLOT(currentFloorChanged()));
        connect(mDocument, SIGNAL(currentLayerChanged()),
                SLOT(currentLayerChanged()));

        connect(mDocument, SIGNAL(floorRemoved(BuildingFloor*)),
                SLOT(updateActions())); // floor 1/N
    }

    setLayersList();
    updateActions();

    if (ui->actionPecil->isEnabled() && !TileToolManager::instance()->currentTool())
        DrawTileTool::instance()->makeCurrent();

}

void BuildingTileModeWidget::clearDocument()
{
    if (mDocument)
        mDocument->disconnect(this);
    mDocument = 0;
    view()->scene()->clearDocument();
    setLayersList();
    updateActions();
}

void BuildingTileModeWidget::switchTo()
{
    ui->dockLayers->show(); // FIXME: unless the user hid it
    ui->dockTilesets->show(); // FIXME: unless the user hid it

    if (mFirstTimeSeen) {
        if (!ui->tilesets->count())
            setTilesetList(); // TileMetaInfoMgr signals might have done this already.

        QSettings mSettings;
        mSettings.beginGroup(QLatin1String("BuildingEditor/TileModeWidget"));
        QString tilesetName = mSettings.value(QLatin1String("SelectedTileset")).toString();
        if (!tilesetName.isEmpty()) {
            int index = TileMetaInfoMgr::instance()->indexOf(tilesetName);
            if (index >= 0)
                ui->tilesets->setCurrentRow(index);
        }
        qreal scale = mSettings.value(QLatin1String("EditorScale"), 1.0).toReal();
        view()->zoomable()->setScale(scale);
        mSettings.endGroup();

        mFirstTimeSeen = false;
    }
}

void BuildingTileModeWidget::switchAway()
{
    ui->dockLayers->hide();
    ui->dockTilesets->hide();
}

void BuildingTileModeWidget::setLayersList()
{
    ui->layers->clear();
    if (mDocument) {
        foreach (QString layerName, view()->scene()->layerNames()) {
            QListWidgetItem *item = new QListWidgetItem;
            item->setText(layerName);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Checked);
            ui->layers->insertItem(0, item);
        }
    }
}

void BuildingTileModeWidget::setTilesetList()
{
    ui->tilesets->clear();

    int width = 64;
    QFontMetrics fm = ui->tilesets->fontMetrics();
    foreach (Tileset *tileset, TileMetaInfoMgr::instance()->tilesets()) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setText(tileset->name());
        if (tileset->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
        width = qMax(width, fm.width(tileset->name()));
    }
    int sbw = ui->tilesets->verticalScrollBar()->sizeHint().width();
    ui->tilesets->setFixedWidth(width + 16 + sbw);
}

void BuildingTileModeWidget::setTilesList()
{
    MixedTilesetModel *model = ui->tiles->model();
    model->setShowLabels(Preferences::instance()->autoSwitchLayer());

    if (!mCurrentTileset || mCurrentTileset->isMissing())
        model->setTiles(QList<Tile*>());
    else {
        QStringList labels;
        for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
            Tile *tile = mCurrentTileset->tileAt(i);
            QString label = TilesetManager::instance()->layerName(tile);
            if (label.isEmpty())
                label = tr("???");
            labels += label;
        }
        model->setTileset(mCurrentTileset, labels);
    }
}

void BuildingTileModeWidget::switchLayerForTile(Tiled::Tile *tile)
{
    QString layerName = TilesetManager::instance()->layerName(tile);
    if (!layerName.isEmpty()) {
        if (view()->scene()->layerNames().contains(layerName))
            mDocument->setCurrentLayer(layerName);
    }
}

void BuildingTileModeWidget::writeSettings()
{
    QSettings mSettings;
    mSettings.beginGroup(QLatin1String("BuildingEditor/TileModeWidget"));
    mSettings.setValue(QLatin1String("EditorScale"), view()->zoomable()->scale());
    mSettings.setValue(QLatin1String("SelectedTileset"),
                       mCurrentTileset ? mCurrentTileset->name() : QString());
    mSettings.endGroup();
}

void BuildingTileModeWidget::currentLayerChanged(int row)
{
    if (!mDocument)
        return;
    QString layerName;
    if (row >= 0)
        layerName = ui->layers->item(row)->text();
    mDocument->setCurrentLayer(layerName);
}

void BuildingTileModeWidget::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (row >= 0)
        mCurrentTileset = TileMetaInfoMgr::instance()->tileset(row);
    setTilesList();
}

void BuildingTileModeWidget::tileSelectionChanged()
{
    QModelIndexList selection = ui->tiles->selectionModel()->selectedIndexes();
    if (selection.size()) {
        QModelIndex index = selection.first();
        if (Tile *tile = ui->tiles->model()->tileAt(index)) {
            QString tileName = BuildingTilesMgr::instance()->nameForTile(tile);
            DrawTileTool::instance()->setTile(tileName);

            switchLayerForTile(tile);
        }
    }
    updateActions();
}

void BuildingTileModeWidget::opacityChanged(int value)
{
    if (mSynching || !mDocument)
        return;

    mDocument->setLayerOpacity(mDocument->currentFloor(),
                               mDocument->currentLayer(),
                               qreal(value) / ui->opacity->maximum());
}

void BuildingTileModeWidget::layerItemChanged(QListWidgetItem *item)
{
    if (mSynching || !mDocument)
        return;

    mDocument->setLayerVisibility(mDocument->currentFloor(),
                                  item->text(),
                                  item->checkState() == Qt::Checked);
}

void BuildingTileModeWidget::tilesetAdded(Tileset *tileset)
{
    setTilesetList();
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    ui->tilesets->setCurrentRow(row);
}

void BuildingTileModeWidget::tilesetAboutToBeRemoved(Tileset *tileset)
{
    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    delete ui->tilesets->takeItem(row);
}

// Called when a tileset image changes or a missing tileset was found.
void BuildingTileModeWidget::tilesetChanged(Tileset *tileset)
{
    if (tileset == mCurrentTileset)
        setTilesList();

    int row = TileMetaInfoMgr::instance()->indexOf(tileset);
    if (QListWidgetItem *item = ui->tilesets->item(row))
        item->setForeground(tileset->isMissing() ? Qt::red : Qt::black);
}

void BuildingTileModeWidget::autoSwitchLayerChanged(bool autoSwitch)
{
    ui->tiles->model()->setShowLabels(autoSwitch);
}

void BuildingTileModeWidget::currentFloorChanged()
{
    QString layerName = mDocument->currentLayer();
    setLayersList();
    if (mDocument) {
        if (view()->scene()->layerNames().contains(layerName)) {
            int index = view()->scene()->layerNames().indexOf(layerName);
            int row = ui->layers->count() - index - 1;
            ui->layers->setCurrentRow(row);
        } else {
            ui->layers->setCurrentRow(ui->layers->count() - 1);
        }
    }
    updateActions();
}

void BuildingTileModeWidget::currentLayerChanged()
{
    if (mDocument) {
        int index = view()->scene()->layerNames().indexOf(mDocument->currentLayer());
        if (index >= 0) {
            int row = ui->layers->count() - index - 1;
            ui->layers->setCurrentRow(row);
        }
    }
    updateActions();
}

void BuildingTileModeWidget::upLevel()
{
    if ( mDocument->currentFloorIsTop())
        return;
    int level = mDocument->currentLevel() + 1;
    mDocument->setSelectedObjects(QSet<BuildingObject*>());
    mDocument->setCurrentFloor(mDocument->building()->floor(level));
}

void BuildingTileModeWidget::downLevel()
{
    if ( mDocument->currentFloorIsBottom())
        return;
    int level = mDocument->currentLevel() - 1;
    mDocument->setSelectedObjects(QSet<BuildingObject*>());
    mDocument->setCurrentFloor(mDocument->building()->floor(level));
}

void BuildingTileModeWidget::tileScaleChanged(qreal scale)
{
    mZoomable->setScale(scale);
}

void BuildingTileModeWidget::updateActions()
{
    mSynching = true;

    QString currentLayerName = mDocument ? mDocument->currentLayer() : QString();
    qreal opacity = 1.0f;
    if (mDocument)
        opacity = mDocument->currentFloor()->layerOpacity(currentLayerName);
    ui->opacity->setValue(ui->opacity->maximum() * opacity);
    ui->opacity->setEnabled(!currentLayerName.isEmpty());

    DrawTileTool::instance()->setEnabled(!currentLayerName.isEmpty() &&
            !DrawTileTool::instance()->currentTile().isEmpty());
    SelectTileTool::instance()->setEnabled(!currentLayerName.isEmpty());
    ui->actionUpLevel->setEnabled(mDocument != 0 &&
            !mDocument->currentFloorIsTop());
    ui->actionDownLevel->setEnabled(mDocument != 0 &&
            !mDocument->currentFloorIsBottom());

    if (mDocument)
        mFloorLabel->setText(tr("Floor %1/%2")
                             .arg(mDocument->currentLevel() + 1)
                             .arg(mDocument->building()->floorCount()));
    else
        mFloorLabel->clear();

    mSynching = false;
}
