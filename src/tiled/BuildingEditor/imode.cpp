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

#include "imode.h"

using namespace BuildingEditor;

IMode::IMode(QObject *parent) :
    QObject(parent),
    mEnabled(true),
    mActive(false)
{
}

void IMode::setEnabled(bool enabled)
{
    if (mEnabled == enabled)
        return;
    mEnabled = enabled;
    emit enabledStateChanged(mEnabled);
}

void IMode::setActive(bool active)
{
    if (mActive == active)
        return;
    mActive = active;
    emit activeStateChanged(mActive);
}

/////

SINGLETON_IMPL(ModeManager)

ModeManager::ModeManager(QObject *parent) :
    QObject(parent),
    mCurrentMode(0)
{
}

void ModeManager::setCurrentMode(IMode *mode)
{
    if (mode == mCurrentMode)
        return;

    if (mCurrentMode)
        mCurrentMode->setActive(false);

    mCurrentMode = mode;

    if (mCurrentMode)
        mCurrentMode->setActive(true);

    emit currentModeChanged();
}
