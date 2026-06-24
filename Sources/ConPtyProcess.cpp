#include "ConPtyProcess.h"

#ifdef Q_OS_WIN

#include <QDir>

typedef HRESULT (WINAPI *PFNCREATEPSEUDOCONSOLE)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
typedef VOID (WINAPI *PFNCONSOLEDISCONNECTPCONSOLE)(HPCON);
typedef HRESULT (WINAPI *PFNRESIZEPSEUDOCONSOLE)(HPCON, COORD);

static PFNCREATEPSEUDOCONSOLE pfnCreatePseudoConsole = nullptr;
static PFNCONSOLEDISCONNECTPCONSOLE pfnConsoleDisconnectPseudoConsole = nullptr;
static bool g_conptyLoaded = false;
static bool g_conptyAttempted = false;

static bool loadConPty() {
    if (g_conptyAttempted) return g_conptyLoaded;
    g_conptyAttempted = true;

    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) return false;

    pfnCreatePseudoConsole = (PFNCREATEPSEUDOCONSOLE)GetProcAddress(hKernel32, "CreatePseudoConsole");
    pfnConsoleDisconnectPseudoConsole = (PFNCONSOLEDISCONNECTPCONSOLE)GetProcAddress(hKernel32, "ConsoleDisconnectPseudoConsole");

    g_conptyLoaded = (pfnCreatePseudoConsole != nullptr);
    return g_conptyLoaded;
}

ConPtyReader::ConPtyReader(HANDLE hPipe, QObject *parent)
    : QThread(parent), m_hPipe(hPipe) {}

void ConPtyReader::stop() {
    m_running = false;
}

void ConPtyReader::run() {
    char buf[4096];
    while (m_running) {
        DWORD bytesRead = 0;
        BOOL ok = ReadFile(m_hPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            if (!m_running) break;
            Sleep(10);
            continue;
        }
        buf[bytesRead] = '\0';
        emit dataReady(QByteArray(buf, bytesRead));
    }
}

ConPtyProcess::ConPtyProcess(QObject *parent)
    : QObject(parent) {}

ConPtyProcess::~ConPtyProcess() {
    kill();
}

bool ConPtyProcess::start(const QString &program, const QStringList &args, const QString &workDir) {
    if (!loadConPty()) {
        emit errorOccurred("ConPTY is not available on this Windows version");
        return false;
    }

    cleanup();

    HANDLE hPipe_in_Read = nullptr, hPipe_in_Write = nullptr;
    HANDLE hPipe_out_Read = nullptr, hPipe_out_Write = nullptr;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    CreatePipe(&hPipe_in_Read, &hPipe_in_Write, &sa, 0);
    CreatePipe(&hPipe_out_Read, &hPipe_out_Write, &sa, 0);

    COORD size = { 120, 30 };
    HRESULT hr = pfnCreatePseudoConsole(size, hPipe_in_Read, hPipe_out_Write, 0, &m_hPC);
    if (FAILED(hr)) {
        CloseHandle(hPipe_in_Read);
        CloseHandle(hPipe_in_Write);
        CloseHandle(hPipe_out_Read);
        CloseHandle(hPipe_out_Write);
        emit errorOccurred("Failed to create pseudo console");
        return false;
    }

    CloseHandle(hPipe_in_Read);
    CloseHandle(hPipe_out_Write);

    m_hPipe_in = hPipe_in_Write;
    m_hPipe_out = hPipe_out_Read;

    QString cmdLine = program;
    for (const QString &arg : args) {
        cmdLine += " \"" + arg + "\"";
    }

    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);

    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
    QByteArray attrListBuf(attrListSize, 0);
    si.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrListBuf.data());
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrListSize);

    UpdateProcThreadAttribute(si.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, m_hPC, sizeof(HPCON), nullptr, nullptr);

    QString workDirStr = workDir.isEmpty() ? QDir::currentPath() : workDir;
    std::wstring cmdW = cmdLine.toStdWString();
    std::wstring dirW = workDirStr.toStdWString();

    BOOL ok = CreateProcessW(nullptr, cmdW.data(), nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT, nullptr, dirW.data(),
        &si.StartupInfo, &m_processInfo);

    DeleteProcThreadAttributeList(si.lpAttributeList);

    if (!ok) {
        cleanup();
        emit errorOccurred("Failed to start process");
        return false;
    }

    m_running = true;

    m_reader = new ConPtyReader(m_hPipe_out, this);
    connect(m_reader, &ConPtyReader::dataReady, this, &ConPtyProcess::readyRead);
    connect(m_reader, &ConPtyReader::finished, this, [this]() {
        if (m_processInfo.hProcess) {
            WaitForSingleObject(m_processInfo.hProcess, INFINITE);
            DWORD exitCode = 0;
            GetExitCodeProcess(m_processInfo.hProcess, &exitCode);
            m_running = false;
            emit finished(static_cast<int>(exitCode));
        }
    });
    m_reader->start();

    return true;
}

void ConPtyProcess::write(const QByteArray &data) {
    if (m_hPipe_in) {
        DWORD written = 0;
        WriteFile(m_hPipe_in, data.constData(), data.size(), &written, nullptr);
    }
}

bool ConPtyProcess::isRunning() const {
    return m_running;
}

void ConPtyProcess::kill() {
    m_running = false;

    if (m_reader) {
        m_reader->stop();
        m_reader->wait(2000);
        m_reader->deleteLater();
        m_reader = nullptr;
    }

    if (m_processInfo.hProcess) {
        TerminateProcess(m_processInfo.hProcess, 1);
        WaitForSingleObject(m_processInfo.hProcess, 1000);
        CloseHandle(m_processInfo.hProcess);
        CloseHandle(m_processInfo.hThread);
        m_processInfo = {};
    }

    cleanup();
}

void ConPtyProcess::cleanup() {
    if (m_hPC && pfnConsoleDisconnectPseudoConsole) {
        pfnConsoleDisconnectPseudoConsole(m_hPC);
        m_hPC = nullptr;
    }
    if (m_hPipe_in) { CloseHandle(m_hPipe_in); m_hPipe_in = nullptr; }
    if (m_hPipe_out) { CloseHandle(m_hPipe_out); m_hPipe_out = nullptr; }
}

#endif
