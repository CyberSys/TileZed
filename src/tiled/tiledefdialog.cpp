/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
 *
 * This file is part of Tiled.
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

#include "tiledefdialog.h"
#include "ui_tiledefdialog.h"

#include "tiledeffile.h"
#include "tilesetmanager.h"
#include "zoomable.h"
#include "utils.h"

#include "tile.h"
#include "tileset.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolBar>
#include <QToolButton>
#include <QUndoGroup>
#include <QUndoStack>

using namespace Tiled;
using namespace Tiled::Internal;

/////

namespace TileDefUndoRedo {

class AddGlobalTileset : public QUndoCommand
{
public:
    AddGlobalTileset(TileDefDialog *d, Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Add Tileset")),
        mDialog(d),
        mTileset(tileset)
    {
    }

    void undo()
    {
        mDialog->removeTileset(mTileset);
    }

    void redo()
    {
        mDialog->addTileset(mTileset);
    }

    TileDefDialog *mDialog;
    Tileset *mTileset;
};

class RemoveGlobalTileset : public QUndoCommand
{
public:
    RemoveGlobalTileset(TileDefDialog *d, Tileset *tileset) :
        QUndoCommand(QCoreApplication::translate("UndoCommands", "Remove Tileset")),
        mDialog(d),
        mTileset(tileset)
    {
    }

    void undo()
    {
        mDialog->addTileset(mTileset);
    }

    void redo()
    {
        mDialog->removeTileset(mTileset);
    }

    TileDefDialog *mDialog;
    Tileset *mTileset;
};

} // namespace

using namespace TileDefUndoRedo;

TileDefDialog *TileDefDialog::mInstance = 0;

TileDefDialog *TileDefDialog::instance()
{
    if (!mInstance)
        mInstance = new TileDefDialog;
    return mInstance;
}

void TileDefDialog::deleteInstance()
{
    delete mInstance;
    mInstance = 0;
}

TileDefDialog::TileDefDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::TileDefDialog),
    mCurrentTileset(0),
    mZoomable(new Zoomable(this)),
    mSynching(false),
    mUpdatePending(false),
    mTileDefFile(0),
    mTileDefProperties(new TileDefProperties),
    mUndoGroup(new QUndoGroup(this)),
    mUndoStack(new QUndoStack(this))
{
    ui->setupUi(this);

    /////

    QAction *undoAction = mUndoGroup->createUndoAction(this, tr("Undo"));
    QAction *redoAction = mUndoGroup->createRedoAction(this, tr("Redo"));
    QIcon undoIcon(QLatin1String(":images/16x16/edit-undo.png"));
    QIcon redoIcon(QLatin1String(":images/16x16/edit-redo.png"));
    mUndoGroup->addStack(mUndoStack);
    mUndoGroup->setActiveStack(mUndoStack);

    QToolButton *button = new QToolButton(this);
    button->setIcon(undoIcon);
    Utils::setThemeIcon(button, "edit-undo");
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setText(undoAction->text());
    button->setEnabled(mUndoGroup->canUndo());
    button->setShortcut(QKeySequence::Undo);
    mUndoButton = button;
    ui->buttonLayout->insertWidget(0, button);
    connect(mUndoGroup, SIGNAL(canUndoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), undoAction, SIGNAL(triggered()));

    button = new QToolButton(this);
    button->setIcon(redoIcon);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Utils::setThemeIcon(button, "edit-redo");
    button->setText(redoAction->text());
    button->setEnabled(mUndoGroup->canRedo());
    button->setShortcut(QKeySequence::Redo);
    mRedoButton = button;
    ui->buttonLayout->insertWidget(1, button);
    connect(mUndoGroup, SIGNAL(canRedoChanged(bool)), button, SLOT(setEnabled(bool)));
    connect(button, SIGNAL(clicked()), redoAction, SIGNAL(triggered()));

    connect(mUndoGroup, SIGNAL(undoTextChanged(QString)), SLOT(undoTextChanged(QString)));
    connect(mUndoGroup, SIGNAL(redoTextChanged(QString)), SLOT(redoTextChanged(QString)));

    /////

    ui->splitter->setStretchFactor(0, 1);

    mZoomable->setScale(0.5); // FIXME
    mZoomable->connectToComboBox(ui->scaleComboBox);
    ui->tiles->setZoomable(mZoomable);
    ui->tiles->model()->setShowHeaders(false);

    ui->tiles->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tiles->model()->setShowHeaders(false);
    ui->tiles->model()->setShowLabels(true);
    ui->tiles->model()->setHighlightLabelledItems(true);

    connect(ui->tilesets, SIGNAL(currentRowChanged(int)),
            SLOT(currentTilesetChanged(int)));
    connect(ui->tiles->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(tileSelectionChanged()));

    connect(ui->actionNew, SIGNAL(triggered()), SLOT(fileNew()));
    connect(ui->actionOpen, SIGNAL(triggered()), SLOT(fileOpen()));
    connect(ui->actionSave, SIGNAL(triggered()), SLOT(fileSave()));
    connect(ui->actionAddTileset, SIGNAL(triggered()), SLOT(addTileset()));

    foreach (TileDefProperty *prop, mTileDefProperties->mProperties) {
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            if (QCheckBox *w = ui->propertySheet->findChild<QCheckBox*>(p->mName)) {
                connect(w, SIGNAL(toggled(bool)), SLOT(checkboxToggled(bool)));
                mCheckBoxes[p->mName] = w;
            }
            else
                qDebug() << "missing QCheckBox for property" << prop->mName;
            continue;
        }
        if (IntegerTileDefProperty *p = prop->asInteger()) {
            if (QSpinBox *w = ui->propertySheet->findChild<QSpinBox*>(p->mName)) {
                connect(w, SIGNAL(valueChanged(int)), SLOT(spinBoxValueChanged(int)));
                mSpinBoxes[p->mName] = w;
            }
            else
                qDebug() << "missing QSpinBox for property" << prop->mName;
            continue;
        }
        if (StringTileDefProperty *p = prop->asString()) {
            if (QComboBox *w = ui->propertySheet->findChild<QComboBox*>(p->mName)) {
                mComboBoxes[p->mName] = w;
            }
            else
                qDebug() << "missing QComboBox for property" << prop->mName;
            continue;
        }
        if (EnumTileDefProperty *p = prop->asEnum()) {
            if (QComboBox *w = ui->propertySheet->findChild<QComboBox*>(p->mName)) {
                w->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                w->addItems(p->mEnums);
                w->installEventFilter(this); // to disable mousewheel
                connect(w, SIGNAL(activated(int)), SLOT(comboBoxActivated(int)));
                mComboBoxes[p->mName] = w;
            }
            else
                qDebug() << "missing QComboBox for property" << prop->mName;
            continue;
        }
    }
