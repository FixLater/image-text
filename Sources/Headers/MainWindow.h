#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QMap>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>
#include <QPropertyAnimation>

class SettingsDialog;
class DashboardPage;
class StarBackground;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
    Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth)

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    int sidebarWidth() const;
    void setSidebarWidth(int width);

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void navigateTo(const QString &moduleName);
    void navigateBack();
    void onModuleClicked(const QString &moduleName);
    void onSettingsClicked();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void flashTrayIcon();
    void onNewMessage(const QString &message);

private:
    Ui::MainWindow *ui;
    QPoint m_dragPosition;
    bool m_isDragging = false;

    DashboardPage *m_dashboardPage = nullptr;
    QMap<QString, QWidget*> m_pages;
    StarBackground *m_starBg;

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QTimer *m_flashTimer;
    bool m_flashState = false;
    QString m_lastMessage;

    void initPages();
    void updateBreadcrumb(const QString &path);
    bool isInTitleBarArea(const QPoint &pos) const;
    void applyTitleBarStyle();
    void setupSystemTray();

    bool m_sidebarVisible = false;
    QVector<QPushButton*> m_sidebarButtons;

    void toggleSidebar();
    void setupLeftSidebarIcons();
    void animateSidebar(bool show);
    void updateSidebarSelection(const QString &active);

    QPropertyAnimation *m_sidebarAnimation = nullptr;
};

#endif // MAINWINDOW_H
