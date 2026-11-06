// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "controls.h"

namespace sw
{
// Searches a flat choice list in a separate popup. A query never edits the
// current value; only choosing a result emits the usual combo signals.
class SearchableCombo : public Combo
{
public:
    explicit SearchableCombo(const QString &searchHint, QWidget *parent = nullptr);
    void showPopup() override;
    void hidePopup() override;

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QString hint;
    QPointer<QFrame> popup;
    QLineEdit *search = nullptr;
    QListView *matches = nullptr;
    QLabel *matchCount = nullptr;
    QSortFilterProxyModel *filtered = nullptr;
    void buildPopup();
    void updateMatches();
    void choose(const QModelIndex &index);
};
}
