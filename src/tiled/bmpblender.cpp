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

#include "bmpblender.h"

#include "BuildingEditor/buildingfloor.h"
#include "BuildingEditor/simplefile.h"
#include "BuildingEditor/buildingtiles.h"

#include "map.h"
#include "tilelayer.h"
#include "tileset.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSet>
#include <QTextStream>

using namespace Tiled;
using namespace Tiled::Internal;

BmpBlender::BmpBlender(Map *map) :
    mMap(map),
    mRules(map->bmpSettings()->rules()),
    mBlendList(map->bmpSettings()->blends())
{
    fromMap();
}

BmpBlender::~BmpBlender()
{
//    qDeleteAll(mRules);
//    qDeleteAll(mBlendList);
    qDeleteAll(mTileNameGrids);
}

bool BmpBlender::read()
{
#ifdef QT_NO_DEBUG
#ifdef WORLDED
    QString fileName = QApplication::applicationDirPath() + QLatin1String("/Rules.txt");
#else
    QString fileName = QApplication::applicationDirPath() + QLatin1String("/WorldEd/Rules.txt");
#endif
#else
    QString fileName = QLatin1String("C:/Programming/Tiled/PZWorldEd/PZWorldEd/Rules.txt"); // FIXME
#endif
    if (!readRules(fileName))
        return false;

#ifdef QT_NO_DEBUG
#ifdef WORLDED
    fileName = QApplication::applicationDirPath() + QLatin1String("/Blends.txt");
#else
    fileName = QApplication::applicationDirPath() + QLatin1String("/WorldEd/Blends.txt");
#endif
#else
    fileName = QLatin1String("C:/Programming/Tiled/PZWorldEd/PZWorldEd/Blends.txt"); // FIXME
#endif
    if (!readBlends(fileName))
        return false;

    return true;
}

bool BmpBlender::readRules(const QString &filePath)
{
    BmpRulesFile rulesFile;
    if (!rulesFile.read(filePath)) {
        mError = rulesFile.errorString();
        return false;
    }

    qDeleteAll(mRules);
    mRules.clear();
    mRuleByColor.clear();
    mRuleLayers.clear();

    foreach (BmpRule *rule, rulesFile.rules()) {
        mRules += new BmpRule(rule);
        mRuleByColor[rule->color] += mRules.last();
        if (!mRuleLayers.contains(rule->targetLayer))
            mRuleLayers += rule->targetLayer;
    }

    return true;
}

bool BmpBlender::readBlends(const QString &filePath)
{
    BmpBlendsFile blendFile;
    if (!blendFile.read(filePath)) {
        mError = blendFile.errorString();
        return false;
    }

    qDeleteAll(mBlendList);
    mBlendList.clear();
    mBlendsByLayer.clear();

    QSet<QString> layers;
    foreach (BmpBlend *blend, blendFile.blends()) {
        mBlendList += new BmpBlend(blend);
        mBlendsByLayer[blend->targetLayer] += mBlendList.last();
        layers.insert(blend->targetLayer);
    }
    mBlendLayers = layers.values();

    return true;
}

void BmpBlender::recreate()
{
    qDeleteAll(mTileNameGrids);
    mTileNameGrids.clear();

    qDeleteAll(mTileLayers);
    mTileLayers.clear();

    update(0, 0, mMap->width(), mMap->height());
}

void BmpBlender::update(int x1, int y1, int x2, int y2)
{
    imagesToTileNames(x1, y1, x2, y2);
    blend(x1 - 1, y1 - 1, x2 + 1, y2 + 1);
    tileNamesToLayers(x1 - 1, y1 - 1, x2 + 1, y2 + 1);
}

void BmpBlender::fromMap()
{
    mRules = mMap->bmpSettings()->rules();
    mRuleByColor.clear();
    mRuleLayers.clear();
    foreach (BmpRule *rule, mRules) {
        mRuleByColor[rule->color] += rule;
        if (!mRuleLayers.contains(rule->targetLayer))
            mRuleLayers += rule->targetLayer;
    }

    mBlendList = mMap->bmpSettings()->blends();
    mBlendsByLayer.clear();
    mBlendLayers.clear();
    QSet<QString> layers;
    foreach (BmpBlend *blend, mBlendList) {
        mBlendsByLayer[blend->targetLayer] += blend;
        layers.insert(blend->targetLayer);
    }
    mBlendLayers = layers.values();
}

