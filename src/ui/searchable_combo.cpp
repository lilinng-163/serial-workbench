// SPDX-License-Identifier: Apache-2.0
#include "searchable_combo.h"

namespace sw
{
namespace
{
QString searchKey(QString text)
{
    static const QRegularExpression separators("[\\s_./\\\\-]+");
    return text.toCaseFolded().remove(separators);
}

class ChoiceFilter : public QSortFilterProxyModel
{
public:
    explicit ChoiceFilter(QObject *parent) : QSortFilterProxyModel(parent) {}

    void setQuery(const QString &query)
    {
        queryKey = searchKey(query);
        invalidate();
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        const auto value = QSortFilterProxyModel::data(index, role);
        if (role == Qt::DisplayRole && value.toString().isEmpty()) { return "未选择配置"; }
        if (role == Qt::ToolTipRole && value.toString().isEmpty())
        {
            return QSortFilterProxyModel::data(index, Qt::DisplayRole);
        }
        return value;
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        const auto index = sourceModel()->index(row, filterKeyColumn(), parent);
        const auto key = searchKey(index.data(Qt::DisplayRole).toString() + " " + index.data(Qt::ToolTipRole).toString());
        return key.contains(queryKey);
    }

private:
    QString queryKey;
};
}

SearchableCombo::SearchableCombo(const QString &searchHint, QWidget *parent) : Combo(parent), hint(searchHint)
{
    setToolTip("展开下拉框后输入关键词搜索；支持大小写、空格和连字符的宽松匹配。Ctrl+空格也可打开搜索。");
    auto shortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space), this);
    shortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(shortcut, &QShortcut::activated, this, &SearchableCombo::showPopup);
}

void SearchableCombo::buildPopup()
{
    popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setObjectName("searchComboPopup");
    popup->setAttribute(Qt::WA_TranslucentBackground);
    popup->installEventFilter(this);
    auto layout = new QVBoxLayout(popup);
    layout->setContentsMargins(10, 10, 10, 8);
    layout->setSpacing(8);
    search = new QLineEdit();
    search->setObjectName("comboSearchInput");
    search->setPlaceholderText(hint);
    search->setAccessibleName(hint);
    search->setClearButtonEnabled(true);
    search->installEventFilter(this);
    layout->addWidget(search);
    matches = new QListView();
    matches->setObjectName("comboSearchResults");
    matches->setEditTriggers(QAbstractItemView::NoEditTriggers);
    matches->setSelectionMode(QAbstractItemView::SingleSelection);
    matches->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    matches->setTextElideMode(Qt::ElideMiddle);
    auto filter = new ChoiceFilter(popup);
    filtered = filter;
    matches->setModel(filtered);
    layout->addWidget(matches, 1);
    matchCount = new QLabel();
    matchCount->setObjectName("muted");
    matchCount->setAccessibleName("搜索结果数量");
    layout->addWidget(matchCount);
    connect(search, &QLineEdit::textChanged, this, [this, filter](const QString &query)
    {
        filter->setQuery(query);
        updateMatches();
    });
    connect(matches, &QListView::clicked, this, &SearchableCombo::choose);
    connect(matches, &QListView::activated, this, &SearchableCombo::choose);
}

void SearchableCombo::updateMatches()
{
    matchCount->setText(filtered->rowCount() == 0 ? "没有匹配项 · 试试其他关键词"
        : QString("找到 %1 项 · ↑↓ 选择，Enter 确认，Esc 返回").arg(filtered->rowCount()));
    const int selectedRow = isEditable() ? findText(currentText(), Qt::MatchExactly) : currentIndex();
    const auto selected = filtered->mapFromSource(model()->index(selectedRow, modelColumn(), rootModelIndex()));
    const auto index = selected.isValid() ? selected : filtered->index(0, modelColumn());
    matches->setCurrentIndex(index);
    if (index.isValid()) { matches->scrollTo(index, QAbstractItemView::PositionAtCenter); }
}

void SearchableCombo::showPopup()
{
    if (!isEnabled()) { return; }
    if (!popup) { buildPopup(); }
    filtered->setSourceModel(model());
    filtered->setFilterKeyColumn(modelColumn());
    search->clear();
    updateMatches();
    auto display = QGuiApplication::screenAt(mapToGlobal(rect().center()));
    if (!display) { display = screen(); }
    const auto available = display->availableGeometry();
    const int popupWidth = qMin(qMax(width(), 440), available.width() - 16);
    const int popupHeight = qMin(390, available.height() - 32);
    auto position = mapToGlobal(QPoint(0, height() + 3));
    if (position.y() + popupHeight > available.bottom() - 8)
    {
        position.setY(mapToGlobal(QPoint()).y() - popupHeight - 3);
    }
    position.setX(qBound(available.left() + 8, position.x(), available.right() - popupWidth - 7));
    position.setY(qBound(available.top() + 8, position.y(), available.bottom() - popupHeight - 7));
    popup->setGeometry(QRect(position, QSize(popupWidth, popupHeight)));
    popup->show();
    search->setFocus(Qt::PopupFocusReason);
}

void SearchableCombo::hidePopup()
{
    if (popup) { popup->hide(); }
    QComboBox::hidePopup();
}

void SearchableCombo::choose(const QModelIndex &index)
{
    if (!popup || !popup->isVisible() || !index.isValid() || !(index.flags() & Qt::ItemIsEnabled)) { return; }
    const int row = filtered->mapToSource(index).row();
    hidePopup();
    const QPointer<SearchableCombo> guard(this);
    setCurrentIndex(row);
    if (guard) { emit activated(row); }
}

bool SearchableCombo::eventFilter(QObject *object, QEvent *event)
{
    if (object == popup && event->type() == QEvent::Hide) { QComboBox::hidePopup(); }
    if (object == search && popup && popup->isVisible() && event->type() == QEvent::KeyPress)
    {
        const auto key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape)
        {
            hidePopup();
            setFocus(Qt::OtherFocusReason);
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
        {
            choose(matches->currentIndex());
            return true;
        }
        if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up)
        {
            if (filtered->rowCount() > 0)
            {
                const int delta = key->key() == Qt::Key_Down ? 1 : -1;
                const int row = qBound(0, matches->currentIndex().row() + delta, filtered->rowCount() - 1);
                const auto index = filtered->index(row, modelColumn());
                matches->setCurrentIndex(index);
                matches->scrollTo(index);
            }
            return true;
        }
    }
    return Combo::eventFilter(object, event);
}

void SearchableCombo::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Space && event->modifiers() == Qt::ControlModifier)
    {
        showPopup();
        event->accept();
        return;
    }
    if (!isEditable() && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && !event->text().isEmpty() && event->text().at(0).isPrint())
    {
        showPopup();
        search->setText(event->text());
        event->accept();
        return;
    }
    Combo::keyPressEvent(event);
}
}
