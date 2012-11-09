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

#include "buildingtools.h"

#include "building.h"
#include "buildingdocument.h"
#include "buildingeditorwindow.h"
#include "buildingfloor.h"
#include "buildingobjects.h"
#include "buildingtemplates.h"
#include "buildingtiles.h"
#include "buildingundoredo.h"
#include "FloorEditor.h"
#include "furnituregroups.h"
#include "rooftiles.h"

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QGraphicsRectItem>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QUndoStack>

using namespace BuildingEditor;

/////

BaseTool::BaseTool() :
    QObject(0),
    mEditor(0)
{
    ToolManager::instance()->addTool(this);
}

void BaseTool::setEditor(FloorEditor *editor)
{
    mEditor = editor;

    connect(mEditor, SIGNAL(documentChanged()), SLOT(documentChanged()));
}

void BaseTool::setEnabled(bool enabled)
{
    if (enabled != mAction->isEnabled()) {
        mAction->setEnabled(enabled);
        ToolManager::instance()->toolEnabledChanged(this, enabled);
    }
}

void BaseTool::setStatusText(const QString &text)
{
    mStatusText = text;
    emit statusTextChanged();
}

BuildingFloor *BaseTool::floor() const
{
    return mEditor->document()->currentFloor();
}

bool BaseTool::isCurrent()
{
    return ToolManager::instance()->currentTool() == this;
}

void BaseTool::makeCurrent()
{
    ToolManager::instance()->activateTool(this);
}

/////

ToolManager *ToolManager::mInstance = 0;

ToolManager *ToolManager::instance()
{
    if (!mInstance)
        mInstance = new ToolManager;
    return mInstance;
}

ToolManager::ToolManager() :
    QObject(),
    mCurrentTool(0)
{
}

void ToolManager::addTool(BaseTool *tool)
{
    mTools += tool;
}

void ToolManager::activateTool(BaseTool *tool)
{
    if (mCurrentTool) {
        mCurrentTool->deactivate();
        mCurrentTool->action()->setChecked(false);
        mCurrentTool->disconnect(this);
    }

    mCurrentTool = tool;

    if (mCurrentTool) {
        connect(mCurrentTool, SIGNAL(statusTextChanged()),
                SLOT(currentToolStatusTextChanged()));
        mCurrentTool->activate();
        mCurrentTool->action()->setChecked(true);
    }

    emit currentToolChanged(mCurrentTool);
}

void ToolManager::toolEnabledChanged(BaseTool *tool, bool enabled)
{
    if (!enabled && tool == mCurrentTool) {
        foreach (BaseTool *tool2, mTools) {
            if (tool2 != tool && tool2->action()->isEnabled()) {
                activateTool(tool2);
                return;
            }
        }
        mCurrentTool = 0;
        emit currentToolChanged(mCurrentTool);
    }
}

void ToolManager::currentToolStatusTextChanged()
{
    emit statusTextChanged(mCurrentTool);
}

/////

PencilTool *PencilTool::mInstance = 0;

PencilTool *PencilTool::instance()
{
    if (!mInstance)
        mInstance = new PencilTool();
    return mInstance;
}

PencilTool::PencilTool() :
    BaseTool(),
    mMouseDown(false),
    mCursor(0)
{
    setStatusText(tr("Left-click to draw a room.  Right-click to switch to room under pointer."));
}

void PencilTool::documentChanged()
{
    mCursor = 0; // it was deleted from the editor
}

void PencilTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QPoint tilePos = mEditor->sceneToTile(event->scenePos());

    if (event->button() == Qt::RightButton) {
        if (!mEditor->currentFloorContains(tilePos))
            return;
        Room *room = mEditor->document()->currentFloor()->GetRoomAt(tilePos);
        if (room) {
            BuildingEditorWindow::instance->setCurrentRoom(room);
            updateCursor(event->scenePos());
        }
        return;
    }

    mInitialPaint = true;
    if (mEditor->currentFloorContains(tilePos) &&
            mEditor->document()->currentFloor()->GetRoomAt(tilePos) != BuildingEditorWindow::instance->currentRoom()) {
        mEditor->document()->undoStack()->push(new PaintRoom(mEditor->document(),
                                                             mEditor->document()->currentFloor(),
                                                             tilePos,
                                                             BuildingEditorWindow::instance->currentRoom()));
        mInitialPaint = false;
    }
    mMouseDown = true;
}

void PencilTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updateCursor(event->scenePos());

    if (mMouseDown) {
        QPoint tilePos = mEditor->sceneToTile(event->scenePos());
        if (mEditor->currentFloorContains(tilePos) &&
                mEditor->document()->currentFloor()->GetRoomAt(tilePos) != BuildingEditorWindow::instance->currentRoom()) {
            PaintRoom *cmd = new PaintRoom(mEditor->document(),
                                           mEditor->document()->currentFloor(),
                                           tilePos,
                                           BuildingEditorWindow::instance->currentRoom());
            cmd->setMergeable(!mInitialPaint);
            mEditor->document()->undoStack()->push(cmd);
            mInitialPaint = false;
        }
    }
}

void PencilTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mMouseDown) {
        mMouseDown = false;
    }
}

void PencilTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
}

void PencilTool::deactivate()
{
    if (mCursor)
        mEditor->removeItem(mCursor);
}

void PencilTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos);
    if (!mCursor) {
        mCursor = new QGraphicsRectItem;
        mCursor->setZValue(FloorEditor::ZVALUE_CURSOR);
    }
    mCursor->setRect(mEditor->tileToSceneRect(tilePos).adjusted(0,0,-1,-1));
    mCursor->setBrush(QColor(BuildingEditorWindow::instance->currentRoom()->Color));
    mCursor->setVisible(mEditor->currentFloorContains(tilePos));
}

/////

EraserTool *EraserTool::mInstance = 0;

EraserTool *EraserTool::instance()
{
    if (!mInstance)
        mInstance = new EraserTool();
    return mInstance;
}

EraserTool::EraserTool() :
    BaseTool(),
    mMouseDown(false),
    mCursor(0)
{
    setStatusText(tr("Left-click to erase room."));
}

void EraserTool::documentChanged()
{
    mCursor = 0; // it was deleted from the editor
}

void EraserTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    mInitialPaint = true;
    QPoint tilePos = mEditor->sceneToTile(event->scenePos());
    if (mEditor->currentFloorContains(tilePos) &&
            mEditor->document()->currentFloor()->GetRoomAt(tilePos) != 0) {
        mEditor->document()->undoStack()->push(new EraseRoom(mEditor->document(),
                                                             mEditor->document()->currentFloor(),
                                                             tilePos));
        mInitialPaint = false;
    }
    mMouseDown = true;
}

void EraserTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    updateCursor(event->scenePos());

    if (mMouseDown) {
        QPoint tilePos = mEditor->sceneToTile(event->scenePos());
        if (mEditor->currentFloorContains(tilePos) &&
                mEditor->document()->currentFloor()->GetRoomAt(tilePos) != 0) {
            EraseRoom *cmd = new EraseRoom(mEditor->document(),
                                                      mEditor->document()->currentFloor(),
                                                      tilePos);
            cmd->setMergeable(!mInitialPaint);
            mEditor->document()->undoStack()->push(cmd);
            mInitialPaint = false;
        }
    }
}

void EraserTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
    if (mMouseDown) {
        mMouseDown = false;
    }
}

void EraserTool::activate()
{
    updateCursor(QPointF(-100,-100));
    mEditor->addItem(mCursor);
}

void EraserTool::deactivate()
{
    if (mCursor)
        mEditor->removeItem(mCursor);
}

void EraserTool::updateCursor(const QPointF &scenePos)
{
    QPoint tilePos = mEditor->sceneToTile(scenePos);
    if (!mCursor) {
        QPen pen(QColor(255,0,0,128));
        pen.setWidth(3);
        mCursor = new QGraphicsRectItem;
        mCursor->setPen(pen);
        mCursor->setZValue(FloorEditor::ZVALUE_CURSOR);
    }
    mCursor->setRect(mEditor->tileToSceneRect(tilePos).adjusted(0,0,-1,-1));
    mCursor->setVisible(mEditor->currentFloorContains(tilePos));
}

/////

SelectMoveRoomsTool *SelectMoveRoomsTool::mInstance = 0;

SelectMoveRoomsTool *SelectMoveRoomsTool::instance()
{
    if (!mInstance)
        mInstance = new SelectMoveRoomsTool;
    return mInstance;
}

SelectMoveRoomsTool::SelectMoveRoomsTool() :
    BaseTool(),
    mMode(NoMode),
    mMouseDown(false),
    mSelectionItem(0),
    mBmp(0)
{
    setStatusText(tr("Left-click to select.  Left-click-drag selection to move rooms."));
}

void SelectMoveRoomsTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMouseDown = true;
        mStartScenePos = event->scenePos();
        mStartTilePos = mEditor->sceneToTile(event->scenePos());
    }
    if (event->button() == Qt::RightButton) {
        if (mMode == Moving)
            cancelMoving();
    }
}

void SelectMoveRoomsTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pos = event->scenePos();

    if (mMode == NoMode && mMouseDown) {
        const int dragDistance = (mStartScenePos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            QPoint tilePos = mEditor->sceneToTile(event->scenePos());
            if (mSelectedArea.contains(tilePos))
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting: {
        QPoint tilePos = mEditor->sceneToTile(pos);
        QRect tileBounds = QRect(mStartTilePos, tilePos).normalized();

        mSelectedArea = QRegion(tileBounds);
        QPainterPath path;
        path.addRegion(mSelectedArea);
        mSelectionItem->setPath(path);
        break;
    }
    case Moving:
        updateMovingItems(pos, event->modifiers());
        break;
    case CancelMoving:
        break;
    case NoMode:
        break;
    }
}

void SelectMoveRoomsTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode: // TODO: single-click to select adjoining tiles of a room
        if (mSelectionItem && !mSelectedArea.contains(mStartTilePos)) {
            delete mSelectionItem;
            mSelectionItem = 0;
            mSelectedArea = QRegion();
        }