static bool adjacentToNonBlack(const QImage &image1, const QImage &image2, int x1, int y1)
{
    const QRgb black = qRgb(0, 0, 0);
    QRect r(0, 0, image1.width(), image1.height());
    for (int y = y1 - 1; y <= y1 + 1; y++) {
        for (int x = x1 - 1; x <= x1 + 1; x++) {
            if (!r.contains(x, y)) continue;
            if (image1.pixel(x, y) != black || image2.pixel(x, y) != black)
                return true;
        }
    }
    return false;
}

void BmpBlender::imagesToTileNames(int x1, int y1, int x2, int y2)
{
    if (mTileNameGrids.isEmpty()) {
        foreach (QString layerName, mRuleLayers + mBlendLayers) {
            if (!mTileNameGrids.contains(layerName)) {
                mTileNameGrids[layerName]
                        = new BuildingEditor::FloorTileGrid(mMap->width(),
                                                            mMap->height());
            }
        }
    }

    const QRgb black = qRgb(0, 0, 0);

    // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
    // one of the Rules.txt tiles, pretend that that pixel exists in the image.
    x1 -= 1, x2 += 1, y1 -= 1, y2 += 1;
    int index = mMap->indexOfLayer(QLatin1String("0_Floor"), Layer::TileLayerType);
    TileLayer *tl = (index == -1) ? 0 : mMap->layerAt(index)->asTileLayer();
    QList<BmpRule*> floor0Rules;
    foreach (BmpRule *rule, mRules) {
        if (rule->targetLayer == QLatin1String("0_Floor") &&
                rule->bitmapIndex == 0)
            floor0Rules += rule;
    }

    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            foreach (QString layerName, mRuleLayers)
                mTileNameGrids[layerName]->replace(x, y, QString());

            QRgb col = mMap->rbmpMain().pixel(x, y);
            QRgb col2 = mMap->rbmpVeg().pixel(x, y);

            // Hack - If a pixel is black, and the user-drawn map tile in 0_Floor is
            // one of the Rules.txt tiles, pretend that that pixel exists in the image.
            if (tl && col == black && adjacentToNonBlack(mMap->rbmpMain().rimage(),
                                                         mMap->rbmpVeg().rimage(),
                                                         x, y)) {
                if (Tile *tile = tl->cellAt(x, y).tile) {
                    QString tileName = BuildingEditor::BuildingTilesMgr::nameForTile(tile);
                    foreach (BmpRule *rule, floor0Rules) {
                        if (rule->tileChoices.contains(tileName))
                            col = rule->color;
                    }
                }
            }

            if (mRuleByColor.contains(col)) {
                QList<BmpRule*> rules = mRuleByColor[col];

                foreach (BmpRule *rule, rules) {
                    if (rule->bitmapIndex != 0)
                        continue;
                    if (!mTileNameGrids.contains(rule->targetLayer))
                        continue;
                    QString tileName = rule->tileChoices[mMap->bmp(0).rand(x, y) % rule->tileChoices.count()];
                    mTileNameGrids[rule->targetLayer]->replace(x, y, tileName);
                }
            }

            if (col2 != black && mRuleByColor.contains(col2)) {
                QList<BmpRule*> rules = mRuleByColor[col2];

                foreach (BmpRule *rule, rules) {
                    if (rule->bitmapIndex != 1)
                        continue;
                    if (rule->condition != col && rule->condition != black)
                        continue;
                    if (!mTileNameGrids.contains(rule->targetLayer))
                        continue;
                    QString tileName = rule->tileChoices[mMap->bmp(1).rand(x, y) % rule->tileChoices.count()];
                    mTileNameGrids[rule->targetLayer]->replace(x, y, tileName);
                }
            }
        }
    }
}

void BmpBlender::blend(int x1, int y1, int x2, int y2)
{
    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    BuildingEditor::FloorTileGrid *grid
            = mTileNameGrids[QLatin1String("0_Floor")];
    if (!grid)
        return;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            QString tileName = grid->at(x, y);
            foreach (QString layerName, mBlendLayers) {
                BmpBlend *blend = getBlendRule(x, y, tileName, layerName);
                mTileNameGrids[layerName]->replace(x, y, blend ? blend->blendTile : QString());
            }
        }
    }
}

