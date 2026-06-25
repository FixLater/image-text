#include "DashboardPage.h"
#include "SettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QTextOption>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QDesktopServices>
#include <QUrl>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimer>

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 24, 32, 24);
    mainLayout->setSpacing(0);

    m_cardArea = new QWidget(this);
    m_cardArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_cardArea);

    ModuleCard wsCard;
    wsCard.name = "websocket";
    wsCard.icon = "⚡";
    wsCard.title = "WebSocket";
    wsCard.description = "连接 WebSocket 服务器，实时发送消息和文件";
    m_cardContainers["websocket"] = createCard(wsCard);
    m_cardContainers["websocket"]->setParent(m_cardArea);
    m_cardOrder << "websocket";

    m_cardContainers["translate"] = createTranslateCard();
    m_cardContainers["translate"]->setParent(m_cardArea);
    m_cardOrder << "translate";

    m_cardContainers["fileserver"] = createFileServerCard();
    m_cardContainers["fileserver"]->setParent(m_cardArea);
    m_cardOrder << "fileserver";

    m_cardContainers["shell"] = createShellCard();
    m_cardContainers["shell"]->setParent(m_cardArea);
    m_cardOrder << "shell";

    m_networkManager = new QNetworkAccessManager(this);

    QTimer::singleShot(0, this, [this]() { resizeEvent(nullptr); });
}

void DashboardPage::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (m_isAnimating) return;

    if (m_isExpanded) {
        int areaW = m_cardArea->width();
        int areaH = m_cardArea->height();
        int padding = 16;
        int leftW = 260;
        int rightW = areaW - leftW - padding * 3;
        if (rightW < 200) rightW = 200;
        int smallCardH = 130;
        int gap = 12;

        int leftIndex = 0;
        for (int i = 0; i < m_cardOrder.size(); ++i) {
            if (m_cardOrder[i] == m_expandedCardName) continue;
            auto *card = m_cardContainers[m_cardOrder[i]];
            card->setGeometry(padding, leftIndex * (smallCardH + gap), leftW, smallCardH);
            leftIndex++;
        }

        auto *expandedCard = m_cardContainers[m_expandedCardName];
        expandedCard->setGeometry(leftW + padding * 2, 0, rightW, areaH);
        return;
    }

    int areaW = m_cardArea->width();
    int areaH = m_cardArea->height();
    int cardW = 280;
    int cardH = 180;
    int gap = 16;
    int cols = qMax(1, (areaW + gap) / (cardW + gap));
    int startX = (areaW - cols * cardW - (cols - 1) * gap) / 2;

    for (int i = 0; i < m_cardOrder.size(); ++i) {
        auto *card = m_cardContainers[m_cardOrder[i]];
        int col = i % cols;
        int row = i / cols;
        int x = startX + col * (cardW + gap);
        int y = row * (cardH + gap);
        card->setFixedSize(cardW, cardH);
        card->move(x, y);
        card->show();
    }
}