#if 0
        if (mClickedObject) {
            QSet<BuildingObject*> selection = mEditor->document()->selectedObjects();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedObject))
                    selection.remove(mClickedObject);
                else
                    selection.insert(mClickedObject);
            } else {
                selection.clear();
                selection.insert(mClickedObject);
            }
            mEditor->document()->setSelectedObjects(selection);
        } else {
            mEditor->document()->setSelectedObjects(QSet<BuildingObject*>());
        }
#endif
        break;
    case Selecting:
        updateSelection(event->scenePos(), event->modifiers());
//        mEditor->removeItem(mSelectionItem);
        mMode = NoMode;
        break;
    case Moving:
        mMouseDown = false;
        finishMoving(event->scenePos());
        break;
    case CancelMoving:
        mMode = NoMode;
        break;
    }

    mMouseDown = false;
}

void SelectMoveRoomsTool::documentChanged()
{
    mSelectionItem = 0;
}

void SelectMoveRoomsTool::activate()
{
}

void SelectMoveRoomsTool::deactivate()
{
    delete mSelectionItem;
    mSelectionItem = 0;
    mSelectedArea = QRegion();
}

void SelectMoveRoomsTool::updateSelection(const QPointF &pos,
                                           Qt::KeyboardModifiers modifiers)
{
}

void SelectMoveRoomsTool::startSelecting()
{
    mMode = Selecting;
    if (mSelectionItem == 0) {
        mSelectionItem = new QGraphicsPathItem();
        mSelectionItem->setPen(QColor(0x33,0x99,0xff));
        mSelectionItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
        mSelectionItem->setZValue(FloorEditor::ZVALUE_CURSOR);
        mSelectionItem->setScale(30);
        mEditor->addItem(mSelectionItem);
    }
    mSelectedArea = QRegion(QRect(mStartTilePos, QSize(1,1)));
    QPainterPath path;
    path.addRegion(mSelectedArea);
    mSelectionItem->setPath(path);
}

void SelectMoveRoomsTool::startMoving()
{
    mMode = Moving;
    mDragOffset = QPoint();

    GraphicsFloorItem *item = mEditor->itemForFloor(floor());
    mBmp = new QImage(item->bmp()->copy());
    item->setDragBmp(mBmp);
}

void SelectMoveRoomsTool::updateMovingItems(const QPointF &pos,
                                             Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPoint startTilePos = mStartTilePos;
    QPoint currentTilePos = mEditor->sceneToTile(pos);
    mDragOffset = currentTilePos - startTilePos;

    BuildingFloor *floor = this->floor();
    QImage *bmp = mEditor->itemForFloor(floor)->bmp();
    for (int x = 0; x < floor->width(); x++) {
        for (int y = 0; y < floor->height(); y++) {
            if (mSelectedArea.contains(QPoint(x, y)))
                mBmp->setPixel(x, y, qRgb(0,0,0));
            else
                mBmp->setPixel(x, y, bmp->pixel(x, y));
        }
    }

    QRect floorBounds(0, 0, floor->width(), floor->height());
    foreach (QRect src, mSelectedArea.rects()) {
        src &= floorBounds;
        for (int x = src.left(); x <= src.right(); x++) {
            for (int y = src.top(); y <= src.bottom(); y++) {
                QPoint p = QPoint(x, y) + mDragOffset;
                if (floorBounds.contains(p))
                    mBmp->setPixel(p, bmp->pixel(x, y));
            }
        }
    }

    mEditor->itemForFloor(floor)->update();

    QPainterPath path;
    path.addRegion(mSelectedArea.translated(mDragOffset));
    mSelectionItem->setPath(path);
}

void SelectMoveRoomsTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)

    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    BuildingFloor *floor = this->floor();
    mEditor->itemForFloor(floor)->setDragBmp(0);
    delete mBmp;
    mBmp = 0;

    if (mDragOffset.isNull()) // Move is a no-op
        return;

    QVector<QVector<Room*> > grid = floor->grid();

    QRect floorBounds(0, 0, floor->width(), floor->height());
    foreach (QRect src, mSelectedArea.rects()) {
        src &= floorBounds;
        for (int x = src.left(); x <= src.right(); x++) {
            for (int y = src.top(); y <= src.bottom(); y++) {
                grid[x][y] = 0;
            }
        }
    }

    foreach (QRect src, mSelectedArea.rects()) {
        src &= floorBounds;
        for (int x = src.left(); x <= src.right(); x++) {
            for (int y = src.top(); y <= src.bottom(); y++) {
                QPoint p = QPoint(x, y) + mDragOffset;
                if (floorBounds.contains(p))
                    grid[p.x()][p.y()] = floor->grid()[x][y];
            }
        }
    }

    // Final position of the selection.
    mSelectedArea.translate(mDragOffset);

    mEditor->document()->undoStack()->push(new SwapFloorGrid(mEditor->document(),
                                                             floor, grid));
}

void SelectMoveRoomsTool::cancelMoving()
{
    mEditor->itemForFloor(floor())->setDragBmp(0);
    delete mBmp;
    mBmp = 0;

    QPainterPath path;
    path.addRegion(mSelectedArea);
    mSelectionItem->setPath(path);

    mMode = CancelMoving;
}

