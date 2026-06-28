#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QPushButton>
#include <QMap>
#include <QLabel>
#include <QTextBrowser>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QPointer>

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

    void loadCards();

    void registerCardWidgets(const QString &name, const CardWidgets &w);
    void registerFileServerCard(QLabel *status, QLabel *addr, QPushButton *toggle);

    void updateCardStatus(const QString &moduleName, bool connected, const QString &address = QString());
    void appendCardLog(const QString &moduleName, int tabIndex, const QString &html);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

signals:
    void moduleClicked(const QString &moduleName);

private:
    QWidget *m_cardArea;
    QMap<QString, CardWidgets> m_cardWidgets;
    QMap<QString, QWidget *> m_cardContainers;
    QStringList m_cardOrder;

    QLabel *m_fsStatusLabel = nullptr;
    QLabel *m_fsAddressLabel = nullptr;
    QPushButton *m_fsToggleBtn = nullptr;

    QString m_expandedCardName;
    bool m_isExpanded = false;

    void expandCard(const QString &cardName);
    void collapseCard();
    void switchCard(const QString &cardName);

    bool m_isAnimating = false;
    QPointer<QParallelAnimationGroup> m_animGroup;
};

#endif // DASHBOARDPAGE_H
