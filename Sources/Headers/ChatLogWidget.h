#ifndef CHATLOGWIDGET_H
#define CHATLOGWIDGET_H

#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVector>
#include <QLabel>
#include <QTextDocument>
#include <QScrollBar>
#include <QResizeEvent>

struct ChatMessage {
    QString text;
    QString type;
};

class ChatBubble : public QWidget {
    Q_OBJECT
public:
    explicit ChatBubble(const ChatMessage &msg, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    ChatMessage m_msg;
    mutable int m_cachedBubbleW = 0;
    mutable int m_cachedBubbleH = 0;
    void ensureLayout() const;
};

class ChatLogWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit ChatLogWidget(QWidget *parent = nullptr);
    void appendMessage(const QString &text, const QString &type);
    void clearMessages();
    QVector<ChatMessage> messages() const { return m_messages; }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QWidget *m_container;
    QVBoxLayout *m_layout;
    QVector<ChatMessage> m_messages;
};

#endif // CHATLOGWIDGET_H
