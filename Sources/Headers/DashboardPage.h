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

    void updateCardStatus(const QString &moduleName, bool connected);
    void appendCardLog(const QString &moduleName, int tabIndex, const QString &html);
    void updateCardTabInfo(const QString &moduleName, int current, int total);
    void setCardLogFromTab(const QString &moduleName, const QString &html);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void moduleClicked(const QString &moduleName);
    void cardPrevClicked(const QString &moduleName);
    void cardNextClicked(const QString &moduleName);
    void fileServerToggled(bool start);

private:
    QGridLayout *m_gridLayout;
    QMap<QString, CardWidgets> m_cardWidgets;
    QMap<QString, int> m_cardTabIndices;
    QWidget *createCard(const ModuleCard &card);
    QWidget *createTranslateCard();
    QWidget *createFileServerCard();

    QTextEdit *m_translateInput = nullptr;
    QLabel *m_translateResult = nullptr;
    QLabel *m_translateStatus = nullptr;
    QPushButton *m_translateBtn = nullptr;
    QNetworkAccessManager *m_networkManager = nullptr;

    QPushButton *m_serverToggleBtn = nullptr;
    QLabel *m_serverStatusLabel = nullptr;
    QLabel *m_serverAddressLabel = nullptr;

    void doTranslate(const QString &text);
};

#endif // DASHBOARDPAGE_H
