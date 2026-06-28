#ifndef UNIFIEDCARD_H
#define UNIFIEDCARD_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>

class DashboardPage;

struct UnifiedCardLayout {
    QWidget *card;
    QLabel *titleLabel;
    QLabel *statusLabel;
    QPushButton *expandBtn;
    QWidget *contentArea;
    QHBoxLayout *bottomRow;
};

inline UnifiedCardLayout createUnifiedCard(QWidget *parent, const QString &title,
                                           DashboardPage *db, const QString &moduleName,
                                           bool showSep = true) {
    UnifiedCardLayout u;

    u.card = new QWidget(parent);
    u.card->setObjectName("moduleCard");
    u.card->setCursor(Qt::PointingHandCursor);
    u.card->setFixedSize(280, 180);

    auto *mainLayout = new QVBoxLayout(u.card);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto *headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(12, 6, 8, 6);
    headerRow->setSpacing(4);

    u.titleLabel = new QLabel(title);
    u.titleLabel->setStyleSheet(
        "font-size: 9pt; font-weight: bold; color: #94a3b8;"
        "background: transparent; border: none;");
    headerRow->addWidget(u.titleLabel);

    u.statusLabel = new QLabel();
    u.statusLabel->setStyleSheet(
        "background-color: #64748b; border-radius: 3px; min-width: 6px; max-width: 6px;"
        "min-height: 6px; max-height: 6px; border: none; margin-left: 4px;");
    headerRow->addWidget(u.statusLabel);
    headerRow->addStretch(1);

    u.expandBtn = new QPushButton();
    u.expandBtn->setObjectName("expandBtn");
    u.expandBtn->setFixedSize(20, 20);
    u.expandBtn->setCursor(Qt::PointingHandCursor);
    u.expandBtn->setToolTip(QString::fromUtf8("\xe6\x94\xbe\xe5\xa4\xa7"));
    u.expandBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 3px; }"
        "QPushButton:hover { background-color: #36383d; }");
    u.expandBtn->setIcon(QIcon(":/icons/maximize.svg"));
    u.expandBtn->setIconSize(QSize(12, 12));
    headerRow->addWidget(u.expandBtn);
    mainLayout->addLayout(headerRow);

    if (showSep) {
        auto *sepContainer = new QWidget();
        sepContainer->setFixedHeight(1);
        auto *sepLayout = new QHBoxLayout(sepContainer);
        sepLayout->setContentsMargins(12, 0, 12, 0);
        sepLayout->setSpacing(0);
        auto *sep = new QWidget();
        sep->setFixedHeight(1);
        sep->setStyleSheet("background-color: #36383d;");
        sepLayout->addWidget(sep);
        mainLayout->addWidget(sepContainer);
    }

    u.contentArea = new QWidget();
    u.contentArea->setStyleSheet("background: transparent; border: none;");
    mainLayout->addWidget(u.contentArea, 1);

    u.bottomRow = new QHBoxLayout();
    u.bottomRow->setContentsMargins(12, 4, 12, 6);
    u.bottomRow->setSpacing(6);
    mainLayout->addLayout(u.bottomRow);

    u.card->setStyleSheet(
        "#moduleCard { background-color: #1e1f22; border: 1px solid #36383d; border-radius: 8px; }"
        "#moduleCard:hover { border-color: #0ea5e9; }");

    auto *shadow = new QGraphicsDropShadowEffect();
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 60));
    u.card->setGraphicsEffect(shadow);

    if (db) {
        QObject::connect(u.expandBtn, &QPushButton::clicked, db, [db, moduleName]() {
            db->toggleCardExpansion(moduleName);
        });
    }

    return u;
}

#endif // UNIFIEDCARD_H