/////

BaseObjectTool::BaseObjectTool() :
    BaseTool(),
    mTileEdge(Center),
    mCursorObject(0),
    mCursorItem(0)
{
}

void BaseObjectTool::documentChanged()
{
    mCursorItem = 0; // it was deleted from the editor
}

void BaseObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        if (BuildingObject *object = mEditor->topmostObjectAt(event->scenePos())) {
            BuildingFloor *floor = mEditor->document()->currentFloor();
            mEditor->document()->undoStack()->push(new RemoveObject(mEditor->document(),
                                                                    floor,
                                                                    floor->indexOf(object)));
        }
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    if (!mCursorItem || !mCursorItem->isVisible() || !mCursorItem->isValidPos())
        return;

    placeObject();
}

void BaseObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mTilePos = mEditor->sceneToTile(event->scenePos());

    QPointF p = mEditor->sceneToTileF(event->scenePos());
    QPointF m(p.x() - int(p.x()), p.y() - int(p.y()));
    TileEdge xEdge = Center, yEdge = Center;
    if (m.x() < 0.25)
        xEdge = W;
    else if (m.x() >= 0.75)
        xEdge = E;
    if (m.y() < 0.25)
        yEdge = N;
    else if (m.y() >= 0.75)
        yEdge = S;
    if (xEdge == Center && yEdge == Center || (xEdge != Center && yEdge != Center))
        mTileEdge = Center;
    else if (xEdge != Center)
        mTileEdge = xEdge;
    else
        mTileEdge = yEdge;

    updateCursorObject();

    if (mCursorItem && mCursorObject && mCursorItem->isVisible()) {
        BuildingFloor *floor = mEditor->document()->currentFloor();
        mCursorItem->setValidPos(mCursorObject->isValidPos(QPoint(), floor));
    }
}

void BaseObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event)
}

void BaseObjectTool::activate()
{
}

void BaseObjectTool::deactivate()
{
    if (mCursorItem) {
        mEditor->removeItem(mCursorItem);
        delete mCursorItem;
        mCursorItem = 0;
    }
}

void BaseObjectTool::setCursorObject(BuildingObject *object)
{
    mCursorObject = object;
    if (!mCursorItem) {
        mCursorItem = new GraphicsObjectItem(mEditor, mCursorObject);
        mCursorItem->setZValue(FloorEditor::ZVALUE_CURSOR);
        mEditor->addItem(mCursorItem);
    }
    mCursorItem->setObject(mCursorObject);
}

/////

DoorTool *DoorTool::mInstance = 0;

DoorTool *DoorTool::instance()
{
    if (!mInstance)
        mInstance = new DoorTool();
    return mInstance;
}

DoorTool::DoorTool() :
    BaseObjectTool()
{
    setStatusText(tr("Left-click to place a door.  Right-click to remove any object."));
}

void DoorTool::placeObject()
{
    BuildingFloor *floor = mEditor->document()->currentFloor();
    Door *door = new Door(floor, mCursorObject->x(), mCursorObject->y(),
                          mCursorObject->dir());
    door->setTile(mEditor->building()->doorTile());
    door->setFrameTile(mEditor->building()->doorFrameTile());
    mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                         floor,
                                                         floor->objectCount(),
                                                         door));
}

void DoorTool::updateCursorObject()
{
    if (mTileEdge == Center || !mEditor->currentFloorContains(mTilePos)) {
        if (mCursorItem)
            mCursorItem->setVisible(false);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BuildingObject::Direction dir = BuildingObject::N;

    if (mTileEdge == W)
        dir = BuildingObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BuildingObject::W;
    }
    else if (mTileEdge == S)
        y++;

    if (!mCursorObject) {
        BuildingFloor *floor = 0; //mEditor->document()->currentFloor();
        mCursorObject = new Door(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);

    setCursorObject(mCursorObject);
}

/////

WindowTool *WindowTool::mInstance = 0;

WindowTool *WindowTool::instance()
{
    if (!mInstance)
        mInstance = new WindowTool();
    return mInstance;
}

WindowTool::WindowTool() :
    BaseObjectTool()
{
    setStatusText(tr("Left-click to place a window.  Right-click to remove any object."));
}

void WindowTool::placeObject()
{
    BuildingFloor *floor = mEditor->document()->currentFloor();
    Window *window = new Window(floor, mCursorObject->x(), mCursorObject->y(),
                                mCursorObject->dir());
    window->setTile(mEditor->building()->windowTile());
    mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                         floor,
                                                         floor->objectCount(),
                                                         window));
}

