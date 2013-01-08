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

#ifndef BUILDINGTILETOOLS_H
#define BUILDINGTILETOOLS_H

#include "buildingtools.h"

#include <QBrush>
#include <QGraphicsPolygonItem>
#include <QPen>
#include <QRectF>

class QAction;
class QGraphicsSceneMouseEvent;
class QUndoStack;

namespace BuildingEditor {

class BaseFloorEditor;
class BuildingDocument;
class BuildingFloor;
class BuildingTileModeScene;
class FloorTileGrid;

/////

class DrawTileToolCursor : public QGraphicsItem
{
public:
    DrawTileToolCursor(BaseFloorEditor *editor, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget);

    void setColor(const QColor &color);

    void setTileRegion(const QRegion &tileRgn);

private:
    BaseFloorEditor *mEditor;
    QRegion mRegion;
    QRectF mBoundingRect;
    QColor mColor;
};

class DrawTileTool : public BaseTool
{
    Q_OBJECT
public:
    static DrawTileTool *instance();

    DrawTileTool();

    void documentChanged();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

    void setTile(const QString &tileName);

    QString currentTile() const
    { return mTileName; }

    void setCaptureTiles(FloorTileGrid *tiles, const QRegion &rgn);

public slots:
    void activate();
    void deactivate();

private:
    void beginCapture();
    void endCapture();
    void clearCaptureTiles();

    void updateCursor(const QPointF &scenePos, bool force = true);
    void updateStatusText();

private:
    static DrawTileTool *mInstance;
    bool mMouseDown;
    bool mMouseMoved;
    bool mErasing;
    QPointF mMouseScenePos;
    QPointF mStartScenePos;
    QPoint mStartTilePos;
    QPoint mCursorTilePos;
    QRect mCursorTileBounds;
    DrawTileToolCursor *mCursor;
    bool mCapturing;
    FloorTileGrid *mCaptureTiles;
    QRegion mCaptureTilesRgn;

    QString mTileName;
};

class SelectTileTool : public BaseTool
{
    Q_OBJECT
public:
    static SelectTileTool *instance();

    SelectTileTool();

    void documentChanged();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

public slots:
    void activate();
    void deactivate();

private:
    void updateCursor(const QPointF &scenePos, bool force = true);
    void updateStatusText();

private:
    static SelectTileTool *mInstance;

    enum SelectionMode {
        Replace,
        Add,
        Subtract,
        Intersect
    };

    SelectionMode mSelectionMode;
    bool mMouseDown;
    bool mMouseMoved;
    QPointF mMouseScenePos;
    QPointF mStartScenePos;
    QPoint mStartTilePos;
    QPoint mCursorTilePos;
    QRect mCursorTileBounds;
    DrawTileToolCursor *mCursor;
    QRegion mSelectedRegion;
};

} // namespace BuildingEditor

#endif // BUILDINGTILETOOLS_H
