#include <QtQuick>
#include <QDBusConnection>
#include <sailfishapp.h>

#include "dbusadaptor.h"
#include "logstore.h"

Q_DECL_EXPORT int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName(QString());
    app->setOrganizationDomain(QStringLiteral("sshnotif"));
    app->setApplicationName(QStringLiteral("sshnotif"));

    QScopedPointer<QQuickView> view(SailfishApp::createView());

    LogStore store;
    view->rootContext()->setContextProperty(QStringLiteral("logStore"), &store);

    DBusAdaptor adaptor(view.data());
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.registerService(QStringLiteral("org.sshnotif"));
    bus.registerObject(QStringLiteral("/org/sshnotif"), view.data(),
                       QDBusConnection::ExportAdaptors);

    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}