void BmpBlender::tileNamesToLayers(int x1, int y1, int x2, int y2)
{
    bool recreated = false;
    if (mTileLayers.isEmpty()) {
        foreach (QString layerName, mRuleLayers + mBlendLayers) {
            if (!mTileLayers.contains(layerName)) {
                mTileLayers[layerName] = new TileLayer(layerName, 0, 0,
                                                       mMap->width(), mMap->height());
            }
        }
        recreated = true;
    }

    x1 = qBound(0, x1, mMap->width() - 1);
    x2 = qBound(0, x2, mMap->width() - 1);
    y1 = qBound(0, y1, mMap->height() - 1);
    y2 = qBound(0, y2, mMap->height() - 1);

    QMap<QString,Tileset*> tilesets;
    foreach (Tileset *ts, mMap->tilesets())
        tilesets[ts->name()] = ts;

    foreach (QString layerName, mTileLayers.keys()) {
        BuildingEditor::FloorTileGrid *grid = mTileNameGrids[layerName];
        TileLayer *tl = mTileLayers[layerName];
        for (int y = y1; y <= y2; y++) {
            for (int x = x1; x <= x2; x++) {
                QString tileName = grid->at(x, y);
                if (tileName.isEmpty()) {
                    tl->setCell(x, y, Cell(0));
                    continue;
                }
                QString tilesetName;
                int tileID;
                if (BuildingEditor::BuildingTilesMgr::parseTileName(tileName, tilesetName, tileID)) {
                    if (tilesets.contains(tilesetName))
                        tl->setCell(x, y, Cell(tilesets[tilesetName]->tileAt(tileID)));
                }
            }
        }
    }

    if (recreated)
        emit layersRecreated();
}

QString BmpBlender::getNeighbouringTile(int x, int y)
{
    if (x < 0 || y < 0 || x >= mMap->width() || y >= mMap->height())
        return QString();
    BuildingEditor::FloorTileGrid *grid
            = mTileNameGrids[QLatin1String("0_Floor")];
    return grid->at(x, y);
}

BmpBlend *BmpBlender::getBlendRule(int x, int y, const QString &tileName,
                                           const QString &layer)
{
    BmpBlend *lastBlend = 0;
    if (tileName.isEmpty())
        return lastBlend;
    foreach (BmpBlend *blend, mBlendsByLayer[layer]) {
        Q_ASSERT(blend->targetLayer == layer);
        if (blend->targetLayer != layer)
            continue;

        if (tileName != blend->mainTile) {
            if (blend->ExclusionList.contains(tileName))
                continue;
            bool bPass = false;
            switch (blend->dir) {
            case BmpBlend::N:
                bPass = getNeighbouringTile(x, y - 1) == blend->mainTile;
                break;
            case BmpBlend::S:
                bPass = getNeighbouringTile(x, y + 1) == blend->mainTile;
                break;
            case BmpBlend::E:
                bPass = getNeighbouringTile(x + 1, y) == blend->mainTile;
                break;
            case BmpBlend::W:
                bPass = getNeighbouringTile(x - 1, y) == blend->mainTile;
                break;
            case BmpBlend::NE:
                bPass = getNeighbouringTile(x, y - 1) == blend->mainTile &&
                        getNeighbouringTile(x + 1, y) == blend->mainTile;
                break;
            case BmpBlend::SE:
                bPass = getNeighbouringTile(x, y + 1) == blend->mainTile &&
                        getNeighbouringTile(x + 1, y) == blend->mainTile;
                break;
            case BmpBlend::NW:
                bPass = getNeighbouringTile(x, y - 1) == blend->mainTile &&
                        getNeighbouringTile(x - 1, y) == blend->mainTile;
                break;
            case BmpBlend::SW:
                bPass = getNeighbouringTile(x, y + 1) == blend->mainTile &&
                        getNeighbouringTile(x - 1, y) == blend->mainTile;
                break;
            }

            if (bPass)
                lastBlend = blend;
        }
    }

    return lastBlend;
}

/////

BmpRulesFile::BmpRulesFile()
{
}

BmpRulesFile::~BmpRulesFile()
{
    qDeleteAll(mRules);
}

bool BmpRulesFile::read(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        mError = tr("%1\n(while reading %2)").arg(file.errorString())
                .arg(QDir::toNativeSeparators(fileName));
        return false;
    }

    qDeleteAll(mRules);
    mRules.clear();

    QTextStream sr(&file);
    while (!sr.atEnd()) {
        QString line = sr.readLine();

        if (line.contains(QLatin1Char('#')))
            continue;
        if (line.trimmed().isEmpty())
            continue;

        QStringList lineSplit = line.split(QLatin1Char(','));
        int bmp = lineSplit[0].trimmed().toInt();
        QRgb col = qRgb(lineSplit[1].trimmed().toInt(),
                        lineSplit[2].trimmed().toInt(),
                        lineSplit[3].trimmed().toInt());
        QStringList choices = lineSplit[4].split(QLatin1Char(' '));
        int n = 0;
        foreach (QString choice, choices) {
            choices[n] = choice.trimmed();
            if (choices[n] == QLatin1String("null"))
                choices[n].clear();
            n++;
        }
        QRgb con = qRgb(0, 0, 0);

        QString layer = lineSplit[5].trimmed();
        bool hasCon = false;
        if (lineSplit.length() > 6) {
            con = qRgb(lineSplit[6].trimmed().toInt(),
                       lineSplit[7].trimmed().toInt(),
                       lineSplit[8].trimmed().toInt());
            hasCon = true;
        }

        if (hasCon) {
            AddRule(bmp, col, choices, layer, con);
        } else {
            AddRule(bmp, col, choices, layer);
        }
    }

    return true;
}

