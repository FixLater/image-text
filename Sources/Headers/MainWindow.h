#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTimer>

class SettingsDialog;
class DashboardPage;
class WebSocketPage;
class TranslationPage;
class StarBackground;

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

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

    DashboardPage *m_dashboardPage;
    WebSocketPage *m_websocketPage;
    TranslationPage *m_translationPage;
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

    void rebuildTabBar();
    void onTabAddClicked();
    void onTabClicked(int index);
    void onTabCloseClicked(int index);
    void showTabBar(bool show);
    void updateDashboardTabInfo();
    int m_activeTabIndex = -1;
};

#endif // MAINWINDOW_H