#if 0
    // Hack - force the tileset-names-list font to be updated now, because
    // setTilesetList() uses its font metrics to determine the maximum item
    // width.
    ui->tilesets->setFont(QFont());
    setTilesetList();
#endif

    QLabel label;
    mLabelFont = label.font();
    mBoldLabelFont = mLabelFont;
    mBoldLabelFont.setBold(true);

    fileOpen(QLatin1String("C:\\Users\\Tim\\Desktop\\ProjectZomboid\\maptools\\tiledefinitions.tiles"));
    setTilesetList();

    updateUI();
}

TileDefDialog::~TileDefDialog()
{
    delete ui;

    TilesetManager::instance()->removeReferences(mTilesets.values());
    TilesetManager::instance()->removeReferences(mRemovedTilesets);
    delete mTileDefFile;
}

void TileDefDialog::addTileset()
{ 
    const QString tilesDir = mTileDefFile->directory();
    const QString filter = Utils::readableImageFormatsFilter();
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
                                                          tr("Tileset Image"),
                                                          tilesDir,
                                                          filter);

    mUndoStack->beginMacro(tr("Add Tilesets"));

    foreach (QString f, fileNames) {
        QFileInfo info(f);
        if (Tiled::Tileset *ts = loadTileset(info.canonicalFilePath())) {
            QString name = info.completeBaseName();
            // Replace any current tileset with the same name as an existing one.
            // This will NOT replace the meta-info for the old tileset, it will
            // be used by the new tileset as well.
            if (Tileset *old = tileset(name))
                mUndoStack->push(new RemoveGlobalTileset(this, old));
            mUndoStack->push(new AddGlobalTileset(this, ts));
        } else {
            QMessageBox::warning(this, tr("It's no good, Jim!"), mError);
        }
    }

    mUndoStack->endMacro();
}

