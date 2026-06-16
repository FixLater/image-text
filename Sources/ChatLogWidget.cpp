#include "ChatLogWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QEvent>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QtMath>

ChatBubble::ChatBubble(const ChatMessage &msg, QWidget *parent)
    : QWidget(parent), m_msg(msg) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    if (m_msg.type == "system") {
        m_systemLabel = new QLabel(m_msg.text, this);
        m_systemLabel->setAlignment(Qt::AlignCenter);
        m_systemLabel->setStyleSheet(
            "QLabel { color: #555e6e; font-size: 7pt; font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif; "
            "background: transparent; padding: 4px 0; }"
        );
        return;
    }

    bool isSent = (m_msg.type == "sent");
    QColor textColor = isSent ? QColor("#ffffff") : QColor("#e2e8f0");
    QColor avatarBg = isSent ? QColor(0x3b82f6) : QColor("#10b981");
    QString letter = isSent ? "S" : "R";

    m_avatarLabel = new QLabel(this);
    m_avatarLabel->setFixedSize(34, 34);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    QFont af("Microsoft YaHei UI", 12, QFont::Bold);
    m_avatarLabel->setFont(af);
    m_avatarLabel->setText(letter);
    m_avatarLabel->setStyleSheet(
        QString("QLabel { color: #ffffff; background-color: %1; border-radius: 17px; }").arg(avatarBg.name())
    );

    m_textBrowser = new QTextBrowser(this);
    m_textBrowser->setReadOnly(true);
    m_textBrowser->setOpenExternalLinks(false);
    m_textBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textBrowser->setFrameShape(QFrame::NoFrame);
    m_textBrowser->setContextMenuPolicy(Qt::CustomContextMenu);
    m_textBrowser->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_textBrowser->setPlaceholderText("");
    m_textBrowser->document()->setDefaultFont(QFont("Microsoft YaHei UI", 9));
    m_textBrowser->setPlainText(m_msg.text);
    m_textBrowser->setStyleSheet(
        QString(
            "QTextBrowser {"
            "  background-color: transparent; color: %1;"
            "  border: none; padding: 0px; margin: 0px;"
            "  selection-background-color: rgba(255, 255, 255, 0.3);"
            "}"
        ).arg(textColor.name())
    );

    connect(m_textBrowser, &QWidget::customContextMenuRequested, this, [this](QPoint pos) {
        QMenu menu(this);
        menu.setStyleSheet(
            "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
            "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
            "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
        );
        QAction *copyAction = menu.addAction("复制");
        QAction *selectAllAction = menu.addAction("全选");
        QAction *selected = menu.exec(m_textBrowser->viewport()->mapToGlobal(pos));
        if (selected == copyAction) {
            if (m_textBrowser->textCursor().hasSelection()) {
                m_textBrowser->copy();
            } else {
                m_textBrowser->selectAll();
                m_textBrowser->copy();
            }
        } else if (selected == selectAllAction) {
            m_textBrowser->selectAll();
        }
    });
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
    m_cachedBubbleH = qMax(36, qCeil(wrapDoc.size().height()) + 16);
}

QSize ChatBubble::sizeHint() const {
    if (m_msg.type == "system") {
        int w = parentWidget() ? parentWidget()->width() : 400;
        return {w, 28};
    }
    ensureLayout();
    int w = parentWidget() ? parentWidget()->width() : 400;
    return {w, m_cachedBubbleH + 16};
}

void ChatBubble::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);

    if (m_msg.type == "system") {
        if (m_systemLabel) m_systemLabel->setGeometry(0, 6, width(), 20);
        return;
    }

    m_cachedBubbleW = 0;
    m_cachedBubbleH = 0;
    ensureLayout();

    int w = width();
    int avatarSize = 34;
    int gap = 10;
    int avatarMargin = 12;
    bool isSent = (m_msg.type == "sent");
    int avatarY = 8;
    int avatarX, bubbleX, bubbleY = 8;

    if (isSent) {
        avatarX = w - avatarSize - avatarMargin;
        bubbleX = avatarX - gap - m_cachedBubbleW;
    } else {
        avatarX = avatarMargin;
        bubbleX = avatarX + avatarSize + gap;
    }

    if (m_avatarLabel) {
        m_avatarLabel->move(avatarX, avatarY);
    }

    if (m_textBrowser) {
        m_textBrowser->document()->setTextWidth(m_cachedBubbleW - 24);
        int textX = bubbleX + 12;
        int textY = bubbleY + 8;
        int textW = m_cachedBubbleW - 24;
        int textH = m_cachedBubbleH - 16;
        m_textBrowser->setGeometry(textX, textY, textW, textH);
    }

    int totalH = m_cachedBubbleH + 16;
    if (height() != totalH) {
        setFixedHeight(totalH);
    }
}

