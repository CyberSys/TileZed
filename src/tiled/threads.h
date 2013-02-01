/*
 * Copyright 2013, Tim Baker <treectrl@users.sf.net>
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

#ifndef THREADS_H
#define THREADS_H

#include <QThread>

class BaseWorker : public QObject
{
    Q_OBJECT

public:
    BaseWorker() :
        QObject(0),
        mAbortPtr(0)
    {

    }
    BaseWorker(bool *abortPtr) :
        QObject(0),
        mAbortPtr(abortPtr)
    {

    }
    virtual ~BaseWorker()
    {
    }

    bool aborted() { return mAbortPtr && *mAbortPtr; }

public slots:
    virtual void work() = 0;

signals:
    void finished();

private:
    bool *mAbortPtr;
};

class InterruptibleThread : public QThread
{
public:
    InterruptibleThread() :
        QThread(),
        mInterrupted(false)
    {

    }

    void interrupt() { mInterrupted = true; }
    void resume() { mInterrupted = false; }

    bool *var() { return &mInterrupted; }

private:
    bool mInterrupted;
};

class Sleep : public QThread
{
public:
    static void sleep(unsigned long secs) {
        QThread::sleep(secs);
    }
    static void msleep(unsigned long msecs) {
        QThread::msleep(msecs);
    }
    static void usleep(unsigned long usecs) {
        QThread::usleep(usecs);
    }
};

#include <QCoreApplication>
#define IN_APP_THREAD Q_ASSERT(QThread::currentThread() == qApp->thread());
#define IN_WORKER_THREAD Q_ASSERT(QThread::currentThread() != qApp->thread());

#endif // THREADS_H
