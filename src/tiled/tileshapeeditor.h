#ifndef TILESHAPEEDITOR_H
#define TILESHAPEEDITOR_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QDialog>
#include <QSet>
#include <QTimer>
#include <QVector3D>

class QUndoStack;

namespace Ui {
class TileShapeEditor;
}

namespace Tiled {
namespace Internal {

class TileShape;
class TileShapeXform;
class TileShapeEditor;
class TileShapeFace;
class TileShapeScene;
class Zoomable;

class TileShapeGrid : public QGraphicsItem
{
public:
    TileShapeGrid(TileShapeScene *scene, QGraphicsItem *parent = 0);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setGridZ(qreal z);
    void setGridSize(int x, int y, int z);
    int gridSizeX() const { return mGridSize.x; }
    int gridSizeY() const { return mGridSize.y; }
    int gridSizeZ() const { return mGridSize.z; }

    QVector3D snapXY(const QVector3D &v);
    QVector3D snapZ(const QVector3D &v);
    qreal snapZ(qreal z);

private:
    TileShapeScene *mScene;
    struct GridSize {
        GridSize(int x, int y, int z) : x(x), y(y), z(z) {}
        int x;
        int y;
        int z;
    } mGridSize;
    qreal mZ;
};

class TileShapeItem : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    TileShapeItem(TileShapeScene *scene, TileShape *shape, QGraphicsItem *parent = 0);

    TileShape *tileShape() const { return mShape; }

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setSelectedFace(int faceIndex);
    int selectedFace() const { return mSelectedFace; }

    void setCursorPoint(const QVector3D &pt, bool replace);
    void clearCursorPoint();
    QVector3D cursorPoint() const { return mCursorPoint; }

    void shapeChanged();

signals:
    void selectionChanged(int faceIndex);

private:
    TileShapeScene *mScene;
    TileShape *mShape;
    QRectF mBoundingRect;
    int mSelectedFace;
    QVector3D mCursorPoint;
    bool mCursorPointReplace;
    bool mHasCursorPoint;
};

class BaseTileShapeTool : public QObject
{
    Q_OBJECT

public:
    BaseTileShapeTool(TileShapeScene *scene);

    virtual void activate() {}
    virtual void deactivate() {}

    virtual void mousePressEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event) = 0;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) = 0;

    QAction *action() { return mAction; }

    void setEnabled(bool enabled);

    TileShapeEditor *editor() const;
    QUndoStack *undoStack() const;

    virtual void shapeChanged() {};

signals:
    void statusTextChanged(const QString &text);
    void enabledChanged(bool enabled);

protected:
    TileShapeScene *mScene;
    QAction *mAction;

    QGraphicsItemGroup *mCursorGroupXY;
    QGraphicsItemGroup *mCursorGroupZ;
};

class CreateTileShapeFaceTool : public BaseTileShapeTool
{
public:
    CreateTileShapeFaceTool(TileShapeScene *scene);

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    TileShapeItem *mShapeItem;
    QGraphicsLineItem *mCursorItemX, *mCursorItemY;

    enum Mode {
        NoMode,
        SetXY,
        SetZ
    };
    Mode mMode;
};

class TileShapeHandle : public QGraphicsItem
{
public:
    TileShapeHandle(TileShapeScene *scene, int faceIndex, int pointIndex);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setSelected(bool selected);
    bool isSelected() const { return mSelected; }

    int faceIndex() const { return mFaceIndex; }
    int pointIndex() const { return mPointIndex; }

    QVector3D tilePos() const;
    QPointF uv() const;

    void setDragOrigin(const QVector3D &pos) { mDragOrigin = pos; }
    QVector3D dragOrigin() const { return mDragOrigin; }
    void setDragOffset(const QVector3D &delta);

    void setUV(const QPointF &uv);

private:
    TileShapeScene *mScene;
    int mFaceIndex;
    int mPointIndex;
    bool mSelected;
    QVector3D mDragOrigin;
};

class EditTileShapeFaceTool : public BaseTileShapeTool
{
public:
    EditTileShapeFaceTool(TileShapeScene *scene);

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void shapeChanged() { if (!mFinishMoving) updateHandles(); }

    void updateHandles();

    enum Mode {
        NoMode,
        Selecting,
        MoveXY,
        MoveZ
    };

    void startMoving();
    void updateMovingItems(const QPointF &scenePos);
    void finishMoving(const QPointF &scenePos);
    void cancelMoving();
    void startSelecting();
    void setSelectedHandles(const QSet<TileShapeHandle *> &handles);

    Mode mMode;
    QList<TileShapeHandle*> mHandles;
    QSet<TileShapeHandle*> mSelectedHandles;
    QPointF mStartScenePos;
    TileShapeHandle *mClickedHandle;
    QGraphicsLineItem *mCursorItemX, *mCursorItemY;
    QPointF mDragOffsetXY;
    bool mFinishMoving;
};

class TileShapeUVGuide : public QObject, public QGraphicsItem
{
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
public:
    TileShapeUVGuide(TileShapeScene *scene);

    QRectF boundingRect() const;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

    void setGridSize(int x, int y) { mGridSize.x = x; mGridSize.y = y; update(); }
    int gridSizeX() const { return mGridSize.x; }
    int gridSizeY() const { return mGridSize.y; }

