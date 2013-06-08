/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#include "buildinglayersdock.h"
#include "ui_buildinglayersdock.h"

#include "buildingdocument.h"
#include "buildingfloor.h"
#include "buildingmap.h"

using namespace BuildingEditor;

BuildingLayersDock::BuildingLayersDock(QWidget *parent) :
    QDockWidget(parent),
    ui(new Ui::BuildingLayersDock),
    mDocument(0),
    mSynching(false)
{
    ui->setupUi(this);

    connect(ui->opacity, SIGNAL(valueChanged(int)), SLOT(opacityChanged(int)));

    connect(ui->layers, SIGNAL(currentRowChanged(int)),
            SLOT(currentLayerChanged(int)));
    connect(ui->layers, SIGNAL(itemChanged(QListWidgetItem*)),
            SLOT(layerItemChanged(QListWidgetItem*)));

    updateActions();
}

BuildingLayersDock::~BuildingLayersDock()
{
    delete ui;
}

void BuildingLayersDock::setDocument(BuildingDocument *doc)
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = doc;

    if (mDocument) {
        connect(mDocument, SIGNAL(currentFloorChanged()),
                SLOT(currentFloorChanged()));
        connect(mDocument, SIGNAL(currentLayerChanged()),
                SLOT(currentLayerChanged()));
    }

    setLayersList();
    currentLayerChanged();
}

void BuildingLayersDock::clearDocument()
{
    if (mDocument)
        mDocument->disconnect(this);

    mDocument = 0;

    setLayersList();
}

void BuildingLayersDock::setLayersList()
{
    mSynching = true;

    ui->layers->clear();

    if (mDocument) {
        foreach (QString layerName, BuildingMap::layerNames(mDocument->currentLevel())) {
            QListWidgetItem *item = new QListWidgetItem;
            item->setText(layerName);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            bool visible = mDocument->currentFloor()->layerVisibility(layerName);
            item->setCheckState(visible ? Qt::Checked : Qt::Unchecked);
            ui->layers->insertItem(0, item);
        }
    }

    mSynching = false;

    updateActions();
}

void BuildingLayersDock::currentLayerChanged(int row)
{
    if (!mDocument || mSynching)
        return;
    QString layerName;
    if (row >= 0)
        layerName = ui->layers->item(row)->text();
    mDocument->setCurrentLayer(layerName);
}

void BuildingLayersDock::opacityChanged(int value)
{
    if (mSynching || !mDocument)
        return;

    mDocument->setLayerOpacity(mDocument->currentFloor(),
                               mDocument->currentLayer(),
                               qreal(value) / ui->opacity->maximum());
}

void BuildingLayersDock::layerItemChanged(QListWidgetItem *item)
{
    if (mSynching || !mDocument)
        return;

    mDocument->setLayerVisibility(mDocument->currentFloor(),
                                  item->text(),
                                  item->checkState() == Qt::Checked);
}

void BuildingLayersDock::currentFloorChanged()
{
    QString layerName = mDocument->currentLayer();
    setLayersList();

    if (mDocument) {
        if (BuildingMap::layerNames(mDocument->currentLevel()).contains(layerName)) {
            int index = BuildingMap::layerNames(mDocument->currentLevel()).indexOf(layerName);
            int row = ui->layers->count() - index - 1;
            ui->layers->setCurrentRow(row);
        } else {
            ui->layers->setCurrentRow(ui->layers->count() - 1);
        }
    }
    updateActions();
}

void BuildingLayersDock::currentLayerChanged()
{
    if (mDocument) {
        int index = BuildingMap::layerNames(mDocument->currentLevel()).indexOf(mDocument->currentLayer());
        if (index >= 0) {
            int row = ui->layers->count() - index - 1;
            ui->layers->setCurrentRow(row);
        }
    }
    updateActions();
}

void BuildingLayersDock::updateActions()
{
    mSynching = true;

    QString currentLayerName = mDocument ? mDocument->currentLayer() : QString();
    qreal opacity = 1.0f;
    if (mDocument)
        opacity = mDocument->currentFloor()->layerOpacity(currentLayerName);
    ui->opacity->setValue(ui->opacity->maximum() * opacity);
    ui->opacity->setEnabled(!currentLayerName.isEmpty());

    mSynching = false;
}
