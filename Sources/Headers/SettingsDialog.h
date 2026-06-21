#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QStackedWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QSettings>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMap>
#include <QPoint>

struct ConfigField {
    QString key;
    QString label;
    QString tooltip;
    enum Type { String, Int, Bool, Choice } type;
    QVariant defaultValue;
    QStringList choices;
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

    static bool exitWithoutReminder();
    static int closeAction();

    static QString translateSourceLang();
    static QString translateTargetLang();
    static int fileServerPort();
    static bool fileServerAllowLan();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QListWidget *m_navList;
    QStackedWidget *m_stack;
    bool m_dragging;
    QPoint m_dragPos;

    void buildNav();

    static QWidget *createModuleTab(const ConfigModule &module);
    void applyDialogStyle();

    static QList<ConfigModule> buildModules();

private slots:
    void onSave();
};

#endif // SETTINGSDIALOG_H
