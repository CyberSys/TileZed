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

#include "edgetooldialog.h"
#include "ui_edgetooldialog.h"

#include "documentmanager.h"
#include "edgetool.h"
#include "mainwindow.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>

using namespace Tiled::Internal;

EdgeToolDialog *EdgeToolDialog::mInstance = 0;

EdgeToolDialog *EdgeToolDialog::instance()
{
    if (!mInstance)
        mInstance = new EdgeToolDialog(MainWindow::instance());
    return mInstance;
}

EdgeToolDialog::EdgeToolDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EdgeToolDialog),
    mVisibleLater(true)
{
    ui->setupUi(this);

    setWindowFlags(windowFlags() | Qt::Tool);

    readSettings();
    ui->suppressBlend->setChecked(EdgeTool::instance().suppressBlendTiles());

    connect(ui->edgeList, SIGNAL(currentRowChanged(int)), SLOT(currentRowChanged(int)));
    connect(ui->dashLen, SIGNAL(valueChanged(int)), SLOT(dashChanged()));
    connect(ui->dashGap, SIGNAL(valueChanged(int)), SLOT(dashChanged()));
    connect(ui->suppressBlend, SIGNAL(toggled(bool)), SLOT(suppressChanged(bool)));

    mVisibleLaterTimer.setSingleShot(true);
    mVisibleLaterTimer.setInterval(200);
    connect(&mVisibleLaterTimer, SIGNAL(timeout()), SLOT(setVisibleNow()));

//    readTxt();
}

EdgeToolDialog::~EdgeToolDialog()
{
    delete ui;
}

void EdgeToolDialog::setVisible(bool visible)
{
    if (visible) {
//        readSettings();

        QString fileName(qApp->applicationDirPath() + QLatin1String("/Edges.txt"));
        if (QFileInfo(fileName).exists()) {
            if (mTxtModifiedTime != QFileInfo(fileName).lastModified())
                readTxt();
        }

        currentRowChanged(ui->edgeList->currentRow());
    }

    QDialog::setVisible(visible);

    if (!visible)
        writeSettings();
}

void EdgeToolDialog::setVisibleLater(bool visible)
{
    mVisibleLater = visible;
    mVisibleLaterTimer.start();
}

void EdgeToolDialog::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("EdgeToolDialog"));
    QByteArray geom = settings.value(QLatin1String("geometry")).toByteArray();
    if (!geom.isEmpty())
        restoreGeometry(geom);
    bool suppress = settings.value(QLatin1String("suppress"), EdgeTool::instance().suppressBlendTiles()).toBool();
    suppressChanged(suppress);
    settings.endGroup();
}

void EdgeToolDialog::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("EdgeToolDialog"));
    settings.setValue(QLatin1String("geometry"), saveGeometry());
    settings.setValue(QLatin1String("suppress"), EdgeTool::instance().suppressBlendTiles());
    settings.endGroup();
}

void EdgeToolDialog::currentRowChanged(int row)
{
    Edges *edges = (row >= 0) ? mEdges.at(row) : 0;
    EdgeTool::instance().setEdges(edges);

    if (!edges || edges->mLayer.isEmpty()) return;
    MapDocument *doc = DocumentManager::instance()->currentDocument();
    if (!doc) return;
    int index = doc->map()->indexOfLayer(edges->mLayer);
    if (index >= 0) {
        Layer *layer = doc->map()->layerAt(index);
        if (layer->asTileLayer())
            doc->setCurrentLayerIndex(index);
    }
}

void EdgeToolDialog::dashChanged()
{
    EdgeTool::instance().setDash(ui->dashLen->value(), ui->dashGap->value());
}

void EdgeToolDialog::suppressChanged(bool suppress)
{
    EdgeTool::instance().setSuppressBlendTiles(suppress);
}

void EdgeToolDialog::setVisibleNow()
{
    if (mVisibleLater != isVisible())
        setVisible(mVisibleLater);
}

void EdgeToolDialog::readTxt()
{
    EdgeFile edgeFile;
    QString fileName(qApp->applicationDirPath() + QLatin1String("/Edges.txt"));
    if (QFileInfo(fileName).exists()) {
        if (edgeFile.read(fileName)) {
            mEdges = edgeFile.takeEdges();
            mTxtModifiedTime = QFileInfo(fileName).lastModified();
        } else {
            QMessageBox::warning(MainWindow::instance(), tr("Error Reading Edges.txt"),
                                 edgeFile.errorString());
        }
    }

    ui->edgeList->clear();
    foreach (Edges *edges, mEdges)
        ui->edgeList->addItem(edges->mLabel);

    ui->edgeList->setCurrentRow(mEdges.size() ? 0 : -1);
}
