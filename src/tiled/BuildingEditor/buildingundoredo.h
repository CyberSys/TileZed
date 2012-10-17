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

#ifndef BUILDINGUNDOREDO_H
#define BUILDINGUNDOREDO_H

#include <QPoint>
#include <QUndoCommand>
#include <QVector>

namespace BuildingEditor {

class BuildingDocument;
class BuildingFloor;
class Layout;
class Room;

enum {
    UndoCmd_PaintRoom = 1000,
    UndoCmd_EraseRoom = 1001
};

class ChangeRoomAtPosition : public QUndoCommand
{
public:
    ChangeRoomAtPosition(BuildingDocument *doc, BuildingFloor *floor,
                         const QPoint &pos, Room *room);

    void undo() { swap(); }
    void redo() { swap(); }

    bool mergeWith(const QUndoCommand *other);

    void setMergeable(bool mergeable)
    { mMergeable = mergeable; }

private:
    void swap();

    BuildingDocument *mDocument;
    BuildingFloor *mFloor;
    class Changed {
    public:
        QPoint mPosition;
        Room *mRoom;
    };
    QVector<Changed> mChanged;
    bool mMergeable;
};

class PaintRoom : public ChangeRoomAtPosition
{
public:
    PaintRoom(BuildingDocument *doc, BuildingFloor *floor,
              const QPoint &pos, Room *room);

    int id() const { return UndoCmd_PaintRoom; }
};

class EraseRoom : public ChangeRoomAtPosition
{
public:
    EraseRoom(BuildingDocument *doc, BuildingFloor *floor,
              const QPoint &pos);

    int id() const { return UndoCmd_EraseRoom; }
};

} // namespace BuildingEditor

#endif // BUILDINGUNDOREDO_H
