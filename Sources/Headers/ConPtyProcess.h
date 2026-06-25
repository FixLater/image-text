#ifndef CONPTYPROCESS_H
#define CONPTYPROCESS_H

#include <QObject>
#include <QByteArray>
#include <QThread>
#include <QMutex>
#include <functional>

#ifdef Q_OS_WIN
#include <windows.h>

#ifndef HPCON
typedef VOID *HPCON;
#endif

#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#ifndef ProcThreadAttributeValue
#define ProcThreadAttributeValue(Number, Thread, Input, Unused) \
    (((Number) & 0x0000FFFF) | ((Thread) ? 0x00010000 : 0) | ((Input) ? 0x00020000 : 0))
#endif
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

class ConPtyReader : public QThread {
    Q_OBJECT
public:
    explicit ConPtyReader(HANDLE hPipe, QObject *parent = nullptr);
    void stop();
signals:
    void dataReady(const QByteArray &data);
protected:
    void run() override;
private:
    HANDLE m_hPipe;
    bool m_running = true;
};

class ConPtyProcess : public QObject {
    Q_OBJECT
public:
    explicit ConPtyProcess(QObject *parent = nullptr);
    ~ConPtyProcess() override;

    bool start(const QString &program, const QStringList &args, const QString &workDir = QString());
    void write(const QByteArray &data);
    bool isRunning() const;
    void kill();

signals:
    void readyRead(const QByteArray &data);
    void finished(int exitCode);
    void errorOccurred(const QString &error);

private:
    HANDLE m_hPipe_in = nullptr;
    HANDLE m_hPipe_out = nullptr;
    HPCON m_hPC = nullptr;
    PROCESS_INFORMATION m_processInfo = {};
    ConPtyReader *m_reader = nullptr;
    bool m_running = false;

    void cleanup();
};

#else

class ConPtyProcess : public QObject {
    Q_OBJECT
public:
    explicit ConPtyProcess(QObject *parent = nullptr) : QObject(parent) {}
    bool start(const QString &, const QStringList &, const QString & = QString()) { return false; }
    void write(const QByteArray &) {}
    bool isRunning() const { return false; }
    void kill() {}
signals:
    void readyRead(const QByteArray &);
    void finished(int);
    void errorOccurred(const QString &);
};

#endif

#endif // CONPTYPROCESS_H
