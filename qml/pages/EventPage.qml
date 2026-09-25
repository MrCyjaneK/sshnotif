import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    property var event: ({})

    function durationText(secs) {
        secs = Number(secs)
        if (isNaN(secs) || secs < 0)
            return "—"
        var h = Math.floor(secs / 3600)
        var m = Math.floor((secs % 3600) / 60)
        var s = Math.floor(secs % 60)
        if (h > 0)
            return h + "h " + m + "m " + s + "s"
        if (m > 0)
            return m + "m " + s + "s"
        return s + "s"
    }

    function typeLabel(t) {
        if (t === "attempt")
            return "Auth attempt"
        if (t === "open")
            return "Session opened"
        if (t === "close")
            return "Session closed"
        return t || "—"
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: typeLabel(event.type)
            }

            DetailItem {
                label: "User"
                value: event.user || "—"
            }
            DetailItem {
                label: "Remote host"
                value: event.rhost || "—"
            }
            DetailItem {
                label: "TTY"
                value: event.tty || "—"
            }
            DetailItem {
                label: "Time"
                value: event.ts ? Format.formatDate(new Date(event.ts * 1000), Format.Timepoint) : (event.ts_iso || "—")
            }
            DetailItem {
                visible: event.duration_s !== undefined
                label: "Duration"
                value: durationText(event.duration_s)
            }
            DetailItem {
                label: "PID"
                value: event.pid ? String(event.pid) : "—"
            }
            DetailItem {
                label: "Event id"
                value: event.id || "—"
            }
        }

        VerticalScrollDecorator {}
    }
}
