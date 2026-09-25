TARGET = sshnotif

CONFIG += sailfishapp
QT += dbus

SOURCES += \
    src/sshnotif.cpp \
    src/logstore.cpp \
    src/dbusadaptor.cpp

HEADERS += \
    src/logstore.h \
    src/dbusadaptor.h

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172 256x256 512x512

desktop.files = sshnotif.desktop

pamhook.target = $$PWD/helper/pam-hook
pamhook.depends = $$PWD/helper/pam-hook.go $$PWD/helper/go.mod
pamhook.commands = CGO_ENABLED=0 GOPROXY=off GOFLAGS=-buildvcs=false GOCACHE=$$OUT_PWD/.gocache go build -C $$PWD/helper -o pam-hook
QMAKE_EXTRA_TARGETS += pamhook
PRE_TARGETDEPS += $$PWD/helper/pam-hook

pamhookbin.path = /usr/libexec/sshnotif
pamhookbin.files = $$PWD/helper/pam-hook
pamhookbin.CONFIG += no_check_exist
INSTALLS += pamhookbin

pamsetup.target = pam-setup
pamsetup.depends = $$PWD/helper/pam-setup.c
pamsetup.commands = $$QMAKE_CC -O2 -Wall -o $$OUT_PWD/pam-setup $$PWD/helper/pam-setup.c
QMAKE_EXTRA_TARGETS += pamsetup
PRE_TARGETDEPS += pam-setup

pamsetupbin.path = /usr/libexec/sshnotif
pamsetupbin.files = $$OUT_PWD/pam-setup
pamsetupbin.CONFIG += no_check_exist
INSTALLS += pamsetupbin

dbusservice.path = /usr/share/dbus-1/services
dbusservice.files = dbus/org.sshnotif.service
INSTALLS += dbusservice

OTHER_FILES += \
    qml/*.qml \
    qml/pages/*.qml \
    rpm/sshnotif.yaml \
    sshnotif.desktop \
    icons/sshnotif.svg \
    helper/pam-hook.go \
    helper/go.mod \
    helper/pam-setup.c \
    dbus/org.sshnotif.service