void TileDefDialog::removeTileset()
{
    QList<QListWidgetItem*> selection = ui->tilesets->selectedItems();
    QListWidgetItem *item = selection.count() ? selection.first() : 0;
    if (item) {
        int row = ui->tilesets->row(item);
        Tileset *tileset = this->tileset(row);
        if (QMessageBox::question(this, tr("Remove Tileset"),
                                  tr("Really remove the tileset '%1'?\nYou will lose all the properties for this tileset!")
                                  .arg(tileset->name()),
                                  QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel)
            return;
        mUndoStack->push(new RemoveGlobalTileset(this, tileset));
    }
}

void TileDefDialog::addTileset(Tileset *ts)
{
    mTilesets[ts->name()] = ts;
    if (!mRemovedTilesets.contains(ts))
        TilesetManager::instance()->addReference(ts);
    mRemovedTilesets.removeAll(ts);

    updateTilesetListLater();
}

void TileDefDialog::removeTileset(Tileset *ts)
{
//    int row = indexOf(ts->name());

    mTilesets.remove(ts->name());
    // Don't remove references now, that will delete the tileset, and the
    // user might undo the removal.
    mRemovedTilesets += ts;

    updateTilesetListLater();
//    setTilesetList();
//    ui->tilesets->setCurrentRow(row);
}

void TileDefDialog::fileNew()
{
}

void TileDefDialog::fileOpen()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Choose .tiles file"),
                                                    QString(), QLatin1String("Tile properties files (*.tiles)"));
    if (fileName.isEmpty())
        return;

    delete mTileDefFile;
    mTileDefFile = 0;
    foreach (Tileset *ts, mTilesets)
        removeTileset(ts);

    fileOpen(fileName);

    setTilesetList();
    updateUI();
}

void TileDefDialog::fileSave()
{
}

void TileDefDialog::currentTilesetChanged(int row)
{
    mCurrentTileset = 0;
    if (row >= 0)
        mCurrentTileset = tileset(row);

    setTilesList();
    updateUI();
}

void TileDefDialog::tileSelectionChanged()
{
    mSelectedTiles.clear();

    QModelIndexList selection = ui->tiles->selectionModel()->selectedIndexes();
    foreach (QModelIndex index, selection) {
        if (Tile *tile = ui->tiles->model()->tileAt(index))
            mSelectedTiles += tile;
    }

    setPropertiesPage();
    updateUI();
}

void TileDefDialog::comboBoxActivated(int index)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QComboBox *w = dynamic_cast<QComboBox*>(sender);
    if (!w) return;

    EnumTileDefProperty *p = 0;
    TileDefProperties props;
    foreach (TileDefProperty *prop, props.mProperties) {
        if (prop->mName == w->objectName()) {
            p = prop->asEnum();
            break;
        }
    }
    if (!p)
        return;

    foreach (Tile *tile, mSelectedTiles) {
        TileDefTileset *defTileset = mTileDefFile->tileset(tile->tileset()->name()); // TODO: defTile->tileset()
        TileDefTile *defTile = defTileset->mTiles[tile->id()];
        defTile->mPropertyUI.ChangeProperties(p->mName, index); // FIXME: undo/redo
        setToolTipEtc(tile->id());
    }

    // FIXME: just redisplay what's needed
    setPropertiesPage();
}

void TileDefDialog::checkboxToggled(bool value)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QCheckBox *w = dynamic_cast<QCheckBox*>(sender);
    if (!w) return;

    BooleanTileDefProperty *p = 0;
    TileDefProperties props;
    foreach (TileDefProperty *prop, props.mProperties) {
        if (prop->mName == w->objectName()) {
            p = prop->asBoolean();
            break;
        }
    }
    if (!p)
        return;

    foreach (Tile *tile, mSelectedTiles) {
        TileDefTileset *defTileset = mTileDefFile->tileset(tile->tileset()->name()); // TODO: defTile->tileset()
        TileDefTile *defTile = defTileset->mTiles[tile->id()];
        defTile->mPropertyUI.ChangeProperties(p->mName, value); // FIXME: undo/redo
        setToolTipEtc(tile->id());
    }

    // FIXME: just redisplay what's needed
    setPropertiesPage();
}

void TileDefDialog::spinBoxValueChanged(int value)
{
    if (mSynching)
        return;

    QObject *sender = this->sender();
    if (!sender) return;
    QSpinBox *w = dynamic_cast<QSpinBox*>(sender);
    if (!w) return;

    IntegerTileDefProperty *p = 0;
    TileDefProperties props;
    foreach (TileDefProperty *prop, props.mProperties) {
        if (prop->mName == w->objectName()) {
            p = prop->asInteger();
            break;
        }
    }
    if (!p)
        return;

    foreach (Tile *tile, mSelectedTiles) {
        TileDefTileset *defTileset = mTileDefFile->tileset(tile->tileset()->name()); // TODO: defTile->tileset()
        TileDefTile *defTile = defTileset->mTiles[tile->id()];
        defTile->mPropertyUI.ChangeProperties(p->mName, value); // FIXME: undo/redo
        setToolTipEtc(tile->id());
    }

    // FIXME: just redisplay what's needed
    setPropertiesPage();
}

