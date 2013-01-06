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

#ifndef BUILDINGTOOLS_H
#define BUILDINGTOOLS_H

#include "buildingobjects.h" // need RoofType enum

#include <QObject>
#include <QPointF>
#include <QRegion>
#include <QSet>
#include <QSize>

class QAction;
class QGraphicsItem;
class QGraphicsPathItem;
class QGraphicsRectItem;
class QGraphicsSceneMouseEvent;
class QImage;
class QUndoStack;

namespace BuildingEditor {

class BuildingFloor;
class BuildingTile;
class Door;
class FloorEditor;
class FurnitureTile;
class GraphicsObjectItem;
class GraphicsRoofItem;
class GraphicsRoofCornerItem;
class GraphicsRoofHandleItem;
class GraphicsWallItem;
class GraphicsWallHandleItem;
class WallObject;

/////

class BaseTool : public QObject
{
    Q_OBJECT
public:
    BaseTool();

    virtual void setEditor(FloorEditor *editor);

    void setAction(QAction *action)
    { mAction = action; }

    QAction *action() const
    { return mAction; }

    void setEnabled(bool enabled);

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

    virtual void currentModifiersChanged(Qt::KeyboardModifiers modifiers)
    { Q_UNUSED(modifiers) }

    Qt::KeyboardModifiers keyboardModifiers() const;

    bool controlModifier() const;
    bool shiftModifier() const;

    QString statusText() const
    { return mStatusText; }

    void setStatusText(const QString &text);

    BuildingFloor *floor() const;
    QUndoStack *undoStack() const;

    bool isCurrent();

signals:
    void statusTextChanged();

public slots:
    void makeCurrent();
    virtual void documentChanged() {}
    virtual void activate() = 0;
    virtual void deactivate() = 0;

protected:
    FloorEditor *mEditor;
    QAction *mAction;
    QString mStatusText;
};

/////

class ToolManager : public QObject
{
    Q_OBJECT
public:
    static ToolManager *instance();

    ToolManager();

    void addTool(BaseTool *tool);
    void activateTool(BaseTool *tool);

    void toolEnabledChanged(BaseTool *tool, bool enabled);

    BaseTool *currentTool() const
    { return mCurrentTool; }

    void checkKeyboardModifiers(Qt::KeyboardModifiers modifiers);

    Qt::KeyboardModifiers keyboardModifiers()
    { return mCurrentModifiers; }

    void clearDocument();

signals:
    void currentToolChanged(BaseTool *tool);
    void statusTextChanged(BaseTool *tool);

private slots:
    void currentToolStatusTextChanged();

private:
    static ToolManager *mInstance;
    QList<BaseTool*> mTools;
    BaseTool *mCurrentTool;
    Qt::KeyboardModifiers mCurrentModifiers;
};

/////

class PencilTool : public BaseTool
{
    Q_OBJECT
public:
    static PencilTool *instance();

    PencilTool();

    void documentChanged();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

public slots:
    void activate();
    void deactivate();

private:
    void updateCursor(const QPointF &scenePos);
    void updateStatusText();

private:
    static PencilTool *mInstance;
    bool mMouseDown;
    bool mErasing;
    QPointF mMouseScenePos;
    QPoint mStartTilePos;
    QRect mCursorTileBounds;
    QGraphicsRectItem *mCursor;
    QRectF mCursorViewRect;
};

/////

class SelectMoveRoomsTool : public BaseTool
{
    Q_OBJECT
public:
    static SelectMoveRoomsTool *instance();

    SelectMoveRoomsTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

    QRegion setSelectedArea(const QRegion &selectedArea);

public slots:
    void documentChanged();
    void activate();
    void deactivate();

private:
    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    void updateSelection(const QPointF &pos,
                         Qt::KeyboardModifiers modifiers);

    void startSelecting();

    void startMoving();
    void updateMovingItems();
    void finishMoving(const QPointF &pos);
    void cancelMoving();

    void finishMovingFloor(BuildingFloor *floor, bool objectsToo);

    void updateStatusText();

    static SelectMoveRoomsTool *mInstance;

    Mode mMode;
    bool mMouseDown;
    bool mMouseOverSelection;
    QPointF mStartScenePos;
    QPoint mStartTilePos;
    QPoint mDragOffset;
    QGraphicsPathItem *mSelectionItem;
    QRegion mSelectedArea;
};

/////

class BaseObjectTool : public BaseTool
{
    Q_OBJECT
public:
    BaseObjectTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

public slots:
    void documentChanged();
    void activate();
    void deactivate();

protected:
    virtual void updateCursorObject() = 0;
    void setCursorObject(BuildingObject *object);
    virtual void placeObject() = 0;

    enum TileEdge {
        Center,
        N,
        S,
        W,
        E
    };

    QPoint mTilePos;
    TileEdge mTileEdge;
    BuildingObject *mCursorObject;
    GraphicsObjectItem *mCursorItem;
};

class DoorTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static DoorTool *instance();

    DoorTool();

    void placeObject();
    void updateCursorObject();

private:
    static DoorTool *mInstance;
};

class WindowTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static WindowTool *instance();

    WindowTool();

    void placeObject();
    void updateCursorObject();

private:
    static WindowTool *mInstance;
};

class StairsTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static StairsTool *instance();

    StairsTool();

    void placeObject();
    void updateCursorObject();

