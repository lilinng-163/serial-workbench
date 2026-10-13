// SPDX-License-Identifier: Apache-2.0

#include "window.h"

namespace sw
{
void Pane::buildScriptResults()
{
    scriptPanel = new QFrame();
    scriptPanel->setObjectName("scriptResultsPanel");
    auto body = new QVBoxLayout(scriptPanel);
    body->setContentsMargins(14, 0, 14, 8);
    body->setSpacing(4);
    auto header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(8);
    auto title = new QLabel("脚本解析");
    header->addWidget(title);
    scriptSummary = new QLabel("等待解析");
    scriptSummary->setObjectName("muted");
    scriptSummary->setTextFormat(Qt::PlainText);
    scriptSummary->setWordWrap(true);
    header->addWidget(scriptSummary, 1);
    auto clear = new IconButton("trash", "清空脚本结果显示，保留共享历史");
    clear->setObjectName("clearScriptResults");
    clear->setFixedSize(28, 28);
    header->addWidget(clear);
    scriptCollapse = new IconButton("scriptCollapse", "收起脚本结果");
    scriptCollapse->setObjectName("collapseScriptResults");
    scriptCollapse->setFixedSize(28, 28);
    header->addWidget(scriptCollapse);
    body->addLayout(header);

    scriptTable = new QTreeWidget();
    scriptTable->setObjectName("scriptResults");
    scriptTable->setRootIsDecorated(false);
    scriptTable->setUniformRowHeights(true);
    scriptTable->setAlternatingRowColors(true);
    scriptTable->setColumnCount(2);
    scriptTable->setHeaderLabels({"时间", "解析字段"});
    scriptTable->setColumnWidth(0, 116);
    scriptTable->header()->setStretchLastSection(false);
    scriptTable->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    scriptTable->header()->hide();
    scriptTable->setMinimumHeight(54);
    scriptTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    body->addWidget(scriptTable, 1);
    auto copy = new QAction("复制解析结果", scriptTable);
    copy->setShortcut(QKeySequence::Copy);
    copy->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    scriptTable->addAction(copy);
    scriptTable->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(copy, &QAction::triggered, this, [this]
    {
        QStringList lines;
        for (auto item : scriptTable->selectedItems())
        {
            lines.append(item->data(0, Qt::UserRole).toJsonObject()["text"].toString());
        }
        if (!lines.isEmpty()) { QApplication::clipboard()->setText(lines.join('\n')); }
    });
    connect(clear, &QAbstractButton::clicked, this, &Pane::clearScriptResults);
    connect(scriptCollapse, &QAbstractButton::clicked, this, [this]
    {
        if (!scriptCollapsed)
        {
            scriptPanelHeight = qMax(96, receiveSplitter->sizes().value(1, scriptPanelHeight));
        }
        scriptCollapsed = !scriptCollapsed;
        resizeScriptResults();
        window->saveSettings();
    });
    connect(receiveSplitter, &QSplitter::splitterMoved, this, [this]
    {
        if (!scriptPanel->isHidden() && !scriptCollapsed)
        {
            scriptPanelHeight = qMax(96, receiveSplitter->sizes().value(1, scriptPanelHeight));
        }
    });
    receiveSplitter->addWidget(scriptPanel);
    receiveSplitter->setStretchFactor(0, 1);
    receiveSplitter->setStretchFactor(1, 0);
    scriptPanel->hide();
}

void Pane::resizeScriptResults()
{
    if (!scriptPanel || scriptPanel->isHidden()) { return; }
    scriptTable->setVisible(!scriptCollapsed);
    scriptPanel->setMinimumHeight(scriptCollapsed ? 32 : 96);
    scriptCollapse->glyph = scriptCollapsed ? "scriptExpand" : "scriptCollapse";
    scriptCollapse->setToolTip(scriptCollapsed ? "展开脚本结果" : "收起脚本结果");
    scriptCollapse->setAccessibleName(scriptCollapse->toolTip());
    scriptCollapse->update();
    const int total = qMax(180, receiveSplitter->height() - receiveSplitter->handleWidth());
    const int height = scriptCollapsed ? 32 : qMin(scriptPanelHeight, qMax(96, total / 2));
    receiveSplitter->setSizes({qMax(80, total - height), height});
}

void Pane::clearScriptResults()
{
    if (!scriptTable) { return; }
    scriptTable->clear();
    scriptCursors = {};
    scriptViewStart = window->engine->journal.sequence;
    if (channel)
    {
        for (auto source = channel->sources.cbegin(); source != channel->sources.cend(); ++source)
        {
            scriptCursors[source.key()] = source.value()->scriptCount;
        }
    }
    scriptSummary->setText("0 条结果");
}

void Pane::refreshScriptResults()
{
    if (!scriptPanel) { return; }
    bool history = false;
    if (channel)
    {
        for (const auto &source : channel->sources) { history |= !source->scriptRecords.empty(); }
    }
    const bool enabled = channel ? channel->scriptEnabled : scriptRequested;
    const QString error = channel ? channel->scriptError : QString();
    const bool visible = supportsScript() && (enabled || history || !error.isEmpty());
    const bool wasHidden = scriptPanel->isHidden();
    scriptPanel->setVisible(visible);
    if (!visible) { return; }
    if (wasHidden)
    {
        resizeScriptResults();
        QTimer::singleShot(0, this, &Pane::resizeScriptResults);
    }
    if (channel && !pause->isChecked())
    {
        const auto sources = channel->scriptResults(0, 200, scriptCursors)["sources"].toObject();
        for (auto source = sources.begin(); source != sources.end(); ++source)
        {
            const auto batch = source.value().toObject();
            scriptCursors[source.key()] = batch["next_cursor"];
            for (const auto &value : batch["records"].toArray())
            {
                const auto record = value.toObject();
                if (record["event_seq"].toInteger() <= scriptViewStart) { continue; }
                QStringList fields;
                const auto values = record["value"].toObject();
                for (auto field = values.begin(); field != values.end(); ++field)
                {
                    fields.append(field.key() + " = " + QString::number(field.value().toDouble(), 'g', 9));
                }
                auto item = new QTreeWidgetItem(scriptTable, {record["time"].toString().mid(11, 12), fields.join("    ")});
                item->setData(0, Qt::UserRole, record);
                item->setToolTip(1, record["pretty"].toString());
                while (scriptTable->topLevelItemCount() > 200)
                {
                    delete scriptTable->takeTopLevelItem(0);
                }
                if (follow->isChecked()) { scriptTable->scrollToItem(item); }
            }
        }
    }
    scriptSummary->setText(!error.isEmpty() ? "已停用 · " + error
        : !channel ? "连接后开始解析"
        : scriptTable->topLevelItemCount() ? QString("%1 条结果").arg(scriptTable->topLevelItemCount())
        : enabled ? "等待解析" : "已停用");
    scriptSummary->setToolTip(error);
}
}