void WindowTool::updateCursorObject()
{
    if (mTileEdge == Center || !mEditor->currentFloorContains(mTilePos)) {
        if (mCursorItem)
            mCursorItem->setVisible(false);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BuildingObject::Direction dir = BuildingObject::N;

    if (mTileEdge == W)
        dir = BuildingObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BuildingObject::W;
    }
    else if (mTileEdge == S)
        y++;

    if (!mCursorObject) {
        BuildingFloor *floor = 0; //mEditor->document()->currentFloor();
        mCursorObject = new Window(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);

    setCursorObject(mCursorObject);
}

/////

StairsTool *StairsTool::mInstance = 0;

StairsTool *StairsTool::instance()
{
    if (!mInstance)
        mInstance = new StairsTool();
    return mInstance;
}

StairsTool::StairsTool() :
    BaseObjectTool()
{
    setStatusText(tr("Left-click to place stairs.  Right-click to remove any object."));
}

void StairsTool::placeObject()
{
    BuildingFloor *floor = mEditor->document()->currentFloor();
    Stairs *stairs = new Stairs(floor, mCursorObject->x(), mCursorObject->y(),
                                mCursorObject->dir());
    stairs->setTile(mEditor->building()->stairsTile());
    mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                         floor,
                                                         floor->objectCount(),
                                                         stairs));
}

void StairsTool::updateCursorObject()
{
    if (mTileEdge == Center || !mEditor->currentFloorContains(mTilePos)) {
        if (mCursorItem)
            mCursorItem->setVisible(false);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    BuildingObject::Direction dir = BuildingObject::N;

    if (mTileEdge == W)
        dir = BuildingObject::W;
    else if (mTileEdge == E) {
        x++;
        dir = BuildingObject::W;
    }
    else if (mTileEdge == S)
        y++;

    if (!mCursorObject) {
        BuildingFloor *floor = 0; //mEditor->document()->currentFloor();
        mCursorObject = new Stairs(floor, x, y, dir);
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);
    mCursorObject->setDir(dir);

    setCursorObject(mCursorObject);
}

/////

FurnitureTool *FurnitureTool::mInstance = 0;

FurnitureTool *FurnitureTool::instance()
{
    if (!mInstance)
        mInstance = new FurnitureTool();
    return mInstance;
}

FurnitureTool::FurnitureTool() :
    BaseObjectTool(),
    mCurrentTile(0)
{
    setStatusText(tr("Left-click to place furniture.  Right-click to remove any object."));
}

void FurnitureTool::placeObject()
{
    BuildingFloor *floor = mEditor->document()->currentFloor();
    FurnitureObject *object = new FurnitureObject(floor,
                                                  mCursorObject->x(),
                                                  mCursorObject->y());
    object->setFurnitureTile(mCurrentTile);
    mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                         floor,
                                                         floor->objectCount(),
                                                         object));
}

void FurnitureTool::updateCursorObject()
{
    if (!mEditor->currentFloorContains(mTilePos)) {
        if (mCursorItem)
            mCursorItem->setVisible(false);
        return;
    }

    if (mCursorItem)
        mCursorItem->setVisible(true);

    int x = mTilePos.x(), y = mTilePos.y();
    if (!mCursorObject) {
        BuildingFloor *floor = 0; //mEditor->document()->currentFloor();
        FurnitureObject *object = new FurnitureObject(floor, x, y);
        object->setFurnitureTile(mCurrentTile);
        mCursorObject = object;
    }
    // mCursorDoor->setFloor()
    mCursorObject->setPos(x, y);

    setCursorObject(mCursorObject);
}

void FurnitureTool::setCurrentTile(FurnitureTile *tile)
{
    mCurrentTile = tile;
    if (mCursorObject)
        static_cast<FurnitureObject*>(mCursorObject)->setFurnitureTile(tile);
}

/////

RoofTool *RoofTool::mInstance = 0;

RoofTool *RoofTool::instance()
{
    if (!mInstance)
        mInstance = new RoofTool;
    return mInstance;
}

RoofTool::RoofTool() :
    BaseTool(),
    mCurrentTile(0),
    mCurrentCapTile(0),
    mRoofType(RoofObject::SlopeW),
    mMode(NoMode),
    mObject(0),
    mItem(0),
    mCursorItem(0),
    mObjectItem(0),
    mHandleObject(0),
    mHandleItem(0),
    mMouseOverHandle(false)
{
}

void RoofTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode != NoMode)
            return; // ignore clicks when creating/resizing
        mStartPos = mEditor->sceneToTile(event->scenePos());
        mCurrentPos = mStartPos;
        if (mMouseOverHandle) {
            if (mHandleItem == mObjectItem->depthUpHandle()) {
                depthUp();
                return;
            }
            if (mHandleItem == mObjectItem->depthDownHandle()) {
                depthDown();
                return;
            }
            if (mHandleItem == mObjectItem->cappedWHandle()) {
                toggleCappedW();
                return;
            }
            if (mHandleItem == mObjectItem->cappedNHandle()) {
                toggleCappedN();
                return;
            }
            if (mHandleItem == mObjectItem->cappedEHandle()) {
                toggleCappedE();
                return;
            }
            if (mHandleItem == mObjectItem->cappedSHandle()) {
                toggleCappedS();
                return;
            }
            mOriginalWidth = mHandleObject->width();
            mOriginalHeight = mHandleObject->height();
            mMode = Resize;
            return;
        }
        if (!mEditor->currentFloorContains(mCurrentPos))
            return;
        mObject = new RoofObject(mEditor->document()->currentFloor(),
                                 mStartPos.x(), mStartPos.y(),
                                 /*width=*/2, /*height=*/2,
                                 /*type=*/mRoofType,
                                 /*cappedW=*/true, /*cappedN=*/true,
                                 /*cappedE=*/true, /*cappedS=*/true);
        mItem = new GraphicsObjectItem(mEditor, mObject);
        mItem->setZValue(FloorEditor::ZVALUE_CURSOR);
        mEditor->addItem(mItem);
        mMode = Create;
    }
}

void RoofTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    mCurrentPos = mEditor->sceneToTile(event->scenePos());

    if (mMode == NoMode) {
        if (!mCursorItem) {
            mCursorItem = new QGraphicsRectItem;
            mCursorItem->setZValue(FloorEditor::ZVALUE_CURSOR);
            mCursorItem->setBrush(QColor(0,255,0,128));
            mEditor->addItem(mCursorItem);
        }
        mCursorItem->setRect(mEditor->tileToSceneRect(mCurrentPos));

        updateHandle(event->scenePos());

        mCursorItem->setVisible(mEditor->currentFloorContains(mCurrentPos) &&
                                !mMouseOverHandle);
        return;
    }

    QPoint diff = mCurrentPos - mStartPos;

    if (mMode == Resize) {
        if (mCurrentPos.x() < mHandleObject->x())
            diff.setX(mHandleObject->x() - mStartPos.x() + (mHandleObject->isN() ? 1 : 0));
        if (mCurrentPos.y() < mHandleObject->y())
            diff.setY(mHandleObject->y() - mStartPos.y() + (mHandleObject->isW() ? 1 : 0));
        if (mCurrentPos.x() >= mEditor->document()->currentFloor()->width())
            diff.setX(mEditor->document()->currentFloor()->width() - mStartPos.x() - 1);
        if (mCurrentPos.y() >= mEditor->document()->currentFloor()->height())
            diff.setY(mEditor->document()->currentFloor()->height() - mStartPos.y() - 1);

        resizeRoof(mHandleObject->width() + diff.x(),
                   mHandleObject->height() + diff.y());
        return;
    }

    if (mMode == Create) {
        if (!mEditor->currentFloorContains(mCurrentPos))
            return;

        QPoint pos = mStartPos;

        // This call might restrict the width and/or height.
        mObject->resize(qAbs(diff.x()) + 1, qAbs(diff.y()) + 1);

        if (diff.x() < 0)
            pos.setX(mStartPos.x() - mObject->width() + 1);
        if (diff.y() < 0)
            pos.setY(mStartPos.y() - mObject->height() + 1);
        mObject->setPos(pos);

        mItem->synchWithObject();
        mItem->update();
    }
}

void RoofTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    if (mMode == Resize) {
        mMode = NoMode;
        int width = mHandleObject->width(), height = mHandleObject->height();
        mHandleObject->resize(mOriginalWidth, mOriginalHeight);
        mEditor->document()->undoStack()->push(new ResizeRoof(mEditor->document(),
                                                              mHandleObject,
                                                              width, height));
        return;
    }

    if (mMode == Create) {
        mMode = NoMode;
        if (mObject->isValidPos()) {
            mObject->setTile(RoofTool::instance()->currentTile());
            mObject->setTile(RoofTiles::instance()->capTileForExteriorWall(
                                 mEditor->building()->exteriorWall()), 1);
            BuildingFloor *floor = mEditor->document()->currentFloor();
            mEditor->document()->undoStack()->push(new AddObject(mEditor->document(),
                                                                 floor,
                                                                 floor->objectCount(),
                                                                 mObject));
        } else
            delete mObject;
        mObject = 0;
        delete mItem;
        mItem = 0;
    }
}

void RoofTool::documentChanged()
{
    // When the document changes, the scene is cleared, deleting our items.
    mItem = 0;
    mCursorItem = 0;
    mHandleItem = 0;
    mObjectItem = 0;

    if (mEditor->document())
        connect(mEditor->document(),
                SIGNAL(objectAboutToBeRemoved(BuildingObject*)),
                SLOT(objectAboutToBeRemoved(BuildingObject*)));
}

void RoofTool::activate()
{
    setStatusText(tr("Left-click-drag to place a roof."));
    if (mCursorItem)
        mEditor->addItem(mCursorItem);
}

void RoofTool::deactivate()
{
    if (mCursorItem)
        mEditor->removeItem(mCursorItem);
}

void RoofTool::objectAboutToBeRemoved(BuildingObject *object)
{
    if (object == mHandleObject) {
        mHandleObject = 0;
        mObjectItem = 0;
        mMouseOverHandle = false;
        mMode = NoMode;
    }
}

RoofObject *RoofTool::topmostRoofAt(const QPointF &scenePos)
{
    foreach (QGraphicsItem *item, mEditor->items(scenePos)) {
        if (GraphicsRoofItem *roofItem = dynamic_cast<GraphicsRoofItem*>(item)) {
            if (roofItem->object()->floor() == mEditor->document()->currentFloor()) {
                return roofItem->object()->asRoof();
            }
        }
    }
    return 0;
}

