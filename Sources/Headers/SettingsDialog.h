#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QSettings>
#include <QLineEdit>
#include <QSpinBox>
#include <QMap>
#include <QPoint>

struct ConfigField {
    QString key;
    QString label;
    QString tooltip;
    enum Type { String, Int } type;
    QVariant defaultValue;
};

struct ConfigModule {
    QString name;
    QString icon;
    QList<ConfigField> fields;
};

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    static QSettings *settings();

    static QString wsUrl();
    static int wsPingIntervalMs();
    static int wsReconnectIntervalMs();
    static int wsMaxReconnectAttempts();
    static int wsJwtTokenLength();
    static QString wsRoomId();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QTabWidget *m_tabWidget;
    bool m_dragging;
    QPoint m_dragPos;

    void buildTabs();
    QWidget *createModuleTab(const ConfigModule &module);
    void applyDialogStyle();

    static QList<ConfigModule> buildModules();

private slots:
    void onSave();
};

#endif // SETTINGSDIALOG_H
