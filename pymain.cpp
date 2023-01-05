#include "lib/python/pythonbindings.h"
#include <QString>
#include <QDir>
#include <QApplication>

static void setPyHome(const wchar_t *apploc) {
#ifdef Q_OS_MAC
    Py_SetPythonHome(QFileInfo(QString::fromWCharArray(apploc)).dir().filePath("../Resources").toStdWString().c_str());
#else
    Py_SetPythonHome(QFileInfo(QString::fromWCharArray(apploc)).dir().filePath("py").toStdWString().c_str());
#endif
}

#ifdef Q_OS_WIN
int wmain(int argc, wchar_t *argv[]) {
#else
int main(int argc, char *argv[]) {
#endif
    // HACK: we need ENSUREPIP_OPTIONS set on Unix b/c pip otherwise thinks we're using a framework
    if (qEnvironmentVariableIsEmpty("ENSUREPIP_OPTIONS")) {
        qputenv("ENSUREPIP_OPTIONS", "1");
    }
    setPyHome(argv[0]);

#ifdef Q_OS_WIN
    return Py_Main(argc, argv);
#else
    return Py_BytesMain(argc, argv);
#endif
}
