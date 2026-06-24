#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QMap>
#include <QLabel>
#include <QTextBrowser>
#include <QTextEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QTimer>
#include <QSequentialAnimationGroup>

struct ModuleCard {
    QString name;
    QString icon;
    QString title;
    QString description;
};

struct CardWidgets {
    QLabel *statusLabel = nullptr;
    QTextBrowser *logPreview = nullptr;
    QPushButton *prevBtn = nullptr;
    QPushButton *nextBtn = nullptr;
    QLabel *tabLabel = nullptr;
};

class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr);

    void updateCardStatus(const QString &moduleName, bool connected, const QString &address = QString());
    void appendCardLog(const QString &moduleName, int tabIndex, const QString &html);
    void updateCardTabInfo(const QString &moduleName, int current, int total);
    void setCardLogFromTab(const QString &moduleName, const QString &html);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void moduleClicked(const QString &moduleName);
    void cardPrevClicked(const QString &moduleName);
    void cardNextClicked(const QString &moduleName);
    void fileServerToggled(bool start);

private:
    QWidget *m_cardArea;
    QGridLayout *m_gridLayout;
    QMap<QString, CardWidgets> m_cardWidgets;
    QMap<QString, int> m_cardTabIndices;
    QMap<QString, QWidget *> m_cardContainers;
    QStringList m_cardOrder;
    QWidget *createCard(const ModuleCard &card);
    QWidget *createTranslateCard();
    QWidget *createFileServerCard();
    QWidget *createShellCard();

    QTextEdit *m_translateInput = nullptr;
    QLabel *m_translateResult = nullptr;
    QLabel *m_translateStatus = nullptr;
    QPushButton *m_translateBtn = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;

    QPushButton *m_serverToggleBtn = nullptr;
    QLabel *m_serverStatusLabel = nullptr;
    QLabel *m_serverAddressLabel = nullptr;

    void doTranslate(const QString &text);

    QString m_expandedCardName;
    bool m_isExpanded = false;

    void expandCard(const QString &cardName);
    void collapseCard();
    void switchCard(const QString &cardName);

    struct AnimCard {
        QWidget *widget;
        QRect startGeo;
        QRect endGeo;
    };
    QTimer *m_animTimer = nullptr;
    QList<AnimCard> m_animCards;
    int m_animStep = 0;
    int m_animTotalSteps = 20;
    void startAnimation(const QList<AnimCard> &cards);
    void animationTick();

    QPointer<QParallelAnimationGroup> m_animGroup;
};

#endif // DASHBOARDPAGE_H