bool DashboardPage::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            auto *cardWidget = qobject_cast<QWidget *>(obj);
            if (!cardWidget) return false;

            auto *clickedChild = cardWidget->childAt(mouseEvent->pos());
            if (clickedChild) {
                if (clickedChild->objectName() == "prevBtn" || clickedChild->objectName() == "nextBtn")
                    return false;
            }

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
            } else if (cardWidget->objectName() == "shellCard") {
                name = "shell";
            }

            if (!name.isEmpty()) {
                int clickY = mouseEvent->pos().y();
                if (clickY <= 36) {
                    emit moduleClicked(name);
                } else {
                    if (m_isExpanded && m_expandedCardName == name) {
                        collapseCard();
                    } else if (m_isExpanded && m_expandedCardName != name) {
                        switchCard(name);
                    } else if (!m_isExpanded) {
                        expandCard(name);
                    }
                }
                return true;
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

QWidget *DashboardPage::createShellCard() {
    auto *cardWidget = new QWidget();
    cardWidget->setFixedSize(280, 180);
    cardWidget->setObjectName("shellCard");
    cardWidget->setCursor(Qt::PointingHandCursor);

    auto *mainLayout = new QVBoxLayout(cardWidget);
    mainLayout->setContentsMargins(16, 10, 16, 10);
    mainLayout->setSpacing(4);

    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(6);

    auto *titleLabel = new QLabel("Shell");
    titleLabel->setStyleSheet(
        "font-size: 9pt; font-weight: bold; color: #94a3b8; background: transparent; border: none;"
    );
    topRow->addWidget(titleLabel);
    topRow->addStretch(1);

    mainLayout->addLayout(topRow);
    mainLayout->addSpacing(4);

    auto *descLabel = new QLabel("执行系统命令\n管理本地 Shell 会话");
    descLabel->setStyleSheet(
        "color: #64748b; font-size: 8pt; background: transparent; border: none;"
    );
    mainLayout->addWidget(descLabel);

    mainLayout->addStretch(1);

    auto *statusRow = new QHBoxLayout();
    statusRow->setContentsMargins(0, 0, 0, 0);

    auto *statusLabel = new QLabel("●");
    statusLabel->setStyleSheet(
        "color: #64748b; font-size: 12pt; background: transparent; border: none;"
    );
    statusRow->addWidget(statusLabel);
    statusRow->addStretch(1);

    mainLayout->addLayout(statusRow);

    cardWidget->setStyleSheet(
        "#shellCard { background-color: transparent; border: 1px solid #36383d; border-radius: 8px; }"
        "#shellCard:hover { background-color: rgba(42, 44, 48, 0.5); border-color: #0ea5e9; }"
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

void DashboardPage::expandCard(const QString &cardName) {
    if (m_isExpanded || !m_cardContainers.contains(cardName)) return;

    m_isExpanded = true;
    m_expandedCardName = cardName;

    int areaW = m_cardArea->width();
    int areaH = m_cardArea->height();
    if (areaW < 100 || areaH < 100) return;

    int padding = 16;
    int leftW = 260;
    int rightW = areaW - leftW - padding * 3;
    if (rightW < 200) rightW = 200;
    int smallCardH = 130;
    int gap = 12;

    // Remove shadow effects before animation to avoid expensive repaints
    for (auto it = m_cardContainers.constBegin(); it != m_cardContainers.constEnd(); ++it) {
        it.value()->setGraphicsEffect(nullptr);
        it.value()->setMinimumSize(0, 0);
        it.value()->setMaximumSize(16777215, 16777215);
    }

    if (m_animGroup) m_animGroup->stop();
    m_animGroup = new QParallelAnimationGroup(this);

    int leftIndex = 0;
    for (int i = 0; i < m_cardOrder.size(); ++i) {
        auto *card = m_cardContainers[m_cardOrder[i]];
        QRect endGeo;
        if (m_cardOrder[i] == cardName) {
            endGeo = QRect(leftW + padding * 2, 0, rightW, areaH);
            card->raise();
        } else {
            endGeo = QRect(padding, leftIndex * (smallCardH + gap), leftW, smallCardH);
            leftIndex++;
        }

        auto *anim = new QPropertyAnimation(card, "geometry");
        anim->setDuration(280);
        anim->setStartValue(card->geometry());
        anim->setEndValue(endGeo);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        m_animGroup->addAnimation(anim);
    }

    m_isAnimating = true;
    connect(m_animGroup.data(), &QParallelAnimationGroup::finished, this, [this]() {
        m_isAnimating = false;
    });
    m_animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void DashboardPage::switchCard(const QString &cardName) {
    if (!m_isExpanded || !m_cardContainers.contains(cardName)) return;

    m_expandedCardName = cardName;

    int areaW = m_cardArea->width();
    int areaH = m_cardArea->height();
    int padding = 16;
    int leftW = 260;
    int rightW = areaW - leftW - padding * 3;
    if (rightW < 200) rightW = 200;
    int smallCardH = 130;
    int gap = 12;

    if (m_animGroup) m_animGroup->stop();
    m_animGroup = new QParallelAnimationGroup(this);

    int leftIndex = 0;
    for (int i = 0; i < m_cardOrder.size(); ++i) {
        auto *card = m_cardContainers[m_cardOrder[i]];
        QRect endGeo;
        if (m_cardOrder[i] == cardName) {
            endGeo = QRect(leftW + padding * 2, 0, rightW, areaH);
            card->raise();
        } else {
            endGeo = QRect(padding, leftIndex * (smallCardH + gap), leftW, smallCardH);
            leftIndex++;
        }

        auto *anim = new QPropertyAnimation(card, "geometry");
        anim->setDuration(280);
        anim->setStartValue(card->geometry());
        anim->setEndValue(endGeo);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        m_animGroup->addAnimation(anim);
    }

    m_isAnimating = true;
    connect(m_animGroup.data(), &QParallelAnimationGroup::finished, this, [this]() {
        m_isAnimating = false;
    });
    m_animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void DashboardPage::collapseCard() {
    if (!m_isExpanded) return;

    int areaW = m_cardArea->width();
    int cardW = 280;
    int cardH = 180;
    int gap = 16;
    int cols = qMax(1, (areaW + gap) / (cardW + gap));
    int startX = (areaW - cols * cardW - (cols - 1) * gap) / 2;

    // Remove shadow effects before animation
    for (auto it = m_cardContainers.constBegin(); it != m_cardContainers.constEnd(); ++it) {
        it.value()->setGraphicsEffect(nullptr);
    }

    m_isExpanded = false;
    m_expandedCardName.clear();

    if (m_animGroup) m_animGroup->stop();
    m_animGroup = new QParallelAnimationGroup(this);

    for (int i = 0; i < m_cardOrder.size(); ++i) {
        auto *card = m_cardContainers[m_cardOrder[i]];
        int col = i % cols;
        int row = i / cols;
        QRect endGeo(startX + col * (cardW + gap), row * (cardH + gap), cardW, cardH);

        auto *anim = new QPropertyAnimation(card, "geometry");
        anim->setDuration(280);
        anim->setStartValue(card->geometry());
        anim->setEndValue(endGeo);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        m_animGroup->addAnimation(anim);
    }

    m_isAnimating = true;
    connect(m_animGroup.data(), &QParallelAnimationGroup::finished, this, [this]() {
        m_isAnimating = false;
        // Restore fixed sizes and shadow effects after animation completes
        for (auto it = m_cardContainers.constBegin(); it != m_cardContainers.constEnd(); ++it) {
            it.value()->setFixedSize(280, 180);
            auto *shadow = new QGraphicsDropShadowEffect();
            shadow->setBlurRadius(20);
            shadow->setOffset(0, 4);
            shadow->setColor(QColor(0, 0, 0, 60));
            it.value()->setGraphicsEffect(shadow);
        }
    });
    m_animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}