void RoofTool::updateHandle(const QPointF &scenePos)
{
    RoofObject *ro = topmostRoofAt(scenePos);

    if (mMouseOverHandle) {
        mHandleItem->setHighlight(false);
        mMouseOverHandle = false;
        setStatusText(tr("Left-click-drag to place a roof."));
    }
    mHandleItem = 0;
    if (ro && (ro == mHandleObject)) {
        foreach (QGraphicsItem *item, mEditor->items(scenePos)) {
            if (GraphicsRoofHandleItem *handle = dynamic_cast<GraphicsRoofHandleItem*>(item)) {
                if (handle->parentItem() == mObjectItem) {
                    setStatusText(handle->statusText());
                    mHandleItem = handle;
                    mMouseOverHandle = true;
                    mHandleItem->setHighlight(true);
                    break;
                }
            }
        }
        return;
    }

    if (mObjectItem) {
        mObjectItem->setShowHandles(false);
        mObjectItem = 0;
    }

    if (ro) {
        mObjectItem = mEditor->itemForObject(ro)->asRoof();
        mObjectItem->setShowHandles(true);
    }
    mHandleObject = ro;
}

void RoofTool::resizeRoof(int width, int height)
{
    if (width < 1 || height < 1)
        return;

    RoofObject *roof = mHandleObject;

    int oldWidth = roof->width(), oldHeight = roof->height();
    roof->resize(width, height);
    if (!roof->isValidPos()) {
        roof->resize(oldWidth, oldHeight);
        return;
    }

    mEditor->itemForObject(roof)->synchWithObject();
    mStartPos = roof->bounds().bottomRight();
}

void RoofTool::toggleCappedW()
{
    mEditor->document()->undoStack()->push(new HandleRoof(mEditor->document(),
                                                          mHandleObject,
                                                          HandleRoof::ToggleCappedW));
}

void RoofTool::toggleCappedN()
{
    mEditor->document()->undoStack()->push(new HandleRoof(mEditor->document(),
                                                          mHandleObject,
                                                          HandleRoof::ToggleCappedN));
}

void RoofTool::toggleCappedE()
{
    mEditor->document()->undoStack()->push(new HandleRoof(mEditor->document(),
                                                          mHandleObject,
                                                          HandleRoof::ToggleCappedE));
}

void RoofTool::toggleCappedS()
{
    mEditor->document()->undoStack()->push(new HandleRoof(mEditor->document(),
                                                          mHandleObject,
                                                          HandleRoof::ToggleCappedS));
}

void RoofTool::depthUp()
{
    if (mHandleObject->isDepthMax())
        return;
    mEditor->document()->undoStack()->push(new HandleRoof(mEditor->document(),
                                                          mHandleObject,
                                                          HandleRoof::IncrDepth));
}

void RoofTool::depthDown()
{
    if (mHandleObject->isDepthMin())
        return;
    mEditor->document()->undoStack()->push(new HandleRoof(mEditor->document(),
                                                          mHandleObject,
                                                          HandleRoof::DecrDepth));
}

/////

RoofCornerTool *RoofCornerTool::mInstance = 0;

RoofCornerTool *RoofCornerTool::instance()
{
    if (!mInstance)
        mInstance = new RoofCornerTool;
    return mInstance;
}

RoofCornerTool::RoofCornerTool()
    : RoofTool()
{
    setRoofType(RoofObject::CornerInnerNW);
}

/////

SelectMoveObjectTool *SelectMoveObjectTool::mInstance = 0;

SelectMoveObjectTool *SelectMoveObjectTool::instance()
{
    if (!mInstance)
        mInstance = new SelectMoveObjectTool;
    return mInstance;
}

SelectMoveObjectTool::SelectMoveObjectTool() :
    BaseTool(),
    mMode(NoMode),
    mMouseDown(false),
    mClickedObject(0),
    mSelectionRectItem(0)
{
    setStatusText(tr("Left-click to select.  Left-click-drag to move objects."));
}

void SelectMoveObjectTool::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (mMode != NoMode) // Ignore additional presses during select/move
            return;
        mMouseDown = true;
        mStartScenePos = event->scenePos();
        mClickedObject = mEditor->topmostObjectAt(mStartScenePos);
    }
    if (event->button() == Qt::RightButton) {
        if (mMode == Moving)
            cancelMoving();
    }
}

void SelectMoveObjectTool::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pos = event->scenePos();

    if (mMode == NoMode && mMouseDown) {
        const int dragDistance = (mStartScenePos - pos).manhattanLength();
        if (dragDistance >= QApplication::startDragDistance()) {
            if (mClickedObject)
                startMoving();
            else
                startSelecting();
        }
    }

    switch (mMode) {
    case Selecting:
        mSelectionRectItem->setRect(QRectF(mStartScenePos, pos).normalized());
        break;
    case Moving:
        updateMovingItems(pos, event->modifiers());
        break;
    case CancelMoving:
        break;
    case NoMode:
        break;
    }
}

