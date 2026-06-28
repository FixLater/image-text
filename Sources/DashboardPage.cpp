#include "DashboardPage.h"
#include "ModuleInfo.h"
#include "ModuleRegistry.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimer>
#include <QTextCursor>

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 24, 32, 24);
    mainLayout->setSpacing(0);

    m_cardArea = new QWidget(this);
    m_cardArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_cardArea);

    QTimer::singleShot(0, this, [this]() { loadCards(); resizeEvent(nullptr); });
}

void DashboardPage::loadCards() {
    const auto &modules = ModuleRegistry::modules();
    for (const auto &mod : modules) {
        QWidget *card = mod.createCard(m_cardArea, this);
        card->setParent(m_cardArea);
        card->installEventFilter(this);
        m_cardContainers[mod.name] = card;
        m_cardOrder << mod.name;
    }
}

void DashboardPage::registerCardWidgets(const QString &name, const CardWidgets &w) {
    m_cardWidgets[name] = w;
}

void DashboardPage::registerFileServerCard(QLabel *status, QLabel *addr, QPushButton *toggle) {
    m_fsStatusLabel = status;
    m_fsAddressLabel = addr;
    m_fsToggleBtn = toggle;
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
            auto *clickedChild = qobject_cast<QWidget*>(obj);
            if (!clickedChild) return false;

            if (qobject_cast<QPushButton *>(clickedChild)) return false;

            QWidget *cardWidget = clickedChild;
            while (cardWidget && !m_cardContainers.values().contains(cardWidget)) {
                cardWidget = cardWidget->parentWidget();
            }
            if (!cardWidget) return false;

            for (auto it = m_cardContainers.constBegin(); it != m_cardContainers.constEnd(); ++it) {
                if (it.value() == cardWidget) {
                    QPoint cardPos = clickedChild->mapTo(cardWidget, mouseEvent->pos());
                    int clickY = cardPos.y();
                    if (clickY <= 36) {
                        emit moduleClicked(it.key());
                    } else {
                        if (m_isExpanded && m_expandedCardName == it.key()) {
                            collapseCard();
                        } else if (m_isExpanded && m_expandedCardName != it.key()) {
                            switchCard(it.key());
                        } else if (!m_isExpanded) {
                            expandCard(it.key());
                        }
                    }
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DashboardPage::updateCardStatus(const QString &moduleName, bool connected, const QString &address) {
    if (moduleName == "fileserver") {
        if (!m_fsStatusLabel) return;
        if (connected) {
            m_fsStatusLabel->setStyleSheet(
                "color: #34d399; font-size: 12pt; background: transparent; border: none;");
            m_fsToggleBtn->setText(QString::fromUtf8("\xe5\x81\x9c\xe6\xad\xa2"));
            m_fsToggleBtn->setStyleSheet(
                "QPushButton { background-color: #dc2626; color: white; border: none; border-radius: 4px;"
                "  font-size: 9pt; padding: 4px 12px; }"
                "QPushButton:hover { background-color: #ef4444; }");
            m_fsAddressLabel->setText(QString("<a href=\"%1\" style=\"color: #0ea5e9; text-decoration: underline;\">%1</a>").arg(address));
            m_fsAddressLabel->setStyleSheet("font-size: 8pt; background: transparent; border: none;");
            m_fsAddressLabel->setOpenExternalLinks(true);
            m_fsAddressLabel->setCursor(Qt::PointingHandCursor);
        } else {
            m_fsStatusLabel->setStyleSheet(
                "color: #64748b; font-size: 12pt; background: transparent; border: none;");
            m_fsToggleBtn->setText(QString::fromUtf8("\xe5\x90\xaf\xe5\x8a\xa8"));
            m_fsToggleBtn->setStyleSheet(
                "QPushButton { background-color: #0ea5e9; color: white; border: none; border-radius: 4px;"
                "  font-size: 9pt; padding: 4px 12px; }"
                "QPushButton:hover { background-color: #38bdf8; }");
            m_fsAddressLabel->setText(QString::fromUtf8("\xe6\x9c\xaa\xe8\xbf\x90\xe8\xa1\x8c"));
            m_fsAddressLabel->setStyleSheet(
                "color: #475569; font-size: 8pt; background: transparent; border: none;");
        }
        return;
    }
    if (!m_cardWidgets.contains(moduleName)) return;
    auto &widgets = m_cardWidgets[moduleName];
    if (connected) {
        widgets.statusLabel->setStyleSheet(
            "background-color: #34d399; border-radius: 4px; min-width: 8px; max-width: 8px;"
            "min-height: 8px; max-height: 8px; border: none;");
    } else {
        widgets.statusLabel->setStyleSheet(
            "background-color: #64748b; border-radius: 4px; min-width: 8px; max-width: 8px;"
            "min-height: 8px; max-height: 8px; border: none;");
    }
}

void DashboardPage::appendCardLog(const QString &moduleName, int tabIndex, const QString &html) {
    if (!m_cardWidgets.contains(moduleName)) return;
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
