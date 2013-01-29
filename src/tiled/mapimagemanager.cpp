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

#include "mapimagemanager.h"

#include "imagelayer.h"
#include "isometricrenderer.h"
#include "mainwindow.h"
#include "map.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "objectgroup.h"
#include "orthogonalrenderer.h"
#include "staggeredrenderer.h"
#include "tilelayer.h"
#include "tilesetmanager.h"
#include "zprogress.h"
#include "zlevelrenderer.h"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>

using namespace Tiled;
using namespace Tiled::Internal;

const int IMAGE_WIDTH = 512;

MapImageManager *MapImageManager::mInstance = NULL;

MapImageManager::MapImageManager() :
    QObject(),
    mDeferralDepth(0),
    mDeferralQueued(false)
{
    mImageReaderThread.resize(10);
    mNextThreadForJob = 0;
    for (int i = 0; i < mImageReaderThread.size(); i++) {
        mImageReaderThread[i] = new MapImageReaderThread;
        connect(mImageReaderThread[i], SIGNAL(imageLoaded(QImage*,MapImage*)),
                SLOT(imageLoaded(QImage*,MapImage*)));
    }

    connect(&mImageRenderThread, SIGNAL(imageRendered(QImage*,MapImage*)),
            SLOT(imageRendered(QImage*,MapImage*)));

    connect(MapManager::instance(), SIGNAL(mapFileChanged(MapInfo*)),
            SLOT(mapFileChanged(MapInfo*)));
}

MapImageManager::~MapImageManager()
{
    qDeleteAll(mImageReaderThread);
}

MapImageManager *MapImageManager::instance()
{
    if (mInstance == NULL)
        mInstance = new MapImageManager;
    return mInstance;
}

void MapImageManager::deleteInstance()
{
    delete mInstance;
}

MapImage *MapImageManager::getMapImage(const QString &mapName, const QString &relativeTo)
{
    // Do not emit mapImageChanged as a result of worker threads finishing
    // loading any images while we are creating a new thumbnail image.
    // Any time QCoreApplication::processEvents() gets called (as is done
    // by MapManager's EditorMapReader class and the PROGRESS class) a
    // worker-thread's signal to us may be processed.
    MapImageManagerDeferral deferral;
    QString mapFilePath = MapManager::instance()->pathForMap(mapName, relativeTo);
    if (mapFilePath.isEmpty())
        return 0;

    if (mMapImages.contains(mapFilePath))
        return mMapImages[mapFilePath];

    ImageData data = generateMapImage(mapFilePath);
    if (!data.valid)
        return 0;

    MapInfo *mapInfo = MapManager::instance()->mapInfo(mapFilePath);
    if (data.threadLoad || data.threadRender)
        paintDummyImage(data, mapInfo);
    MapImage *mapImage = new MapImage(data.image, data.scale, data.levelZeroBounds, mapInfo);
    mapImage->mMissingTilesets = data.missingTilesets;
    mapImage->mLoaded = !(data.threadLoad || data.threadRender);

    if (data.threadLoad || data.threadRender) {
        if (data.threadLoad) {
            QString imageFileName = imageFileInfo(mapFilePath).canonicalFilePath();
            mImageReaderThread[mNextThreadForJob]->addJob(imageFileName, mapImage);
            mNextThreadForJob = (mNextThreadForJob + 1) % mImageReaderThread.size();
        }
        if (data.threadRender) {
            mImageRenderThread.addJob(mapImage);
        }
    }

    // Set up file modification tracking on each TMX that makes
    // up this image.
    QList<MapInfo*> sources;
    foreach (QString source, data.sources)
        if (MapInfo *sourceInfo = MapManager::instance()->mapInfo(source))
            sources += sourceInfo;
    mapImage->setSources(sources);

    mMapImages.insert(mapFilePath, mapImage);
    return mapImage;
}

MapImage *MapImageManager::newFromMap(MapComposite *mapComposite)
{
    ImageData data = generateMapImage(mapComposite);
    Q_ASSERT(data.valid);
    MapImage *mapImage = new MapImage(data.image, data.scale, data.levelZeroBounds, mapComposite->mapInfo());
    return mapImage;
}

