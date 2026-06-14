#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QPushButton>

struct ModuleCard {
    QString name;
    QString icon;
    QString title;
    QString description;
};

class DashboardPage : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget *parent = nullptr);

signals:
    void moduleClicked(const QString &moduleName);

private:
    QGridLayout *m_gridLayout;
    QWidget *createCard(const ModuleCard &card);
};

#endif // DASHBOARDPAGE_H