void TileDefDialog::undoTextChanged(const QString &text)
{
    mUndoButton->setToolTip(text);
}

void TileDefDialog::redoTextChanged(const QString &text)
{
    mRedoButton->setToolTip(text);
}

void TileDefDialog::updateTilesetList()
{
    mUpdatePending = false;
    loadTilesets();
    setTilesetList();
//    int row = indexOf(ts->name());
//    ui->tilesets->setCurrentRow(row);

    updateUI();
}

void TileDefDialog::updateUI()
{
    mSynching = true;

    bool hasFile = mTileDefFile != 0;
    ui->actionSave->setEnabled(hasFile);
    ui->actionAddTileset->setEnabled(hasFile);

    mSynching = false;
}

void TileDefDialog::accept()
{
}

bool TileDefDialog::eventFilter(QObject *object, QEvent *event)
{
    if ((event->type() == QEvent::Wheel) && object->parent()
            && object->parent()->parent() == ui->propertySheet) {
        qDebug() << "Wheel event blocked";
        return true;
    }
    return false;
}

void TileDefDialog::fileOpen(const QString &fileName)
{
    TileDefFile *defFile = new TileDefFile;
    if (!defFile->read(fileName)) {
        QMessageBox::warning(this, tr("Eror reading .tiles file"),
                             defFile->errorString());
        delete defFile;
        return;
    }

    mTileDefFile = defFile;
    foreach (QString tilesetName, mTileDefFile->tilesetNames()) {
        TileDefTileset *tsDef = mTileDefFile->tileset(tilesetName);

        // Try to reuse a tileset from our list of removed tilesets.
        bool reused = false;
        foreach (Tileset *ts, mRemovedTilesets) {
            if (ts->imageSource() == tsDef->mImageSource) {
                addTileset(ts);
                reused = true;
                break;
            }
        }
        if (reused)
            continue;

        Tileset *tileset = new Tileset(tilesetName, 64, 128);
        int width = tsDef->mColumns * 64, height = tsDef->mRows * 128;
        QImage image(width, height, QImage::Format_ARGB32);
        image.fill(Qt::red);
        tileset->loadFromImage(image, tsDef->mImageSource);
        Tile *missingTile = TilesetManager::instance()->missingTile();
        for (int i = 0; i < tileset->tileCount(); i++)
            tileset->tileAt(i)->setImage(missingTile->image());
        tileset->setMissing(true);

        addTileset(tileset);
    }
}

void TileDefDialog::updateTilesetListLater()
{
    if (!mUpdatePending) {
        QMetaObject::invokeMethod(this, "updateTilesetList", Qt::QueuedConnection);
        mUpdatePending = true;
    }
}

void TileDefDialog::setTilesetList()
{
    if (mUpdatePending)
        return;

    mCurrentTileset = 0;
    mSelectedTiles.clear();

    QFontMetrics fm = ui->tilesets->fontMetrics();
    int maxWidth = 128;

    ui->tilesets->clear();
    foreach (Tileset *ts, mTilesets.values()) {
        QListWidgetItem *item = new QListWidgetItem(ts->name());
        if (ts->isMissing())
            item->setForeground(Qt::red);
        ui->tilesets->addItem(item);
        maxWidth = qMax(maxWidth, fm.width(ts->name()));
    }
    ui->tilesets->setFixedWidth(maxWidth + 16 +
        ui->tilesets->verticalScrollBar()->sizeHint().width());
}

void TileDefDialog::setTilesList()
{
    mSelectedTiles.clear();

    if (mCurrentTileset) {
        ui->tiles->model()->setTileset(mCurrentTileset);

        // Tooltip shows properties with non-default value.
        for (int i = 0; i < mCurrentTileset->tileCount(); i++) {
            setToolTipEtc(i);
        }
    } else {
        ui->tiles->model()->setTiles(QList<Tile*>());
    }

    tileSelectionChanged(); // model calling reset() doesn't generate selectionChanged signal
}

void TileDefDialog::setToolTipEtc(int tileID)
{
    TileDefTileset *defTileset = mTileDefFile->tileset(mCurrentTileset->name());
    if (!defTileset)
        return;
    TileDefTile *defTile = defTileset->mTiles[tileID];
    QStringList tooltip;
    foreach (UIProperties::UIProperty *p, defTile->mPropertyUI.nonDefaultProperties())
        tooltip += tr("%1 = %2").arg(p->mName).arg(p->valueAsString());
    if (defTile->mProperties.size() && tooltip.isEmpty())
        qDebug() << defTile->mProperties;
    ui->tiles->model()->setToolTip(tileID, tooltip.join(QLatin1String("\n")));
    QRect r;
    if (tooltip.size())
        r = QRect(0,0,1,1);
    ui->tiles->model()->setCategoryBounds(tileID, r);
    ui->tiles->update(ui->tiles->model()->index(mCurrentTileset->tileAt(tileID)));
}