MapImageManager::ImageData MapImageManager::generateMapImage(const QString &mapFilePath, bool force)
{
#if 0
    if (mapFilePath == QLatin1String("<fail>")) {
        QImage image(IMAGE_WIDTH, 256, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setFont(QFont(QLatin1String("Helvetica"), 48, 1, true));
        painter.drawText(0, 0, image.width(), image.height(), Qt::AlignCenter, QLatin1String("FAIL"));
        return image;
    }
#endif

    QFileInfo fileInfo(mapFilePath);
    QFileInfo imageInfo = imageFileInfo(mapFilePath);
    QFileInfo imageDataInfo = imageDataFileInfo(imageInfo);
    if (!force && imageInfo.exists() && imageDataInfo.exists() && (fileInfo.lastModified() < imageInfo.lastModified())) {
        QImageReader reader(imageInfo.absoluteFilePath());
        if (!reader.size().isValid())
            QMessageBox::warning(MainWindow::instance(), tr("Error Loading Image"),
                                 tr("An error occurred trying to read a map thumbnail image.\n") + imageInfo.absoluteFilePath());
        if (reader.size().width() == IMAGE_WIDTH) {
            ImageData data = readImageData(imageDataInfo);
            // If the image was originally created with some tilesets missing,
            // try to recreate the image in case those tileset issues were
            // resolved.
            if (data.missingTilesets)
                data.valid = false;
            if (data.valid) {
                foreach (QString source, data.sources) {
                    QFileInfo sourceInfo(source);
                    if (sourceInfo.exists() && (sourceInfo.lastModified() > imageInfo.lastModified())) {
                        data.valid = false;
                        break;
                    }
                }
            }
            if (data.valid) {
                data.threadLoad = true;
                data.size = reader.size();
                return data;
            }
        }
    }

    PROGRESS progress(tr("Generating thumbnail for %1").arg(fileInfo.completeBaseName()));

    MapInfo *mapInfo = MapManager::instance()->loadMap(mapFilePath);
    if (!mapInfo) {
        mError = MapManager::instance()->errorString();
        return ImageData(); // TODO: Add error handling
    }

    progress.update(tr("Generating thumbnail for %1").arg(fileInfo.completeBaseName()));

    MapComposite mapComposite(mapInfo);

    // Wait for TilesetManager's threads to finish loading the tilesets.
    QSet<Tileset*> usedTilesets;
    foreach (MapComposite *mc, mapComposite.maps()) {
        foreach (TileLayer *tl, mc->map()->tileLayers())
            usedTilesets += tl->usedTilesets();
    }
    usedTilesets.remove(TilesetManager::instance()->missingTileset());
    TilesetManager::instance()->waitForTilesets(usedTilesets.toList());

    ImageData data = generateMapImage(&mapComposite);

    foreach (MapComposite *mc, mapComposite.maps()) {
        if (mc->map()->hasUsedMissingTilesets()) {
            data.missingTilesets = true;
            break;
        }
    }
#if 1
#else
    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);
#endif
    return data;
}

MapImageManager::ImageData MapImageManager::generateMapImage(MapComposite *mapComposite)
{
    Map *map = mapComposite->map();

    MapRenderer *renderer = NULL;

    switch (map->orientation()) {
    case Map::Isometric:
        renderer = new IsometricRenderer(map);
        break;
    case Map::LevelIsometric:
        renderer = new ZLevelRenderer(map);
        break;
    case Map::Orthogonal:
        renderer = new OrthogonalRenderer(map);
        break;
    case Map::Staggered:
        renderer = new StaggeredRenderer(map);
        break;
    default:
        return ImageData();
    }

    mapComposite->saveVisibility();
    mapComposite->saveOpacity();
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        foreach (TileLayer *tl, layerGroup->layers()) {
            bool isVisible = true;
            if (tl->name().contains(QLatin1String("NoRender")))
                isVisible = false;
            layerGroup->setLayerVisibility(tl, isVisible);
            layerGroup->setLayerOpacity(tl, 1.0f);
        }
        layerGroup->synch();
    }

    // Don't draw empty levels
    int maxLevel = 0;
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        if (!layerGroup->bounds().isEmpty())
            maxLevel = layerGroup->level();
    }
    renderer->setMaxLevel(maxLevel);

    QRectF sceneRect = mapComposite->boundingRect(renderer);
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return ImageData();

    qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
    mapSize *= scale;

