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

#ifndef BMPTOOLDIALOG_H
#define BMPTOOLDIALOG_H

#include <QDialog>
#include <QModelIndex>
#include <QTimer>

namespace Ui {
class BmpToolDialog;
}

namespace Tiled {
class BmpAlias;
class BmpBlend;
class BmpRule;

namespace Internal {
class MapDocument;

class BmpToolDialog : public QDialog
{
    Q_OBJECT
    
public:
    static BmpToolDialog *instance();

    void setVisible(bool visible);

    void setVisibleLater(bool visible);

    void setDocument(MapDocument *doc);

    // Hack for MainWindow so I don't have to move declaration of some undo cmds.
    static void changeBmpRules(MapDocument *doc, const QString &fileName,
                               const QList<BmpAlias*> &aliases,
                               const QList<BmpRule*> &rules);
    static void changeBmpBlends(MapDocument *doc, const QString &fileName,
                                const QList<BmpBlend*> &blends);

private slots:
    void currentRuleChanged(const QModelIndex &current);
    void brushSizeChanged(int size);
    void brushSquare();
    void brushCircle();
    void restrictToSelection(bool isRestricted);
    void toggleOverlayLayers();
    void showBMPTiles(bool show);
    void showMapTiles(bool show);
    void setVisibleNow();

    void blendHighlighted(BmpBlend *blend, int dir);
    void synchBlendTilesView();

    void expandCollapse();

    void reloadRules();
    void importRules();
    void exportRules();
    void trashRules();

    void reloadBlends();
    void importBlends();
    void exportBlends();
    void trashBlends();

    void help();

    void bmpRulesChanged();
    void bmpBlendsChanged();

    void brushChanged();

    void documentAboutToClose(int index, MapDocument *doc);

    void warningsChanged();

private:
    Q_DISABLE_COPY(BmpToolDialog)
    static BmpToolDialog *mInstance;
    explicit BmpToolDialog(QWidget *parent = 0);
    ~BmpToolDialog();

    Ui::BmpToolDialog *ui;
    MapDocument *mDocument;
    bool mVisibleLater;
    QTimer mVisibleLaterTimer;
    QMap<MapDocument*,int> mCurrentRuleForDocument;
    bool mExpanded;
};

} // namespace Internal
} // namespace Tiled

#endif // BMPTOOLDIALOG_H
