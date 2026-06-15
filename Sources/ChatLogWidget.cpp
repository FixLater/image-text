#include "ChatLogWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QtMath>

ChatBubble::ChatBubble(const ChatMessage &msg, QWidget *parent)
    : QWidget(parent), m_msg(msg) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ChatBubble::ensureLayout() const {
    if (m_cachedBubbleW > 0) return;

    QFont font("Microsoft YaHei UI", 9);
    QFontMetrics fm(font);
    int fmWidth = fm.horizontalAdvance(m_msg.text);

    QTextDocument sizeDoc;
    sizeDoc.setDefaultFont(font);
    QTextOption noWrapOpt;
    noWrapOpt.setWrapMode(QTextOption::NoWrap);
    sizeDoc.setDefaultTextOption(noWrapOpt);
    sizeDoc.setPlainText(m_msg.text);
    int docWidth = static_cast<int>(qCeil(sizeDoc.size().width()));

    int textWidth = qMax(fmWidth, docWidth);
    int maxW = 300;
    m_cachedBubbleW = qMin(textWidth + 32, maxW);

    QTextDocument wrapDoc;
    wrapDoc.setDefaultFont(font);
    wrapDoc.setTextWidth(m_cachedBubbleW - 24);
    QTextOption wopt;
    wopt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    wrapDoc.setDefaultTextOption(wopt);
    wrapDoc.setPlainText(m_msg.text);
    m_cachedBubbleH = qMax(36, static_cast<int>(qCeil(wrapDoc.size().height())) + 16);
}

QSize ChatBubble::sizeHint() const {
    if (m_msg.type == "system") {
        int w = parentWidget() ? parentWidget()->width() : 400;
        return QSize(w, 28);
    }
    ensureLayout();
    int w = parentWidget() ? parentWidget()->width() : 400;
    return QSize(w, m_cachedBubbleH + 16);
}

void ChatBubble::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateGeometry();
}

void ChatBubble::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int avatarSize = 34;
    int gap = 10;
    int avatarMargin = 12;

    QFont bubbleFont("Microsoft YaHei UI", 9);
    painter.setFont(bubbleFont);

    if (m_msg.type == "system") {
        QFont sysFont("Microsoft YaHei UI", 7);
        painter.setFont(sysFont);
        painter.setPen(QColor("#555e6e"));
        painter.drawText(QRect(0, 6, w, 20), Qt::AlignCenter, m_msg.text);
        return;
    }

    ensureLayout();
    int bubbleW = m_cachedBubbleW;
    int bubbleH = m_cachedBubbleH;

    bool isSent = (m_msg.type == "sent");
    QColor bubbleColor = isSent ? QColor("#3b82f6") : QColor("#2a2d32");
    QColor textColor = isSent ? QColor("#ffffff") : QColor("#e2e8f0");
    QColor avatarColor = isSent ? QColor("#3b82f6") : QColor("#10b981");
    QString avatarLabel = isSent ? "S" : "R";

    int avatarY = 8;
    int avatarX;
    int bubbleX, bubbleY = avatarY;

    if (isSent) {
        avatarX = w - avatarSize - avatarMargin;
        bubbleX = avatarX - gap - bubbleW;
    } else {
        avatarX = avatarMargin;
        bubbleX = avatarX + avatarSize + gap;
    }

    int radius = 10;
    int tailSize = 6;
    int tailMiddleY = bubbleY + 14;

    QPainterPath bubblePath;
    bubblePath.addRoundedRect(bubbleX, bubbleY, bubbleW, bubbleH, radius, radius);

    QPainterPath tailPath;
    if (isSent) {
        int tx = bubbleX + bubbleW;
        tailPath.moveTo(tx, tailMiddleY - tailSize);
        tailPath.lineTo(tx + tailSize + 2, tailMiddleY);
        tailPath.lineTo(tx, tailMiddleY + tailSize);
        tailPath.closeSubpath();
    } else {
        int tx = bubbleX;
        tailPath.moveTo(tx, tailMiddleY - tailSize);
        tailPath.lineTo(tx - tailSize - 2, tailMiddleY);
        tailPath.lineTo(tx, tailMiddleY + tailSize);
        tailPath.closeSubpath();
    }

    QPainterPath fullPath = bubblePath.united(tailPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bubbleColor);
    painter.drawPath(fullPath);

    QTextDocument textDoc;
    textDoc.setDefaultFont(bubbleFont);
    textDoc.setTextWidth(bubbleW - 24);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textDoc.setDefaultTextOption(opt);

    QTextCursor cursor(&textDoc);
    QTextCharFormat fmt;
    fmt.setForeground(textColor);
    cursor.select(QTextCursor::Document);
    cursor.setCharFormat(fmt);
    cursor.insertText(m_msg.text);

    painter.save();
    painter.translate(bubbleX + 12, bubbleY + 8);
    QAbstractTextDocumentLayout *layout = textDoc.documentLayout();
    layout->draw(&painter, QAbstractTextDocumentLayout::PaintContext());
    painter.restore();

    painter.setPen(Qt::NoPen);
    painter.setBrush(avatarColor);
    painter.drawEllipse(avatarX, avatarY, avatarSize, avatarSize);

    QFont avatarFont("Microsoft YaHei UI", 12, QFont::Bold);
    painter.setFont(avatarFont);
    painter.setPen(QColor("#ffffff"));
    painter.drawText(QRect(avatarX, avatarY, avatarSize, avatarSize), Qt::AlignCenter, avatarLabel);
}

ChatLogWidget::ChatLogWidget(QWidget *parent)
    : QScrollArea(parent) {
    setStyleSheet(
        "QScrollArea { background-color: #1a1b1e; border: 1px solid transparent; border-radius: 4px; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background-color: #36383d; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background-color: #475569; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );

    m_container = new QWidget(this);
    m_container->setStyleSheet("background-color: transparent;");

    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(10, 10, 10, 10);
    m_layout->setSpacing(2);
    m_layout->addStretch();

    setWidget(m_container);
    setWidgetResizable(true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFocusPolicy(Qt::StrongFocus);
    viewport()->installEventFilter(this);
}

bool ChatLogWidget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == viewport()) {
        if (event->type() == QEvent::FocusIn) {
            setStyleSheet(
                "QScrollArea { background-color: #1a1b1e; border: 1px solid #0ea5e9; border-radius: 4px; }"
                "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background-color: #36383d; border-radius: 3px; min-height: 20px; }"
                "QScrollBar::handle:vertical:hover { background-color: #475569; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            );
        } else if (event->type() == QEvent::FocusOut) {
            setStyleSheet(
                "QScrollArea { background-color: #1a1b1e; border: 1px solid transparent; border-radius: 4px; }"
                "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background-color: #36383d; border-radius: 3px; min-height: 20px; }"
                "QScrollBar::handle:vertical:hover { background-color: #475569; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            );
        }
    }
    return QScrollArea::eventFilter(obj, event);
}

void ChatLogWidget::appendMessage(const QString &text, const QString &type) {
    ChatMessage msg;
    msg.text = text;
    msg.type = type;
    m_messages.append(msg);

    ChatBubble *bubble = new ChatBubble(msg, m_container);
    m_layout->insertWidget(m_layout->count() - 1, bubble);

    QScrollBar *sb = verticalScrollBar();
    sb->setValue(sb->maximum());
}

void ChatLogWidget::clearMessages() {
    m_messages.clear();
    QLayoutItem *item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_layout->addStretch();
}
