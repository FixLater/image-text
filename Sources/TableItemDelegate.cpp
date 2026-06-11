#include "TableItemDelegate.h"
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QApplication>

TableItemDelegate::TableItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent) {}

void TableItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const QWidget *widget = option.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();

    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

    QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
        cg = QPalette::Inactive;
    if (opt.state & QStyle::State_Selected)
        painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
    else
        painter->setPen(opt.palette.color(cg, QPalette::Text));

    int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, widget) + 1;
    QRect rect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
    rect.adjust(margin, 0, -margin, 0);
    QString text = opt.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, rect.width());

    painter->drawText(rect, opt.displayAlignment, text);
}
