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

#ifndef SIMPLEFILE_H
#define SIMPLEFILE_H

#include <QString>
#include <QTextStream>

class SimpleFileKeyValue
{
public:
    SimpleFileKeyValue()
    {

    }

    SimpleFileKeyValue(const QString &name, const QString &value) :
        name(name),
        value(value)
    {

    }

    QString name;
    QString value;
};

class SimpleFileBlock
{
public:
    QString name;
    QList<SimpleFileKeyValue> values;
    QList<SimpleFileBlock> blocks;

    QString value(const char *key)
    { return value(QLatin1String(key)); }

    QString value(const QString &key);

    SimpleFileBlock block(const char *name)
    { return block(QLatin1String(name)); }

    SimpleFileBlock block(const QString &name);

    void print();
};


class SimpleFile : public SimpleFileBlock
{
public:
    SimpleFile();

    bool read(const QString &filePath);

    bool write(const QString &filePath);

private:
    SimpleFileBlock readBlock(QTextStream &ts);
    void writeBlock(QTextStream &ts, const SimpleFileBlock &block);

    class INDENT
    {
    public:
        INDENT(int &depth) :
            mDepth(depth)
        {
            ++mDepth;
        }
        ~INDENT()
        {
            --mDepth;
        }

        QString text() const
        { return QString(QLatin1Char(' ')).repeated(mDepth * 4); }

    private:
        int &mDepth;
    };

    int mIndent;
};

#endif // SIMPLEFILE_H