void ChatBubble::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_msg.type == "system") return;

    int w = width();
    int avatarSize = 34;
    int gap = 10;
    int avatarMargin = 12;

    ensureLayout();
    int bubbleW = m_cachedBubbleW;
    int bubbleH = m_cachedBubbleH;

    bool isSent = (m_msg.type == "sent");
    QColor bubbleColor = isSent ? QColor("#3b82f6") : QColor("#2a2d32");
    QColor avatarColor = isSent ? QColor("#3b82f6") : QColor("#10b981");
    QString avatarLetter = isSent ? "S" : "R";

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

    int radius = 6;
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

    painter.setPen(Qt::NoPen);
    painter.setBrush(avatarColor);
    painter.drawEllipse(avatarX, avatarY, avatarSize, avatarSize);

    QFont avFont("Microsoft YaHei UI", 12, QFont::Bold);
    painter.setFont(avFont);
    painter.setPen(QColor("#ffffff"));
    painter.drawText(QRect(avatarX, avatarY, avatarSize, avatarSize), Qt::AlignCenter, avatarLetter);
}

void ChatBubble::contextMenuEvent(QContextMenuEvent *event) {
    if (m_msg.type == "system" || !m_textBrowser) return;

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { background-color: #1e293b; color: #e2e8f0; border: 1px solid #334155; border-radius: 6px; padding: 4px; }"
        "QMenu::item { padding: 6px 24px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: rgba(14, 165, 233, 0.2); color: #0ea5e9; }"
    );
    QAction *copyAction = menu.addAction("复制");
    QAction *selectAllAction = menu.addAction("全选");
    QAction *selected = menu.exec(event->globalPos());
    if (selected == copyAction) {
        if (m_textBrowser->textCursor().hasSelection()) {
            m_textBrowser->copy();
        } else {
            m_textBrowser->selectAll();
            m_textBrowser->copy();
        }
    } else if (selected == selectAllAction) {
        m_textBrowser->selectAll();
    }
}

void ChatBubble::clearSelection() {
    if (m_textBrowser) {
        QTextCursor cursor = m_textBrowser->textCursor();
        cursor.clearSelection();
        m_textBrowser->setTextCursor(cursor);
    }
}

ChatLogWidget::ChatLogWidget(QWidget *parent)
    : QScrollArea(parent) {
    setStyleSheet(
        "QScrollArea { background-color: #1a1b1e; border: 1px solid #36383d; border-radius: 6px; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical { background-color: #0ea5e9; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background-color: #38bdf8; }"
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
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QWidget *clickedWidget = childAt(me->pos());
            bool clickedOnTextBrowser = false;
            while (clickedWidget) {
                if (qobject_cast<QTextBrowser *>(clickedWidget)) {
                    clickedOnTextBrowser = true;
                    break;
                }
                clickedWidget = clickedWidget->parentWidget();
            }
            if (!clickedOnTextBrowser) {
                for (int i = 0; i < m_layout->count(); i++) {
                    QLayoutItem *item = m_layout->itemAt(i);
                    if (item && item->widget()) {
                        auto *bubble = qobject_cast<ChatBubble *>(item->widget());
                        if (bubble) {
                            bubble->clearSelection();
                        }
                    }
                }
            }
            setFocus();
        }
        if (event->type() == QEvent::FocusIn) {
            setStyleSheet(
                "QScrollArea { background-color: #1a1b1e; border: 1px solid #0ea5e9; border-radius: 6px; }"
                "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background-color: #0ea5e9; border-radius: 3px; min-height: 20px; }"
                "QScrollBar::handle:vertical:hover { background-color: #38bdf8; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
            );
        } else if (event->type() == QEvent::FocusOut) {
            setStyleSheet(
                "QScrollArea { background-color: #1a1b1e; border: 1px solid #36383d; border-radius: 6px; }"
                "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background-color: #0ea5e9; border-radius: 3px; min-height: 20px; }"
                "QScrollBar::handle:vertical:hover { background-color: #38bdf8; }"
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
