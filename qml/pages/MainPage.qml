import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    RemorsePopup { id: remorse }
    RemorsePopup { id: uninstallRemorse }

    function eventTitle(ev) {
        var user = ev.user || "unknown"
        var host = ev.rhost || "unknown"
        if (ev.type === "attempt")
            return "Attempt: " + user + " from " + host
        if (ev.type === "open")
            return "Session: " + user + " from " + host
        if (ev.type === "close") {
            var extra = ev.duration_s !== undefined ? " after " + durationText(ev.duration_s) : ""
            return "Closed: " + user + " from " + host + extra
        }
        return (ev.type || "event") + ": " + user
    }

    function durationText(secs) {
        secs = Number(secs)
        if (isNaN(secs) || secs < 0)
            return ""
        var h = Math.floor(secs / 3600)
        var m = Math.floor((secs % 3600) / 60)
        var s = Math.floor(secs % 60)
        if (h > 0)
            return h + "h " + m + "m " + s + "s"
        if (m > 0)
            return m + "m " + s + "s"
        return s + "s"
    }

    function eventTime(ev) {
        if (!ev.ts)
            return ev.ts_iso || ""
        return Format.formatDate(new Date(ev.ts * 1000), Format.Timepoint)
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: logStore.events
        currentIndex: -1

        header: Column {
            width: listView.width

            PageHeader {
                title: "SSH Notif"
            }

            SectionHeader {
                text: "Status"
            }

            Label {
                width: parent.width
                leftPadding: Theme.horizontalPageMargin
                rightPadding: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: logStore.deployed ? Theme.highlightColor : Theme.errorColor
                text: logStore.statusSummary
            }

            Label {
                width: parent.width
                leftPadding: Theme.horizontalPageMargin
                rightPadding: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: "Helper: " + (logStore.hookInstalled ? "ok" : "missing")
                      + "  PAM: " + (logStore.pamConfigured ? "ok" : "missing")
                      + "  Log: " + (logStore.logReadable ? "ok" : "unreadable")
            }

            Label {
                width: parent.width
                visible: logStore.lastError.length > 0
                leftPadding: Theme.horizontalPageMargin
                rightPadding: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: logStore.lastError
            }

            BackgroundItem {
                visible: logStore.hookInstalled
                width: parent.width
                height: Theme.itemSizeSmall
                onClicked: logStore.pamConfigured ? uninstallRemorse.execute("Removing PAM hook", function() { logStore.uninstallPam() })
                                                   : logStore.installPam()

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        verticalCenter: parent.verticalCenter
                        leftMargin: Theme.horizontalPageMargin
                        rightMargin: Theme.horizontalPageMargin
                    }
                    text: logStore.pamConfigured ? "Uninstall PAM hook" : "Install PAM hook"
                    color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
            }

            Label {
                width: parent.width
                visible: logStore.snoozed
                leftPadding: Theme.horizontalPageMargin
                rightPadding: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: "Snoozed: " + logStore.snoozeRemainingText
            }

            SectionHeader {
                text: "Events"
            }

            Label {
                visible: logStore.events.length === 0
                width: parent.width
                leftPadding: Theme.horizontalPageMargin
                rightPadding: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: "No SSH events yet. A login, a failed attempt, or a disconnect should show up here."
            }
        }

        PullDownMenu {
            MenuItem {
                text: "Clear logs"
                visible: logStore.events.length > 0
                onClicked: remorse.execute("Clearing logs", function() { logStore.clearLogs() })
            }
            MenuItem {
                text: "Refresh"
                onClicked: logStore.refresh()
            }
            MenuItem {
                text: "Clear snooze"
                visible: logStore.snoozed
                onClicked: logStore.clearSnooze()
            }
            MenuItem {
                text: "Snooze 15 minutes"
                onClicked: logStore.snoozeMinutes(15)
            }
            MenuItem {
                text: "Snooze 30 minutes"
                onClicked: logStore.snoozeMinutes(30)
            }
            MenuItem {
                text: "Snooze 1 hour"
                onClicked: logStore.snoozeMinutes(60)
            }
            MenuItem {
                text: "Snooze 2 hours"
                onClicked: logStore.snoozeMinutes(120)
            }
        }

        delegate: ListItem {
            id: delegate
            width: listView.width
            contentHeight: col.height + Theme.paddingMedium

            onClicked: pageStack.animatorPush(Qt.resolvedUrl("EventPage.qml"), { event: modelData })

            Column {
                id: col
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    text: eventTitle(modelData)
                    truncationMode: TruncationMode.Fade
                    color: delegate.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    text: eventTime(modelData)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: delegate.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