#if 1
#else
    QImage image(mapSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    painter.setRenderHints(QPainter::SmoothPixmapTransform |
                           QPainter::HighQualityAntialiasing);
    painter.setTransform(QTransform::fromScale(scale, scale).translate(-sceneRect.left(), -sceneRect.top()));

    foreach (MapComposite::ZOrderItem zo, mapComposite->zOrder()) {
        if (zo.group) {
            renderer->drawTileLayerGroup(&painter, zo.group);
        } else if (TileLayer *tl = zo.layer->asTileLayer()) {
            if (tl->name().contains(QLatin1String("NoRender")))
                continue;
            renderer->drawTileLayer(&painter, tl);
        }
    }
#endif

    mapComposite->restoreVisibility();
    mapComposite->restoreOpacity();
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups())
        layerGroup->synch();

    ImageData data;
#if 1
    data.threadRender = true;
    data.size = mapSize;
#else
    data.image = image;
#endif
    data.scale = scale;
    data.levelZeroBounds = renderer->boundingRect(QRect(0, 0, map->width(), map->height()));
    data.levelZeroBounds.translate(-sceneRect.topLeft());
    data.sources = mapComposite->getMapFileNames();
    data.valid = true;

    delete renderer;

    return data;
}

void MapImageManager::paintDummyImage(ImageData &data, MapInfo *mapInfo)
{
    Q_ASSERT(data.size.isValid());
    data.image = QImage(data.size, QImage::Format_ARGB32);
//        data.image.setColorTable(QVector<QRgb>() << qRgb(255,255,255));
    QPainter p(&data.image);
    QPolygonF poly;
    MapImage mapImage(data.image, data.scale, data.levelZeroBounds, mapInfo);
    poly += mapImage.tileToImageCoords(0, 0);
    poly += mapImage.tileToImageCoords(mapInfo->width(), 0);
    poly += mapImage.tileToImageCoords(mapInfo->width(), mapInfo->height());
    poly += mapImage.tileToImageCoords(0, mapInfo->height());
    poly += poly.first();
    QPainterPath path;
    path.addPolygon(poly);
    data.image.fill(QColor(Qt::transparent));
    p.fillPath(path, QColor(100,100,100));
}

#define IMAGE_DATA_MAGIC 0xB15B00B5
#define IMAGE_DATA_VERSION 3

