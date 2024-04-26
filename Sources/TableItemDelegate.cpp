#include <QObject>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QApplication>

class TableItemDelegate : public QStyledItemDelegate {
public:
    explicit TableItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QWidget *widget = option.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();

        // Draw the item background
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

        // Set the text color based on the item's selection and focus state
        QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
            cg = QPalette::Inactive;
        if (opt.state & QStyle::State_Selected)
            painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
        else
            painter->setPen(opt.palette.color(cg, QPalette::Text));

        // If the width of the text is greater than the width of the item, elide the text
        int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &opt, widget) + 1;
        QRect rect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
        rect.adjust(margin, 0, -margin, 0);
        QString text = opt.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, rect.width());

        // Draw the text
        painter->drawText(rect, opt.displayAlignment, text);
    }
};