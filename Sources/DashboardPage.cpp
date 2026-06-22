#include "DashboardPage.h"
#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QTextOption>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QUrl>

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 24, 32, 24);
    mainLayout->setSpacing(0);

    m_gridLayout = new QGridLayout();
    m_gridLayout->setSpacing(16);
    m_gridLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    ModuleCard wsCard;
    wsCard.name = "websocket";
    wsCard.icon = "⚡";
    wsCard.title = "WebSocket";
    wsCard.description = "连接 WebSocket 服务器，实时发送消息和文件";
    m_gridLayout->addWidget(createCard(wsCard), 0, 0);

    m_gridLayout->addWidget(createTranslateCard(), 0, 1);
    m_gridLayout->addWidget(createFileServerCard(), 0, 2);

    mainLayout->addLayout(m_gridLayout);
    mainLayout->addStretch(1);

    m_networkManager = new QNetworkAccessManager(this);
}

bool DashboardPage::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            auto *cardWidget = qobject_cast<QWidget *>(obj);
            if (!cardWidget) return false;

            auto *clickedChild = cardWidget->childAt(mouseEvent->pos());
            if (!clickedChild) return false;

            if (clickedChild->objectName() == "prevBtn" || clickedChild->objectName() == "nextBtn")
                return false;

            int clickY = mouseEvent->pos().y();
            if (clickY <= 36) {
                QString name;
                if (cardWidget->objectName() == "moduleCard") {
                    for (auto it = m_cardWidgets.constBegin(); it != m_cardWidgets.constEnd(); ++it) {
                        if (it.value().logPreview && it.value().logPreview->parentWidget() == cardWidget) {
                            name = it.key();
                            break;
                        }
                    }
                } else if (cardWidget->objectName() == "translateCard") {
                    name = "translate";
                } else if (cardWidget->objectName() == "fileServerCard") {
                    name = "fileserver";
                }
                if (!name.isEmpty()) {
                    emit moduleClicked(name);
                    return false;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DashboardPage::updateCardStatus(const QString &moduleName, bool connected, const QString &address) {
    if (moduleName == "fileserver") {
        if (connected) {
            m_serverStatusLabel->setStyleSheet(
                "color: #34d399; font-size: 12pt; background: transparent; border: none;"
            );
            m_serverToggleBtn->setText("停止");
            m_serverToggleBtn->setStyleSheet(
                "QPushButton { background-color: #dc2626; color: white; border: none; border-radius: 4px; font-size: 9pt; padding: 4px 12px; }"
                "QPushButton:hover { background-color: #ef4444; }"
            );
            m_serverAddressLabel->setText(QString("<a href=\"%1\" style=\"color: #0ea5e9; text-decoration: underline;\">%1</a>").arg(address));
            m_serverAddressLabel->setStyleSheet(
                "font-size: 8pt; background: transparent; border: none;"
            );
            m_serverAddressLabel->setOpenExternalLinks(true);
            m_serverAddressLabel->setCursor(Qt::PointingHandCursor);
        } else {
            m_serverStatusLabel->setStyleSheet(
                "color: #64748b; font-size: 12pt; background: transparent; border: none;"
            );
            m_serverToggleBtn->setText("启动");
            m_serverToggleBtn->setStyleSheet(
                "QPushButton { background-color: #0ea5e9; color: white; border: none; border-radius: 4px; font-size: 9pt; padding: 4px 12px; }"
                "QPushButton:hover { background-color: #38bdf8; }"
            );
            m_serverAddressLabel->setText("未运行");
            m_serverAddressLabel->setStyleSheet(
                "color: #475569; font-size: 8pt; background: transparent; border: none;"
            );
        }
        return;
    }
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

QWidget *DashboardPage::createTranslateCard() {
    auto *cardWidget = new QWidget();
    cardWidget->setFixedSize(280, 180);
    cardWidget->setObjectName("translateCard");
    cardWidget->setCursor(Qt::PointingHandCursor);

    auto *mainLayout = new QVBoxLayout(cardWidget);
    mainLayout->setContentsMargins(16, 10, 16, 10);
    mainLayout->setSpacing(4);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    auto *titleLabel = new QLabel("翻译");
    titleLabel->setStyleSheet(
        "font-size: 9pt; font-weight: bold; color: #94a3b8; background: transparent; border: none;"
    );
    topRow->addWidget(titleLabel);
    topRow->addStretch(1);

    m_translateStatus = new QLabel();
    m_translateStatus->setStyleSheet(
        "background-color: #64748b; border-radius: 4px; min-width: 8px; max-width: 8px;"
        "min-height: 8px; max-height: 8px; border: none;"
    );
    topRow->addWidget(m_translateStatus);

    mainLayout->addLayout(topRow);

    m_translateInput = new QTextEdit();
    m_translateInput->setPlaceholderText("输入文本...");
    m_translateInput->setMaximumHeight(65);
    m_translateInput->setStyleSheet(
        "QTextEdit { background-color: #1a1b1e; color: #e2e8f0; border: 1px solid #36383d;"
        "  border-radius: 4px; font-size: 8pt; padding: 4px;"
        "  font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif; }"
        "QTextEdit:focus { border-color: #0ea5e9; }"
    );
    mainLayout->addWidget(m_translateInput);

    auto *btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);

    m_translateBtn = new QPushButton("翻译");
    m_translateBtn->setCursor(Qt::PointingHandCursor);
    m_translateBtn->setStyleSheet(
        "QPushButton { background-color: #0ea5e9; color: white; border: none; border-radius: 4px;"
        "  font-size: 8pt; padding: 3px 12px; }"
        "QPushButton:hover { background-color: #38bdf8; }"
        "QPushButton:disabled { background-color: #334155; color: #64748b; }"
    );
    connect(m_translateBtn, &QPushButton::clicked, this, [this]() {
        QString text = m_translateInput->toPlainText().trimmed();
        if (!text.isEmpty()) doTranslate(text);
    });
    btnRow->addStretch(1);
    btnRow->addWidget(m_translateBtn);
    mainLayout->addLayout(btnRow);

    m_translateResult = new QLabel("");
    m_translateResult->setWordWrap(true);
    m_translateResult->setMaximumHeight(40);
    m_translateResult->setStyleSheet(
        "color: #94a3b8; font-size: 8pt; background: transparent; border: none;"
        "font-family: 'Microsoft YaHei UI', 'Segoe UI', sans-serif;"
    );
    mainLayout->addWidget(m_translateResult);
    mainLayout->addStretch(1);

    cardWidget->setStyleSheet(
        "#translateCard { background-color: transparent; border: 1px solid #36383d; border-radius: 8px; }"
        "#translateCard:hover { background-color: rgba(42, 44, 48, 0.5); border-color: #0ea5e9; }"
    );

    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    cardWidget->setGraphicsEffect(shadow);
    cardWidget->installEventFilter(this);

    return cardWidget;
}

QWidget *DashboardPage::createFileServerCard() {
    auto *cardWidget = new QWidget();
    cardWidget->setFixedSize(280, 180);
    cardWidget->setObjectName("fileServerCard");
    cardWidget->setCursor(Qt::PointingHandCursor);

    auto *mainLayout = new QVBoxLayout(cardWidget);
    mainLayout->setContentsMargins(16, 10, 16, 10);
    mainLayout->setSpacing(4);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    auto *titleLabel = new QLabel("文件服务器");
    titleLabel->setStyleSheet(
        "font-size: 9pt; font-weight: bold; color: #94a3b8; background: transparent; border: none;"
    );
    topRow->addWidget(titleLabel);
    topRow->addStretch(1);

    m_serverStatusLabel = new QLabel("●");
    m_serverStatusLabel->setStyleSheet(
        "color: #64748b; font-size: 12pt; background: transparent; border: none;"
    );
    topRow->addWidget(m_serverStatusLabel);

    mainLayout->addLayout(topRow);
    mainLayout->addSpacing(4);

    auto *descLabel = new QLabel("HTTP 文件服务器\n局域网内访问文件列表");
    descLabel->setStyleSheet(
        "color: #64748b; font-size: 8pt; background: transparent; border: none;"
    );
    mainLayout->addWidget(descLabel);

    mainLayout->addStretch(1);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);

    m_serverAddressLabel = new QLabel("未运行");
    m_serverAddressLabel->setStyleSheet(
        "color: #475569; font-size: 8pt; background: transparent; border: none;"
    );
    connect(m_serverAddressLabel, &QLabel::linkActivated, this, [](const QString &link) {
        QDesktopServices::openUrl(QUrl(link));
    });
    bottomRow->addWidget(m_serverAddressLabel);
    bottomRow->addStretch(1);

    m_serverToggleBtn = new QPushButton("启动");
    m_serverToggleBtn->setCursor(Qt::PointingHandCursor);
    m_serverToggleBtn->setStyleSheet(
        "QPushButton { background-color: #0ea5e9; color: white; border: none; border-radius: 4px;"
        "  font-size: 8pt; padding: 4px 14px; }"
        "QPushButton:hover { background-color: #38bdf8; }"
    );
    connect(m_serverToggleBtn, &QPushButton::clicked, this, [this]() {
        bool isRunning = m_serverToggleBtn->text() == "停止";
        emit fileServerToggled(!isRunning);
    });
    bottomRow->addWidget(m_serverToggleBtn);

    mainLayout->addLayout(bottomRow);

    cardWidget->setStyleSheet(
        "#fileServerCard { background-color: transparent; border: 1px solid #36383d; border-radius: 8px; }"
        "#fileServerCard:hover { background-color: rgba(42, 44, 48, 0.5); border-color: #0ea5e9; }"
    );

    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    cardWidget->setGraphicsEffect(shadow);
    cardWidget->installEventFilter(this);

    return cardWidget;
}

void DashboardPage::doTranslate(const QString &text) {
    m_translateBtn->setEnabled(false);
    m_translateStatus->setStyleSheet(
        "color: #fbbf24; border-radius: 4px; min-width: 8px; max-width: 8px;"
        "min-height: 8px; max-height: 8px; border: none;"
    );
    m_translateResult->setText("翻译中...");

    QJsonObject body;
    body["text"] = text;
    body["source_lang"] = SettingsDialog::translateSourceLang();
    body["target_lang"] = SettingsDialog::translateTargetLang();

    QNetworkRequest request(QUrl("https://api.deeplx.org/0a8q3Z6SKlnxZZUKcvdky_1fVTVs1EBlOTwm4qtUi9Q/translate"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, text]() {
        reply->deleteLater();
        m_translateBtn->setEnabled(true);
        m_translateStatus->setStyleSheet(
            "background-color: #64748b; border-radius: 4px; min-width: 8px; max-width: 8px;"
            "min-height: 8px; max-height: 8px; border: none;"
        );

        if (reply->error() != QNetworkReply::NoError) {
            m_translateResult->setText("请求失败: " + reply->errorString());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject obj = doc.object();
        QString translated = obj["data"].toString();

        if (!translated.isEmpty()) {
            m_translateResult->setText(translated);
        } else {
            m_translateResult->setText("翻译结果为空");
        }
    });
}
