#include "DashboardPage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QTextOption>

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 24, 32, 24);
    mainLayout->setSpacing(0);

    auto *titleLabel = new QLabel("仪表盘");
    titleLabel->setStyleSheet(
        "font-size: 20pt; font-weight: bold; color: #e2e8f0; background: transparent; border: none; padding-bottom: 4px;"
    );
    mainLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("选择一个模块开始使用");
    subtitleLabel->setStyleSheet(
        "font-size: 9pt; color: #64748b; background: transparent; border: none; padding-bottom: 24px;"
    );
    mainLayout->addWidget(subtitleLabel);

    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(16);
    m_gridLayout->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    ModuleCard wsCard;
    wsCard.name = "websocket";
    wsCard.icon = "⚡";
    wsCard.title = "WebSocket";
    wsCard.description = "连接 WebSocket 服务器，实时发送消息和文件";

    m_gridLayout->addWidget(createCard(wsCard), 0, 0);

    mainLayout->addLayout(m_gridLayout);
    mainLayout->addStretch(1);
}

bool DashboardPage::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            auto *cardWidget = qobject_cast<QWidget *>(obj);
            if (cardWidget && cardWidget->objectName() == "moduleCard") {
                auto *clickedChild = cardWidget->childAt(mouseEvent->pos());
                if (clickedChild && clickedChild->objectName() == "prevBtn") return false;
                if (clickedChild && clickedChild->objectName() == "nextBtn") return false;

                int clickY = mouseEvent->pos().y();
                if (clickY > 36) return false;

                for (auto it = m_cardWidgets.constBegin(); it != m_cardWidgets.constEnd(); ++it) {
                    if (it.value().logPreview && it.value().logPreview->parentWidget() == cardWidget) {
                        emit moduleClicked(it.key());
                        return false;
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DashboardPage::updateCardStatus(const QString &moduleName, bool connected) {
    if (!m_cardWidgets.contains(moduleName)) return;
    auto &widgets = m_cardWidgets[moduleName];
    if (connected) {
        widgets.statusLabel->setStyleSheet(
            "background-color: #34d399; border-radius: 4px; min-width: 8px; max-width: 8px;"
            "min-height: 8px; max-height: 8px; border: none;"
        );
    } else {
        widgets.statusLabel->setStyleSheet(
            "background-color: #64748b; border-radius: 4px; min-width: 8px; max-width: 8px;"
            "min-height: 8px; max-height: 8px; border: none;"
        );
    }
}

void DashboardPage::appendCardLog(const QString &moduleName, int tabIndex, const QString &html) {
    if (!m_cardWidgets.contains(moduleName)) return;
    if (!m_cardTabIndices.contains(moduleName)) return;
    if (m_cardTabIndices[moduleName] != tabIndex) return;

    auto &widgets = m_cardWidgets[moduleName];
    widgets.logPreview->append(html);

    QTextCursor cursor = widgets.logPreview->textCursor();
    cursor.movePosition(QTextCursor::End);
    widgets.logPreview->setTextCursor(cursor);

    while (widgets.logPreview->document()->blockCount() > 20) {
        QTextCursor firstBlock(widgets.logPreview->document());
        firstBlock.movePosition(QTextCursor::Start);
        firstBlock.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor);
        firstBlock.removeSelectedText();
    }
}

void DashboardPage::setCardLogFromTab(const QString &moduleName, const QString &html) {
    if (!m_cardWidgets.contains(moduleName)) return;
    auto &widgets = m_cardWidgets[moduleName];
    widgets.logPreview->clear();
    if (!html.isEmpty()) {
        widgets.logPreview->setHtml(html);
    }
    QTextCursor cursor = widgets.logPreview->textCursor();
    cursor.movePosition(QTextCursor::End);
    widgets.logPreview->setTextCursor(cursor);
}

void DashboardPage::updateCardTabInfo(const QString &moduleName, int current, int total) {
    if (!m_cardWidgets.contains(moduleName)) return;
    auto &widgets = m_cardWidgets[moduleName];

    m_cardTabIndices[moduleName] = current;

    bool hasMultiple = total > 1;
    widgets.prevBtn->setVisible(hasMultiple);
    widgets.nextBtn->setVisible(hasMultiple);
    widgets.tabLabel->setVisible(hasMultiple);

    widgets.prevBtn->setEnabled(current > 0);
    widgets.nextBtn->setEnabled(current < total - 1);
    widgets.tabLabel->setText(QString("%1/%2").arg(current + 1).arg(total));
}

QWidget *DashboardPage::createCard(const ModuleCard &card) {
    auto *cardWidget = new QWidget();
    cardWidget->setFixedSize(280, 180);
    cardWidget->setObjectName("moduleCard");
    cardWidget->setCursor(Qt::PointingHandCursor);

    auto *mainLayout = new QVBoxLayout(cardWidget);
    mainLayout->setContentsMargins(16, 10, 16, 10);
    mainLayout->setSpacing(0);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    auto *titleLabel = new QLabel(card.title);
    titleLabel->setStyleSheet(
        "font-size: 9pt; font-weight: bold; color: #94a3b8; background: transparent; border: none;"
    );
    topRow->addWidget(titleLabel);

    topRow->addStretch(1);

    auto *tabLabel = new QLabel("1/1");
    tabLabel->setStyleSheet(
        "font-size: 7pt; color: #475569; background: transparent; border: none;"
    );
    tabLabel->setVisible(false);
    topRow->addWidget(tabLabel);

    auto *statusLabel = new QLabel();
    statusLabel->setStyleSheet(
        "background-color: #64748b; border-radius: 4px; min-width: 8px; max-width: 8px;"
        "min-height: 8px; max-height: 8px; border: none;"
    );
    topRow->addWidget(statusLabel);

    mainLayout->addLayout(topRow);

    mainLayout->addSpacing(6);

    auto *logPreview = new QTextBrowser();
    logPreview->setReadOnly(true);
    logPreview->setContextMenuPolicy(Qt::NoContextMenu);
    logPreview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    logPreview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    logPreview->setOpenExternalLinks(false);
    logPreview->setWordWrapMode(QTextOption::WrapAnywhere);
    logPreview->setStyleSheet(
        "QTextBrowser {"
        "  background-color: transparent; color: #94a3b8;"
        "  border: 1px solid #36383d; border-radius: 4px;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
        "  font-size: 7pt; padding: 4px;"
        "}"
    );
    logPreview->document()->setDefaultStyleSheet(
        ".ts-divider { color: #555; font-size: 6pt; }"
        ".link { color: #38bdf8; text-decoration: underline; }"
    );
    logPreview->document()->setDefaultStyleSheet(
        ".timestamp { color: #334155; }"
        ".info { color: #000000; }"
        ".error { color: #f87171; }"
        ".success { color: #34d399; }"
        ".warning { color: #fbbf24; }"
        ".sent { color: #16a34a; }"
    );
    mainLayout->addWidget(logPreview);

    auto *prevBtn = new QPushButton("◀", cardWidget);
    prevBtn->setObjectName("prevBtn");
    prevBtn->setFixedSize(12, 40);
    prevBtn->move(2, 70);
    prevBtn->setCursor(Qt::PointingHandCursor);
    prevBtn->setVisible(false);
    prevBtn->setStyleSheet(
        "QPushButton { background-color: rgba(38, 40, 44, 200); color: #64748b; border: none; border-radius: 4px; font-size: 9pt; }"
        "QPushButton:hover { color: #e2e8f0; background-color: rgba(55, 65, 81, 220); }"
        "QPushButton:disabled { color: #36383d; background-color: transparent; }"
    );

    auto *nextBtn = new QPushButton("▶", cardWidget);
    nextBtn->setObjectName("nextBtn");
    nextBtn->setFixedSize(12, 40);
    nextBtn->move(266, 70);
    nextBtn->setCursor(Qt::PointingHandCursor);
    nextBtn->setVisible(false);
    nextBtn->setStyleSheet(
        "QPushButton { background-color: rgba(38, 40, 44, 200); color: #64748b; border: none; border-radius: 4px; font-size: 9pt; }"
        "QPushButton:hover { color: #e2e8f0; background-color: rgba(55, 65, 81, 220); }"
        "QPushButton:disabled { color: #36383d; background-color: transparent; }"
    );

    m_cardWidgets[card.name] = {statusLabel, logPreview, prevBtn, nextBtn, tabLabel};
    m_cardTabIndices[card.name] = 0;

    cardWidget->setStyleSheet(
        "#moduleCard {"
        "  background-color: transparent; border: 1px solid #36383d; border-radius: 8px;"
        "}"
        "#moduleCard:hover {"
        "  background-color: rgba(42, 44, 48, 0.5); border-color: #0ea5e9;"
        "}"
    );

    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    cardWidget->setGraphicsEffect(shadow);

    cardWidget->installEventFilter(this);

    connect(prevBtn, &QPushButton::clicked, this, [this, card]() {
        emit cardPrevClicked(card.name);
    });

    connect(nextBtn, &QPushButton::clicked, this, [this, card]() {
        emit cardNextClicked(card.name);
    });

    return cardWidget;
}