QList<BmpRule *> BmpRulesFile::rulesCopy() const
{
    QList<BmpRule *> ret;
    foreach (BmpRule *rule, mRules)
        ret += new BmpRule(rule);
    return ret;
}


void BmpRulesFile::AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                         QString layer, QRgb condition)
{
    QStringList normalizedTileNames;
    foreach (QString tileName, tiles)
        normalizedTileNames += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);

    mRules += new BmpRule(bitmapIndex, col, normalizedTileNames, layer, condition);
}

void BmpRulesFile::AddRule(int bitmapIndex, QRgb col, QStringList tiles,
                         QString layer)
{
    QStringList normalizedTileNames;
    foreach (QString tileName, tiles)
        normalizedTileNames += BuildingEditor::BuildingTilesMgr::normalizeTileName(tileName);

    mRules += new BmpRule(bitmapIndex, col, normalizedTileNames, layer);
}

/////

BmpBlendsFile::BmpBlendsFile()
{
}

BmpBlendsFile::~BmpBlendsFile()
{
    qDeleteAll(mBlends);
}

bool BmpBlendsFile::read(const QString &fileName)
{
    qDeleteAll(mBlends);
    mBlends.clear();

    SimpleFile simpleFile;
    if (!simpleFile.read(fileName)) {
        mError = tr("%1\n(while reading %2)")
                .arg(simpleFile.errorString())
                .arg(QDir::toNativeSeparators(fileName));
        return false;
    }

    QMap<QString,BmpBlend::Direction> dirMap;
    dirMap[QLatin1String("n")] = BmpBlend::N;
    dirMap[QLatin1String("s")] = BmpBlend::S;
    dirMap[QLatin1String("e")] = BmpBlend::E;
    dirMap[QLatin1String("w")] = BmpBlend::W;
    dirMap[QLatin1String("nw")] = BmpBlend::NW;
    dirMap[QLatin1String("sw")] = BmpBlend::SW;
    dirMap[QLatin1String("ne")] = BmpBlend::NE;
    dirMap[QLatin1String("se")] = BmpBlend::SE;

    foreach (SimpleFileBlock block, simpleFile.blocks) {
        if (block.name == QLatin1String("blend")) {
            foreach (SimpleFileKeyValue kv, block.values) {
                if (kv.name != QLatin1String("layer") &&
                        kv.name != QLatin1String("mainTile") &&
                        kv.name != QLatin1String("blendTile") &&
                        kv.name != QLatin1String("dir") &&
                        kv.name != QLatin1String("exclude")) {
                    mError = tr("Unknown blend attribute '%1'").arg(kv.name);
                    return false;
                }
            }

            BmpBlend::Direction dir = BmpBlend::Unknown;
            QString dirName = block.value("dir");
            if (dirMap.contains(dirName))
                dir = dirMap[dirName];
            else {
                mError = tr("Unknown blend direction '%1'").arg(dirName);
                return false;
            }

            QStringList excludes;
            foreach (QString exclude, block.value("exclude").split(QLatin1String(" "), QString::SkipEmptyParts))
                excludes += BuildingEditor::BuildingTilesMgr::normalizeTileName(exclude);

            BmpBlend *blend = new BmpBlend(block.value("layer"),
                        BuildingEditor::BuildingTilesMgr::normalizeTileName(block.value("mainTile")),
                        BuildingEditor::BuildingTilesMgr::normalizeTileName(block.value("blendTile")),
                        dir, excludes);
            mBlends += blend;
        } else {
            mError = tr("Unknown block name '%1'.\nProbable syntax error in Blends.txt.").arg(block.name);
            return false;
        }
    }

    return true;
}

QList<BmpBlend *> BmpBlendsFile::blendsCopy() const
{
    QList<BmpBlend *> ret;
    foreach (BmpBlend *blend, mBlends)
        ret += new BmpBlend(blend);
    return ret;
}

/////

