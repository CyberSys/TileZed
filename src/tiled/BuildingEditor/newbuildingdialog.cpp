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

#include "newbuildingdialog.h"
#include "ui_newbuildingdialog.h"

#include "buildingeditorwindow.h"
#include "buildingtemplates.h"

using namespace BuildingEditor;

NewBuildingDialog::NewBuildingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewBuildingDialog)
{
    ui->setupUi(this);

    ui->comboBox->addItem(QLatin1String("<None>"));
    foreach (BuildingTemplate *def, BuildingTemplates::instance()->templates())
        ui->comboBox->addItem(def->name());
}

NewBuildingDialog::~NewBuildingDialog()
{
    delete ui;
}

int NewBuildingDialog::buildingWidth() const
{
    return ui->width->value();
}

int NewBuildingDialog::buildingHeight() const
{
    return ui->height->value();
}

BuildingTemplate *NewBuildingDialog::buildingTemplate() const
{
    int index = ui->comboBox->currentIndex();
    return index ? BuildingTemplates::instance()->templateAt(index - 1) : 0;
}

void NewBuildingDialog::accept()
{
    QDialog::accept();
}