    QPointF toUV(const QPointF &scenePos);
    void setCurrentUV(const QPointF &uv);
    void setCursorUV(const QPointF &uv);

private slots:
    void blink();

public:
    TileShapeScene *mScene;
    QPointF mCurrentUV;
    QPointF mCursorUV;
    QImage mTexture;
    struct GridSize {
        GridSize(int x, int y) : x(x), y(y) {}
        int x;
        int y;
    } mGridSize;

    QTimer mBlink;
    QGraphicsRectItem *mBlinkItem;
    QColor mBlinkColor;
};

class TileShapeUVTool : public BaseTileShapeTool
{
    Q_OBJECT
public:
    TileShapeUVTool(TileShapeScene *scene);

    void activate();
    void deactivate();

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void shapeChanged() { updateHandles(); }

    void updateHandles();
    void setSelectedHandles(const QSet<TileShapeHandle *> &handles);

signals:
    void showUVGrid(bool show);

public:
    TileShapeUVGuide *mGuide;
    TileShapeHandle *mClickedHandle;
    QSet<TileShapeHandle*> mSelectedHandles;
    QList<TileShapeHandle*> mHandles;

    enum Mode {
        NoMode,
        SetUV
    };
    Mode mMode;
};

class TileShapeScene : public QGraphicsScene
{
    Q_OBJECT
public:
    TileShapeScene(QObject *parent = 0);

    void setTileShape(TileShape *shape);

    TileShapeItem *tileShapeItem() const { return mShapeItem; }
    TileShape *tileShape() const { return mShapeItem->tileShape(); }

    TileShapeGrid *gridItem() const { return mGridItem; }

    static QPointF toScene(qreal x, qreal y, qreal z);
    static QPointF toScene(const QVector3D &v);
    QPolygonF toScene(const QRectF &tileRect, qreal z) const;
    QPolygonF toScene(const QVector<QVector3D> &tilePoly) const;

    static QVector3D toTile(qreal x, qreal y, qreal z);
    static QVector3D toTile(const QPointF &scenePos, qreal z);
    static QVector<QVector3D> toTile(const QPolygonF &scenePoly, qreal z);

    QRectF boundingRect(const QRectF &rect, qreal z) const;
    QRectF boundingRect(TileShape *shape) const;

    int topmostFaceAt(const QPointF &scenePos, int *indexPtr);

    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

    void activateTool(BaseTileShapeTool *tool);
    BaseTileShapeTool *activeTool() { return mActiveTool; }

    void setEditor(TileShapeEditor *editor) { mEditor = editor; }
    TileShapeEditor *editor() const { return mEditor; }

private:
    TileShapeGrid *mGridItem;
    TileShapeItem *mShapeItem;
    BaseTileShapeTool *mActiveTool;
    TileShapeEditor *mEditor;
};

class TileShapeView : public QGraphicsView
{
    Q_OBJECT
public:
    TileShapeView(QWidget *parent = 0);

    TileShapeScene *scene() { return mScene; }

    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

    void wheelEvent(QWheelEvent *event);

    void setHandScrolling(bool handScrolling);

    Zoomable *zoomable() const { return mZoomable; }

private slots:
    void adjustScale(qreal scale);

private:
    TileShapeScene *mScene;
    QPoint mLastMousePos;
    QPointF mLastMouseScenePos;
    bool mHandScrolling;
    Zoomable *mZoomable;
};

class TileShapeEditor : public QDialog
{
    Q_OBJECT

public:
    explicit TileShapeEditor(TileShape *shape, QImage texture, QWidget *parent = 0);
    ~TileShapeEditor();

    TileShape *tileShape() const;

    QUndoStack *undoStack() const { return mUndoStack; }

    // Undo/Redo
    void insertFace(int index, const TileShapeFace &face);
    void removeFace(int index);
    QVector3D changeXYZ(int faceIndex, int pointIndex, const QVector3D &v);
    QPointF changeUV(int faceIndex, int pointIndex, const QPointF &uv);
    void insertXform(int index, const TileShapeXform &xform);
    void removeXform(int index);
    void changeXform(int index, TileShapeXform &xform);
    void changeShape(TileShape &other);
    //

public slots:
    void toolActivated(bool active);
    void toolEnabled();

    void setGridSizeX(int value);
    void setGridSizeY(int value);
    void setGridSizeZ(int value);
    void setGridSize(int x, int y, int z);
    void setUVGridSize(int x, int y);

    void setGridLock(bool lock);
    void faceSelectionChanged(int faceIndex);
    void removeFace();

    void sameAsChanged();

    void addRotate();
    void addTranslate();
    void moveXformUp();
    void moveXformDown();
    void removeTransform();

    void xformSelectionChanged();
    void xformXChanged(double value);
    void xformYChanged(double value);
    void xformZChanged(double value);

    void showUVGrid(bool visible);

    void updateActions();

    void done(int r);

private:
    void xformChanged(double x, double y, double z, int xyz);
    void setXformList();
    QString xformItemText(int index);
    void syncWithGridSize();

private:
    Ui::TileShapeEditor *ui;
    bool mToolChanging;
    QList<BaseTileShapeTool*> mTools;
    CreateTileShapeFaceTool *mCreateFaceTool;
    EditTileShapeFaceTool *mEditFaceTool;
    TileShapeUVTool *mUVTool;
    QList<TileShape*> mSameAsShapes;
    bool mShowUVGrid;
    bool mGridLock;
    bool mUVGridLock;
    int mSync;

    QUndoStack *mUndoStack;
};

} // namespace Internal
} // namespace Tiled

#endif // TILESHAPEEDITOR_H