private:
    static StairsTool *mInstance;
};

class FurnitureTool : public BaseObjectTool
{
    Q_OBJECT
public:
    static FurnitureTool *instance();

    FurnitureTool();

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

    void placeObject();
    void updateCursorObject();

    void setCurrentTile(FurnitureTile *tile);

    FurnitureTile *currentTile() const
    { return mCurrentTile; }

    enum Orient {
        OrientW,
        OrientN,
        OrientE,
        OrientS,
        OrientNW,
        OrientNE,
        OrientSW,
        OrientSE,
        OrientNone
    };
private:
    Orient calcOrient(int x, int y);

    Orient calcOrient(const QPoint &tilePos)
    { return calcOrient(tilePos.x(), tilePos.y()); }

private:
    static FurnitureTool *mInstance;
    FurnitureTile *mCurrentTile;
};

class RoofTool : public BaseTool
{
    Q_OBJECT
public:
    static RoofTool *instance();

    RoofTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void documentChanged();
    void activate();
    void deactivate();

    void setRoofType(RoofObject::RoofType type)
    { mRoofType = type; }

private slots:
    void objectAboutToBeRemoved(BuildingObject *object);

private:
    RoofObject *topmostRoofAt(const QPointF &scenePos);
    void updateHandle(const QPointF &scenePos);
    void resizeRoof(int width, int height);
    void toggleCappedW();
    void toggleCappedN();
    void toggleCappedE();
    void toggleCappedS();
    void depthUp();
    void depthDown();
    void updateStatusText();

private:
    static RoofTool *mInstance;
    RoofObject::RoofType mRoofType;

    enum Mode {
        NoMode,
        Create,
        Resize
    };
    Mode mMode;

    QPoint mStartPos;
    QPoint mCurrentPos;
    RoofObject *mObject;
    GraphicsObjectItem *mItem;
    QGraphicsRectItem *mCursorItem;
    QRectF mCursorViewRect;

    GraphicsRoofItem *mObjectItem; // item for roof object mouse is over
    RoofObject *mHandleObject; // roof object mouse is over
    GraphicsRoofHandleItem *mHandleItem;
    bool mMouseOverHandle;
    int mOriginalWidth, mOriginalHeight;
};

/////

class RoofCornerTool : public RoofTool
{
public:
    static RoofCornerTool *instance();

    RoofCornerTool();

private:
    static RoofCornerTool *mInstance;
};

/////

class SelectMoveObjectTool : public BaseTool
{
    Q_OBJECT
public:
    static SelectMoveObjectTool *instance();

    SelectMoveObjectTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void currentModifiersChanged(Qt::KeyboardModifiers modifiers);

public slots:
    void documentChanged();
    void activate();
    void deactivate();

private:
    enum Mode {
        NoMode,
        Selecting,
        Moving,
        CancelMoving
    };

    void updateSelection(const QPointF &pos,
                         Qt::KeyboardModifiers modifiers);

    void startSelecting();

    void startMoving();
    void updateMovingItems(const QPointF &pos,
                           Qt::KeyboardModifiers modifiers);
    void finishMoving(const QPointF &pos);
    void cancelMoving();

    void updateStatusText();

    static SelectMoveObjectTool *mInstance;

    Mode mMode;
    bool mMouseDown;
    bool mMouseOverObject;
    bool mMouseOverSelection;
    QPointF mStartScenePos;
    QPoint mDragOffset;
    BuildingObject *mClickedObject;
    QSet<BuildingObject*> mMovingObjects;
    QGraphicsRectItem *mSelectionRectItem;
    QList<GraphicsObjectItem*> mClones;
};

/////

class WallTool : public BaseTool
{
    Q_OBJECT
public:
    static WallTool *instance();

    WallTool();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void documentChanged();
    void activate();
    void deactivate();

    void setCurrentExteriorTile(BuildingTileEntry *entry)
    { mCurrentExteriorTile = entry; }

    void setCurrentInteriorTile(BuildingTileEntry *entry)
    { mCurrentInteriorTile = entry; }

    BuildingTileEntry *currentExteriorTile() const
    { return mCurrentExteriorTile; }

    BuildingTileEntry *currentInteriorTile() const
    { return mCurrentInteriorTile; }

private slots:
    void objectAboutToBeRemoved(BuildingObject *object);

private:
    WallObject *topmostWallAt(const QPointF &scenePos);
    void updateHandle(const QPointF &scenePos);
    void updateStatusText();
    void resizeWall(int length);

private:
    static WallTool *mInstance;

    enum Mode {
        NoMode,
        Create,
        Resize
    };
    Mode mMode;

    QPoint mStartPos;
    QPoint mCurrentPos;
    WallObject *mObject;
    GraphicsObjectItem *mItem;
    QGraphicsRectItem *mCursorItem;
    QRectF mCursorViewRect;

    GraphicsWallItem *mObjectItem; // item for object mouse is over
    WallObject *mHandleObject; // object mouse is over
    GraphicsWallHandleItem *mHandleItem;
    bool mMouseOverHandle;
    int mOriginalLength;

    BuildingTileEntry *mCurrentExteriorTile;
    BuildingTileEntry *mCurrentInteriorTile;
};

} // namespace BuildingEditor

#endif // BUILDINGTOOLS_H