void SelectMoveObjectTool::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return;

    switch (mMode) {
    case NoMode:
        if (mClickedObject) {
            QSet<BuildingObject*> selection = mEditor->document()->selectedObjects();
            const Qt::KeyboardModifiers modifiers = event->modifiers();
            if (modifiers & (Qt::ShiftModifier | Qt::ControlModifier)) {
                if (selection.contains(mClickedObject))
                    selection.remove(mClickedObject);
                else
                    selection.insert(mClickedObject);
            } else {
                selection.clear();
                selection.insert(mClickedObject);
            }
            mEditor->document()->setSelectedObjects(selection);
        } else {
            mEditor->document()->setSelectedObjects(QSet<BuildingObject*>());
        }
        break;
    case Selecting:
        updateSelection(event->scenePos(), event->modifiers());
        mEditor->removeItem(mSelectionRectItem);
        mMode = NoMode;
        break;
    case Moving:
        mMouseDown = false;
        finishMoving(event->scenePos());
        break;
    case CancelMoving:
        mMode = NoMode;
        break;
    }

    mMouseDown = false;
    mClickedObject = 0;
}

void SelectMoveObjectTool::documentChanged()
{
    mSelectionRectItem = 0;
}

void SelectMoveObjectTool::activate()
{
}

void SelectMoveObjectTool::deactivate()
{
}

void SelectMoveObjectTool::updateSelection(const QPointF &pos,
                                           Qt::KeyboardModifiers modifiers)
{
    QRectF rect = QRectF(mStartScenePos, pos).normalized();

    // Make sure the rect has some contents, otherwise intersects returns false
    rect.setWidth(qMax(qreal(1), rect.width()));
    rect.setHeight(qMax(qreal(1), rect.height()));

    QSet<BuildingObject*> selectedObjects;

    foreach (BuildingObject *object, mEditor->objectsInRect(rect))
        selectedObjects.insert(object);

    const QSet<BuildingObject*> oldSelection = mEditor->document()->selectedObjects();
    QSet<BuildingObject*> newSelection;

    if (modifiers & (Qt::ControlModifier | Qt::ShiftModifier)) {
        newSelection = oldSelection | selectedObjects;
    } else {
        newSelection = selectedObjects;
    }

    mEditor->document()->setSelectedObjects(newSelection);
}

void SelectMoveObjectTool::startSelecting()
{
    mMode = Selecting;
    if (mSelectionRectItem == 0) {
        mSelectionRectItem = new QGraphicsRectItem();
        mSelectionRectItem->setPen(QColor(0x33,0x99,0xff));
        mSelectionRectItem->setBrush(QBrush(QColor(0x33,0x99,0xff,255/8)));
        mSelectionRectItem->setZValue(FloorEditor::ZVALUE_CURSOR);
    }
    mEditor->addItem(mSelectionRectItem);
}

void SelectMoveObjectTool::startMoving()
{
    mMovingObjects = mEditor->document()->selectedObjects();

    // Move only the clicked item, if it was not part of the selection
    if (!mMovingObjects.contains(mClickedObject)) {
        mMovingObjects.clear();
        mMovingObjects.insert(mClickedObject);
        mEditor->document()->setSelectedObjects(mMovingObjects);
    }

    mMode = Moving;
    mDragOffset = QPoint();
}

void SelectMoveObjectTool::updateMovingItems(const QPointF &pos,
                                             Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    QPoint startTilePos = mEditor->sceneToTile(mStartScenePos);
    QPoint currentTilePos = mEditor->sceneToTile(pos);
    mDragOffset = currentTilePos - startTilePos;

    foreach (BuildingObject *object, mMovingObjects) {
        GraphicsObjectItem *item = mEditor->itemForObject(object);
        item->setDragging(true);
        item->setDragOffset(mDragOffset);
        item->setValidPos(object->isValidPos(mDragOffset));
    }
}

void SelectMoveObjectTool::finishMoving(const QPointF &pos)
{
    Q_UNUSED(pos)

    Q_ASSERT(mMode == Moving);
    mMode = NoMode;

    foreach (BuildingObject *object, mMovingObjects) {
        mEditor->itemForObject(object)->setDragging(false);
        mEditor->itemForObject(object)->setValidPos(true);
    }

    if (mDragOffset.isNull()) // Move is a no-op
        return;

    QUndoStack *undoStack = mEditor->document()->undoStack();
    undoStack->beginMacro(tr("Move %n Object(s)", "", mMovingObjects.size()));
    foreach (BuildingObject *object, mMovingObjects) {
        if (!object->isValidPos(mDragOffset))
            undoStack->push(new RemoveObject(mEditor->document(),
                                             object->floor(),
                                             object->floor()->indexOf(object)));
        else
            undoStack->push(new MoveObject(mEditor->document(), object,
                                           object->pos() + mDragOffset));
    }
    undoStack->endMacro();

    mMovingObjects.clear();
}

void SelectMoveObjectTool::cancelMoving()
{
    foreach (BuildingObject *object, mMovingObjects) {
        mEditor->itemForObject(object)->setDragging(false);
        mEditor->itemForObject(object)->setValidPos(true);
    }

    mMovingObjects.clear();

    mMode = CancelMoving;
}

/////
