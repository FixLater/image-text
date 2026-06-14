#include "DashboardPage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>

DashboardPage::DashboardPage(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(32, 24, 32, 24);
    mainLayout->setSpacing(0);

    auto *titleLabel = new QLabel("Dashboard");
    titleLabel->setStyleSheet(
        "font-size: 20pt; font-weight: bold; color: #e2e8f0; background: transparent; border: none; padding-bottom: 4px;"
    );
    mainLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel("Select a module to get started");
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
    wsCard.description = "Connect to WebSocket servers, send messages and files in real-time";

    m_gridLayout->addWidget(createCard(wsCard), 0, 0);

    mainLayout->addLayout(m_gridLayout);
    mainLayout->addStretch(1);
}

QWidget *DashboardPage::createCard(const ModuleCard &card) {
    auto *cardWidget = new QPushButton();
    cardWidget->setFixedSize(260, 160);
    cardWidget->setCursor(Qt::PointingHandCursor);
    cardWidget->setObjectName("moduleCard");

    auto *layout = new QVBoxLayout(cardWidget);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(0);

    auto *iconLabel = new QLabel(card.icon);
    iconLabel->setStyleSheet(
        "font-size: 24pt; background: transparent; border: none;"
    );
    layout->addWidget(iconLabel);

    layout->addSpacing(12);

    auto *titleLabel = new QLabel(card.title);
    titleLabel->setStyleSheet(
        "font-size: 12pt; font-weight: bold; color: #e2e8f0; background: transparent; border: none;"
    );
    layout->addWidget(titleLabel);

    layout->addSpacing(6);

    auto *descLabel = new QLabel(card.description);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(
        "font-size: 8pt; color: #64748b; background: transparent; border: none;"
    );
    layout->addWidget(descLabel);

    layout->addStretch(1);

    cardWidget->setStyleSheet(
        "#moduleCard {"
        "  background-color: #26282c; border: 1px solid #36383d; border-radius: 10px;"
        "  text-align: left;"
        "}"
        "#moduleCard:hover {"
        "  background-color: #2a2c30; border-color: #0ea5e9;"
        "}"
        "#moduleCard:pressed {"
        "  background-color: #1f2937; border-color: #0ea5e9;"
        "}"
    );

    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    cardWidget->setGraphicsEffect(shadow);

    connect(cardWidget, &QPushButton::clicked, this, [this, card]() {
        emit moduleClicked(card.name);
    });

    return cardWidget;
}
