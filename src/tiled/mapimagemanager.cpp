#include "mapimagemanager.h"

#include "imagelayer.h"
#include "isometricrenderer.h"
#include "map.h"
#include "mapcomposite.h"
#include "mapmanager.h"
#include "objectgroup.h"
#include "zprogress.hpp"
#include "zlevelrenderer.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

using namespace Tiled;

const int IMAGE_WIDTH = 512;

MapImageManager *MapImageManager::mInstance = NULL;

MapImageManager::MapImageManager()
    : QObject()
{
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
    QString mapFilePath = MapManager::instance()->pathForMap(mapName, relativeTo);
    if (mapFilePath.isEmpty())
        return 0;

    if (mMapImages.contains(mapFilePath))
        return mMapImages[mapFilePath];

    QImage image = generateMapImage(mapFilePath);
    if (image.isNull())
        return 0;

    MapInfo *mapInfo = MapManager::instance()->mapInfo(mapFilePath);
    MapImage *mapImage = new MapImage(image, mapInfo);
    mMapImages.insert(mapFilePath, mapImage);
    return mapImage;
}

QImage MapImageManager::generateMapImage(const QString &mapFilePath)
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
    if (imageInfo.exists()) {
        QImage image(imageInfo.absoluteFilePath());
        if (image.isNull())
            QMessageBox::warning(0, tr("Error Loading Image"),
                                 tr("An error occurred trying to read a map thumbnail image.\n") + imageInfo.absoluteFilePath());
        if (image.width() == IMAGE_WIDTH)
            return image;
    }

    PROGRESS progress(tr("Generating thumbnail for %1").arg(fileInfo.completeBaseName()));

    MapInfo *mapInfo = MapManager::instance()->loadMap(mapFilePath);
    if (!mapInfo)
        return QImage(); // TODO: Add error handling

    progress.update(tr("Generating thumbnail for %1").arg(fileInfo.completeBaseName()));

    MapComposite mapComposite(mapInfo);
    QImage image = generateMapImage(&mapComposite);

    image.save(imageInfo.absoluteFilePath());
    return image;
}

QImage MapImageManager::generateMapImage(MapComposite *mapComposite)
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
    }

    QRectF sceneRect = mapComposite->boundingRect(renderer);
    QSize mapSize = sceneRect.size().toSize();
    if (mapSize.isEmpty())
        return QImage();

    qreal scale = IMAGE_WIDTH / qreal(mapSize.width());
    mapSize *= scale;

    QImage image(mapSize, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    painter.setRenderHints(QPainter::SmoothPixmapTransform |
                           QPainter::HighQualityAntialiasing);
    painter.setTransform(QTransform::fromScale(scale, scale).translate(0, -sceneRect.top()));


#if 1
    mapComposite->saveVisibility();
    foreach (Layer *layer, map->layers())
        layer->setVisible(!layer->name().contains(QLatin1String("NoRender")));
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups()) {
        layerGroup->synch();
        renderer->drawTileLayerGroup(&painter, layerGroup);
    }
    mapComposite->restoreVisibility();
    foreach (CompositeLayerGroup *layerGroup, mapComposite->sortedLayerGroups())
        layerGroup->synch();
#elif 0
    const QMap<int,CompositeLayerGroup*> &layerGroups = mapComposite->layerGroups();
    for (int level = 0; level <= map->maxLevel(); level++) {
        if (layerGroups.contains(level))
            renderer->drawTileLayerGroup(&painter, layerGroups[level]);
    }
#else
    bool visibleLayersOnly = false;

    foreach (Layer *layer, map->layers()) {
        if (visibleLayersOnly && !layer->isVisible())
            continue;

        painter.setOpacity(layer->opacity());

        if (TileLayer *tileLayer = layer->asTileLayer()) {
            if (layer->name().contains(QLatin1String("NoRender")))
                continue;
            renderer->drawTileLayer(&painter, tileLayer);
        } else if (ObjectGroup *objGroup = layer->asObjectGroup()) {
            foreach (const MapObject *object, objGroup->objects()) {
//                const QColor color = MapObjectItem::objectColor(object);
//                renderer->drawMapObject(&painter, object, color);
            }
        } else if (ImageLayer *imageLayer = layer->asImageLayer()) {
            renderer->drawImageLayer(&painter, imageLayer);
        }
    }
#endif

    delete renderer;

    return image;
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
    return QFileInfo(imagesDirInfo.absoluteFilePath() + QLatin1Char('/') +
                     mapFileInfo.completeBaseName() + QLatin1String(".png"));
}

///// ///// ///// ///// /////

MapImage::MapImage(QImage image, MapInfo *mapInfo)
    : mImage(image)
    , mInfo(mapInfo)
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
    return (mImage.width() / bounds().width());
}

QPointF MapImage::tileToImageCoords(qreal x, qreal y)
{
    QPointF pos = tileToPixelCoords(x, y);
    // this is the drawMargins of the map (plus LevelIsometric height, if any)
    pos += QPointF(0, mImage.height() / scale() - bounds().height());
    return pos * scale();
}