void TileDefDialog::setPropertiesPage()
{
    mSynching = true;

    TileDefProperties *props = mTileDefProperties;
    TileDefTile *defTile = 0;
    if (mSelectedTiles.size()) {
        if (TileDefTileset *defTileset = mTileDefFile->tileset(mCurrentTileset->name())) {
            defTile = defTileset->mTiles[mSelectedTiles[0]->id()];
        }
    }
    foreach (TileDefProperty *prop, props->mProperties) {
        if (BooleanTileDefProperty *p = prop->asBoolean()) {
            if (QCheckBox *w = mCheckBoxes[p->mName]) {
                bool checked = p->mDefault;
                if (defTile) {
                    checked = defTile->getBoolean(p->mName);
                }
                w->setChecked(checked);
                setBold(w, checked != p->mDefault);
            }
            continue;
        }
        if (IntegerTileDefProperty *p = prop->asInteger()) {
            if (QSpinBox *w = mSpinBoxes[p->mName]) {
                int value = p->mDefault;
                if (defTile) {
                    value = defTile->getInteger(p->mName);
                }
                w->setValue(value);
                setBold(w, value != p->mDefault);
            }
            continue;
        }
        if (StringTileDefProperty *p = prop->asString()) {
            if (QComboBox *w = mComboBoxes[p->mName]) {
                QString value = p->mDefault;
                if (defTile) {
                    value = defTile->getString(p->mName);
                }
                w->setEditText(value);
                setBold(w, value != p->mDefault);
            }
            continue;
        }
        if (EnumTileDefProperty *p = prop->asEnum()) {
            if (QComboBox *w = mComboBoxes[p->mName]) {
                int index = 0;
                if (defTile) {
                    index = defTile->getEnum(p->mName);
                }
                w->setCurrentIndex(index);
                setBold(w, index != 0 /*p->mDefault*/);
            }
            continue;
        }
    }

    mSynching = false;
}

void TileDefDialog::setBold(QWidget *w, bool bold)
{
    if (mLabelFont == w->font()) {
        if (!bold) return;
        w->setFont(mBoldLabelFont);
    } else {
        if (bold) return;
        w->setFont(mLabelFont);
    }

    foreach (QObject *o, w->parent()->children()) {
        if (QLabel *label = dynamic_cast<QLabel*>(o)) {
            if (label->buddy() == w) {
                label->setFont(w->font());
                break;
            }
        }
    }
}

Tileset *TileDefDialog::tileset(const QString &name) const
{
    if (mTilesets.contains(name))
        return mTilesets[name];
    return 0;
}

Tileset *TileDefDialog::tileset(int index) const
{
    if (index >= 0 && index < mTilesets.size())
        return mTilesets.values().at(index);
    return 0;
}

int TileDefDialog::indexOf(const QString &name) const
{
    return tilesetNames().indexOf(name);
}

void TileDefDialog::loadTilesets()
{
    foreach (Tileset *ts, mTilesets.values()) {
        if (ts->isMissing()) {
            QString source = ts->imageSource();
            QString oldSource = source;
            if (QDir::isRelativePath(ts->imageSource())) {
                source = mTileDefFile->directory() + QLatin1Char('/')
                        + ts->imageSource();
            }
            QFileInfo info(source);
            if (info.exists()) {
                source = info.canonicalFilePath();
                if (loadTilesetImage(ts, source)) {
                    ts->setMissing(false); // Yay!
                    TilesetManager::instance()->tilesetSourceChanged(ts, oldSource);
                }
            }
        }
    }
}

Tileset *TileDefDialog::loadTileset(const QString &source)
{
    QFileInfo info(source);
    Tileset *ts = new Tileset(info.completeBaseName(), 64, 128);
    if (!loadTilesetImage(ts, source)) {
        delete ts;
        return 0;
    }
    return ts;
}

bool TileDefDialog::loadTilesetImage(Tileset *ts, const QString &source)
{
    TilesetImageCache *cache = TilesetManager::instance()->imageCache();
    Tileset *cached = cache->findMatch(ts, source);
    if (!cached || !ts->loadFromCache(cached)) {
        const QImage tilesetImage = QImage(source);
        if (ts->loadFromImage(tilesetImage, source))
            cache->addTileset(ts);
        else {
            mError = tr("Error loading tileset image:\n'%1'").arg(source);
            return 0;
        }
    }

    return ts;
}
