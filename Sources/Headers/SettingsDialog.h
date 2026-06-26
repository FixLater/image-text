#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QStackedWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QSettings>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMap>
#include <QPoint>
#include <QStringList>

class ConfigService;

struct ConfigField {
    QString key;
    QString label;
    QString tooltip;
    enum Type { String, Int, Bool, Choice, DynamicChoice } type;
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
    static ConfigService *configService();

    static QString wsUrl();
    static int wsPingIntervalMs();
    static int wsReconnectIntervalMs();
    static int wsMaxReconnectAttempts();
    static int wsJwtTokenLength();
    static bool wsAutoJoinDefaultRoom();
    static QString wsDefaultRoomId();

    static bool exitWithoutReminder();
    static int closeAction();

    static QString translateSourceLang();
    static QString translateTargetLang();
    static int fileServerPort();
    static bool fileServerAllowLan();
    static bool fileServerAutoStart();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QListWidget *m_navList;
    QStackedWidget *m_stack;
    bool m_dragging;
    QPoint m_dragPos;
    QLabel *m_serverStatusLabel = nullptr;
    QComboBox *m_defaultRoomCombo = nullptr;

    void buildNav();

    QWidget *createModuleTab(const ConfigModule &module);
    void applyDialogStyle();

    static QList<ConfigModule> buildModules();

private slots:
    void onSave();
    void onLoadFromServer(const QMap<QString, QString> &configs);
    void onRoomListReceived(const QStringList &rooms);
};

#endif // SETTINGSDIALOG_H
