#ifndef MODULEINFO_H
#define MODULEINFO_H

#include <QString>
#include <QWidget>
#include <functional>

class DashboardPage;

struct ModuleInfo {
    QString name;
    QString sidebarIcon;
    QString sidebarToolTip;
    QString breadcrumb;

    std::function<QWidget*()> createPage;
    std::function<QWidget*(QWidget* cardArea, DashboardPage* db)> createCard;
    std::function<void(QWidget* page, DashboardPage* db)> connectSignals;
};

#endif // MODULEINFO_H
