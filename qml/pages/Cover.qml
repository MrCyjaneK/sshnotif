import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    function lastText() {
        if (!logStore.events.length)
            return "No SSH events yet"
        var ev = logStore.events[0]
        var user = ev.user || "?"
        var host = ev.rhost || "?"
        if (ev.type === "attempt")
            return "Attempt\n" + user + " @ " + host
        if (ev.type === "open")
            return "Session\n" + user + " @ " + host
        if (ev.type === "close")
            return "Closed\n" + user + " @ " + host
        return user + " @ " + host
    }

    CoverPlaceholder {
        icon.source: "/usr/share/icons/hicolor/86x86/apps/sshnotif.png"
        text: logStore.snoozed
              ? ("Snoozed\n" + logStore.snoozeRemainingText)
              : (logStore.deployed ? lastText() : "Not deployed")
    }
}
