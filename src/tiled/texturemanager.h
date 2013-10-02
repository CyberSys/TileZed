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

#ifndef TEXTUREMANAGER_H
#define TEXTUREMANAGER_H

#include "BuildingEditor/singleton.h"

#include <QCoreApplication>
#include <QMap>
#include <QObject>

namespace Tiled {
class Tileset;

namespace Internal {

class TextureInfo
{
public:
    TextureInfo(const QString &name, const QString &fileName, int columnCount, int rowCount) :
        mName(name),
        mFileName(fileName),
        mColumnCount(columnCount),
        mRowCount(rowCount),
        mTileset(0)
    {}

    QString name() const { return mName; }
    QString fileName() const { return mFileName; }
    int columnCount() const { return mColumnCount; }
    int rowCount() const { return mRowCount; }

    void setTileset(Tileset *ts) { mTileset = ts; }
    Tileset *tileset() const { return mTileset; }

private:
    QString mName;
    QString mFileName;
    int mColumnCount;
    int mRowCount;

    Tileset *mTileset;
};

class TextureMgr : public QObject, public Singleton<TextureMgr>
{
    Q_OBJECT
public:
    explicit TextureMgr(QObject *parent = 0);
    ~TextureMgr();

    QString txtName() const { return QLatin1String("Textures.txt"); }
    QString txtPath() const;

    bool readTxt();
    bool writeTxt();

    QList<TextureInfo*> textures() const { return mTextureByName.values(); }
    TextureInfo *texture(const QString &name)
    { return mTextureByName.contains(name) ? mTextureByName[name] : 0; }

    void addTexture(TextureInfo *tex);
    void removeTexture(TextureInfo *tex);

    Tileset *tileset(TextureInfo *tex);

    QString errorString() const { return mError; }

signals:
    void textureAdded(TextureInfo *texture);
    void textureAboutToBeRemoved(TextureInfo *texture);
    void textureRemoved(TextureInfo *texture);

private:
    QMap<QString,TextureInfo*> mTextureByName;
    QList<TextureInfo*> mRemovedTextures;
    QString mError;
};

class TexturesFile
{
    Q_DECLARE_TR_FUNCTIONS(TexturesFile)
public:
    TexturesFile();
    ~TexturesFile();

    bool read(const QString &fileName);
    bool write(const QString &fileName, const QList<TextureInfo*> &textures);

    QList<TextureInfo*> takeTextures()
    {
        QList<TextureInfo*> ret = mTextures;
        mTextures.clear();
        return ret;
    }

    QString errorString() const { return mError; }

private:
    bool parse2Ints(const QString &s, int *pa, int *pb);

private:
    QList<TextureInfo*> mTextures;
    QMap<QString,TextureInfo*> mTextureByName;
    QString mError;
};

} // namespace Internal
} // namespace Tiled

#endif // TEXTUREMANAGER_H