MapImageManager::ImageData MapImageManager::readImageData(const QFileInfo &imageDataFileInfo)
{
    ImageData data;
    QFile file(imageDataFileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return data;

    QDataStream in(&file);

    quint32 magic;
    in >> magic;
    if (magic != IMAGE_DATA_MAGIC)
        return data;

    quint32 version;
    in >> version;
    if (version != IMAGE_DATA_VERSION)
        return data;

    in >> data.scale;

    qreal x, y, w, h;
    in >> x >> y >> w >> h;
    data.levelZeroBounds.setCoords(x, y, x + w, y + h);

    qint32 count;
    in >> count;
    for (int i = 0; i < count; i++) {
        QString source;
        in >> source;
        data.sources += source;
    }

    in >> data.missingTilesets;

    // TODO: sanity-check the values
    data.valid = true;

    return data;
}

void MapImageManager::writeImageData(const QFileInfo &imageDataFileInfo, const MapImageManager::ImageData &data)
{
    QFile file(imageDataFileInfo.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << quint32(IMAGE_DATA_MAGIC);
    out << quint32(IMAGE_DATA_VERSION);
    out.setVersion(QDataStream::Qt_4_0);
    out << data.scale;
    QRectF r = data.levelZeroBounds;
    out << r.x() << r.y() << r.width() << r.height();
    out << qint32(data.sources.length());
    foreach (QString source, data.sources)
        out << source;
    out << data.missingTilesets;
}

void MapImageManager::mapFileChanged(MapInfo *mapInfo)
{
    QMap<QString,MapImage*>::iterator it_begin = mMapImages.begin();
    QMap<QString,MapImage*>::iterator it_end = mMapImages.end();
    QMap<QString,MapImage*>::iterator it;

    MapImageManagerDeferral deferral;

    for (it = it_begin; it != it_end; it++) {
        MapImage *mapImage = it.value();
        if (mapImage->sources().contains(mapInfo)) {
            // When tilesets are added or removed from TileMetaInfoMgr,
            // MapManager generates the mapFileChanged signal for .tbx lots.
            // The generateMapImage() call below would then return the same
            // data for the image that was already loaded with image.size=0
            // to indicate we should load the image in a worker thread.
            // So force the image to be recreated in every case.
            bool force = true;
            ImageData data = generateMapImage(mapImage->mapInfo()->path(), force);
            if (!data.valid) {
                // We had a valid image, but now it's bogus, so blank it out.
                data.image = mapImage->image();
                data.image.fill(Qt::white);
                data.scale = mapImage->scale();
                data.levelZeroBounds = mapImage->levelZeroBounds();
                data.sources.clear();
                foreach (MapInfo *sourceInfo, mapImage->sources())
                    data.sources += sourceInfo->path();
            }
            if (/*data.threadLoad ||*/ data.threadRender)
                paintDummyImage(data, mapInfo);
            mapImage->mapFileChanged(data.image, data.scale,
                                     data.levelZeroBounds);

            // Set up file modification tracking on each TMX that makes
            // up this image.
            QList<MapInfo*> sources;
            foreach (QString source, data.sources)
                if (MapInfo *sourceInfo = MapManager::instance()->mapInfo(source))
                    sources += sourceInfo;
            mapImage->setSources(sources);

            emit mapImageChanged(mapImage);
        }
    }
}

void MapImageManager::imageLoaded(QImage *image, MapImage *mapImage)
{
    mapImage->setImage(*image);
    mapImage->mLoaded = true;
    delete image;

    if (mDeferralDepth > 0)
        mDeferredMapImages += mapImage;
    else
        emit mapImageChanged(mapImage);
}

void MapImageManager::imageRendered(QImage *image, MapImage *mapImage)
{
    mapImage->setImage(*image);
    mapImage->mLoaded = true;
    delete image;

    QFileInfo imageInfo = imageFileInfo(mapImage->mapInfo()->path());
    QFileInfo imageDataInfo = imageDataFileInfo(imageInfo);

    ImageData data;
    data.image = mapImage->image();
    data.levelZeroBounds = mapImage->levelZeroBounds();
    data.scale = mapImage->scale();
    foreach (MapInfo *mapInfo, mapImage->sources())
        data.sources += mapInfo->path();
    data.missingTilesets = mapImage->isMissingTilesets();

    data.image.save(imageInfo.absoluteFilePath());
    writeImageData(imageDataInfo, data);

    mImageRenderThread.cleanupDoneJobs();

    if (mDeferralDepth > 0)
        mDeferredMapImages += mapImage;
    else
        emit mapImageChanged(mapImage);
}

QFileInfo MapImageManager::imageFileInfo(const QString &mapFilePath)
{
    QFileInfo mapFileInfo(mapFilePath);
    QDir mapDir = mapFileInfo.absoluteDir();
    if (!mapDir.exists())
        return QFileInfo();
    QFileInfo imagesDirInfo(mapDir, QLatin1String(".pzeditor"));
    if (!imagesDirInfo.exists()) {
        if (!mapDir.mkdir(QLatin1String(".pzeditor")))
            return QFileInfo();
    }

    // Need to distinguish BMPToTMX image formats, so include .png or .bmp
    // in the file name.
    QString suffix;
    if (mapFileInfo.suffix() != QLatin1String("tmx"))
        suffix = QLatin1String("_") + mapFileInfo.suffix();

    return QFileInfo(imagesDirInfo.absoluteFilePath() + QLatin1Char('/') +
                     mapFileInfo.completeBaseName() + suffix + QLatin1String(".png"));
}

QFileInfo MapImageManager::imageDataFileInfo(const QFileInfo &imageFileInfo)
{
    return QFileInfo(imageFileInfo.absolutePath() + QLatin1Char('/') +
                     imageFileInfo.completeBaseName() + QLatin1String(".dat"));
}

void MapImageManager::deferThreadResults(bool defer)
{
    if (defer) {
        ++mDeferralDepth;
//        qDebug() << "MapImageManager::deferThreadResults depth++ =" << mDeferralDepth;
    } else {
        Q_ASSERT(mDeferralDepth > 0);
//        qDebug() << "MapImageManager::deferThreadResults depth-- =" << mDeferralDepth - 1;
        if (--mDeferralDepth == 0) {
            if (!mDeferralQueued && mDeferredMapImages.size()) {
                QMetaObject::invokeMethod(this, "processDeferrals", Qt::QueuedConnection);
                mDeferralQueued = true;
            }
        }
    }
}

void MapImageManager::processDeferrals()
{
    QList<MapImage*> mapImages = mDeferredMapImages;
    mDeferredMapImages.clear();
    mDeferralQueued = false;
    foreach (MapImage *mapImage, mapImages)
        emit mapImageChanged(mapImage);
}

///// ///// ///// ///// /////

MapImage::MapImage(QImage image, qreal scale, const QRectF &levelZeroBounds, MapInfo *mapInfo)
    : mImage(image)
    , mInfo(mapInfo)
    , mLevelZeroBounds(levelZeroBounds)
    , mScale(scale)
    , mMissingTilesets(false)
    , mLoaded(false)
{
}

QPointF MapImage::tileToPixelCoords(qreal x, qreal y)
{
    const int tileWidth = mInfo->tileWidth();
    const int tileHeight = mInfo->tileHeight();
    const int originX = mInfo->height() * tileWidth / 2;

    return QPointF((x - y) * tileWidth / 2 + originX,
                   (x + y) * tileHeight / 2);
}

QRectF MapImage::tileBoundingRect(const QRect &rect)
{
    const int tileWidth = mInfo->tileWidth();
    const int tileHeight = mInfo->tileHeight();

    const int originX = mInfo->height() * tileWidth / 2;
    const QPoint pos((rect.x() - (rect.y() + rect.height()))
                     * tileWidth / 2 + originX,
                     (rect.x() + rect.y()) * tileHeight / 2);

    const int side = rect.height() + rect.width();
    const QSize size(side * tileWidth / 2,
                     side * tileHeight / 2);

    return QRect(pos, size);
}

QRectF MapImage::bounds()
{
    int mapWidth = mInfo->width(), mapHeight = mInfo->height();
    return tileBoundingRect(QRect(0,0,mapWidth,mapHeight));
}

qreal MapImage::scale()
{
    return mScale;
}

QPointF MapImage::tileToImageCoords(qreal x, qreal y)
{
    QPointF pos = tileToPixelCoords(x, y);
    pos += mLevelZeroBounds.topLeft();
    return pos * scale();
}

void MapImage::mapFileChanged(QImage image, qreal scale, const QRectF &levelZeroBounds)
{
    mImage = image;
    mScale = scale;
    mLevelZeroBounds = levelZeroBounds;
}

/////

MapImageReaderThread::MapImageReaderThread() :
    mQuit(false)
{
}

MapImageReaderThread::~MapImageReaderThread()
{
    mMutex.lock();
    mQuit = true;
    mWaitCondition.wakeOne();
    mMutex.unlock();
    wait();
}

void MapImageReaderThread::run()
{
    forever {
        if (mQuit) // mutex protect it?
            break;

        mMutex.lock();
        Job job = mJobs.takeAt(0);
        mMutex.unlock();

        //            qDebug() << "MapImageReaderThread" << job.imageFileName;

        QImage *image = new QImage(job.imageFileName);
#ifndef QT_NO_DEBUG
        msleep(250);
#endif
        emit imageLoaded(image, job.mapImage);

        if (!mJobs.size()) {
            //                qDebug() << "MapImageReaderThread paused";
            mMutex.lock();
            mWaitCondition.wait(&mMutex);
            mMutex.unlock();
        }
    }
}

void MapImageReaderThread::addJob(const QString &imageFileName, MapImage *mapImage)
{
    QMutexLocker locker(&mMutex);
    mJobs += Job(imageFileName, mapImage);

    if (!isRunning())
        start();
    else {
        mWaitCondition.wakeOne();
    }
}

/////

MapImageRenderThread::MapImageRenderThread() :
    mQuit(false)
{
}

MapImageRenderThread::~MapImageRenderThread()
{
    mMutex.lock();
    mQuit = true;
    mWaitCondition.wakeOne();
    mMutex.unlock();
    wait();

    cleanupDoneJobs();
}

void MapImageRenderThread::run()
{
    forever {
        if (mQuit) // mutex protect it?
            break;

        mMutex.lock();
        Job job = mJobs.takeAt(0);
        mMutex.unlock();

        QImage *image = generateMapImage(job.mapComposite);

        mMutex.lock();
        mDoneJobs += job;
        mMutex.unlock();

        if (image)
            emit imageRendered(image, job.mapImage);

        if (!mJobs.size()) {
            mMutex.lock();
            qDebug() << "MapImageRenderThread sleeping";
            mWaitCondition.wait(&mMutex);
            qDebug() << "MapImageRenderThread waking";
            mMutex.unlock();
        }
    }
}

void MapImageRenderThread::addJob(MapImage *mapImage)
{
    QMutexLocker locker(&mMutex);
    mJobs += Job(mapImage);

    if (!isRunning())
        start();
    else {
        mWaitCondition.wakeOne();
    }
}

void MapImageRenderThread::cleanupDoneJobs()
{
    QMutexLocker locker(&mMutex);
    foreach (const Job &job, mDoneJobs)
        delete job.mapComposite;
    mDoneJobs.clear();
}

QImage *MapImageRenderThread::generateMapImage(MapComposite *mapComposite)
{
    Map *map = mapComposite->map();

    MapRenderer *renderer = NULL;

    switch (map->orientation()) {
    case Map::Isometric:
        renderer = new IsometricRenderer(map);
        break;
    case Map::LevelIsometric:
        renderer = new ZLevelRenderer(map);
        break;
    case Map::Orthogonal:
        renderer = new OrthogonalRenderer(map);
        break;
    case Map::Staggered:
        renderer = new StaggeredRenderer(map);
        break;
    default:
        return 0;
    }

    // Don't draw empty levels
    int maxLevel = 0;
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        if (!layerGroup->bounds().isEmpty())
            maxLevel = layerGroup->level();
    }
    renderer->setMaxLevel(maxLevel);

    QRectF sceneRect = mapComposite->boundingRect(renderer);
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return 0;

    qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
    mapSize *= scale;

    QImage *image = new QImage(mapSize, QImage::Format_ARGB32);
    image->fill(Qt::transparent);
    QPainter painter(image);

    painter.setRenderHints(QPainter::SmoothPixmapTransform |
                           QPainter::HighQualityAntialiasing);
    painter.setTransform(QTransform::fromScale(scale, scale).translate(-sceneRect.left(), -sceneRect.top()));

    foreach (MapComposite::ZOrderItem zo, mapComposite->zOrder()) {
        if (zo.group) {
            renderer->drawTileLayerGroup(&painter, zo.group);
        } else if (TileLayer *tl = zo.layer->asTileLayer()) {
            if (tl->name().contains(QLatin1String("NoRender")))
                continue;
            renderer->drawTileLayer(&painter, tl);
        }
    }

    return image;
}

MapImageRenderThread::Job::Job(MapImage *mapImage) :
    mapComposite(new MapComposite(mapImage->mapInfo())),
    mapImage(mapImage)
{
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        foreach (TileLayer *tl, layerGroup->layers()) {
            bool isVisible = true;
            if (tl->name().contains(QLatin1String("NoRender")))
                isVisible = false;
            layerGroup->setLayerVisibility(tl, isVisible);
            layerGroup->setLayerOpacity(tl, 1.0f);
        }
        layerGroup->synch();
    }
}
