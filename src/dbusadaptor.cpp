#include "dbusadaptor.h"

#include <QGuiApplication>
#include <QWindow>

DBusAdaptor::DBusAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
}

void DBusAdaptor::show()
{
    QWindowList windows = QGuiApplication::topLevelWindows();
    for (int i = 0; i < windows.size(); ++i) {
        QWindow *w = windows.at(i);
        if (w) {
            w->show();
            w->raise();
            w->requestActivate();
        }
    }
}
