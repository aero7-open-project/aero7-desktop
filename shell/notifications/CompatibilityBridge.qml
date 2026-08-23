// SPDX-License-Identifier: MIT
import QtQuick
import org.kde.notificationmanager as NotificationManager

Item {
    id: root
    visible: false

    function value(row, role) {
        return watched.data(watched.index(row, 0), role)
    }

    function publish(row) {
        const icon = value(row, NotificationManager.Notifications.IconNameRole)
                     || value(row, NotificationManager.Notifications.ApplicationIconNameRole)
                     || "dialog-information"
        const created = value(row, NotificationManager.Notifications.CreatedRole)
        notificationServer.ingestCompatibility(
            Number(value(row, NotificationManager.Notifications.IdRole)),
            String(value(row, NotificationManager.Notifications.ApplicationNameRole) || "Application"),
            String(icon),
            String(value(row, NotificationManager.Notifications.SummaryRole) || ""),
            String(value(row, NotificationManager.Notifications.BodyRole) || ""),
            value(row, NotificationManager.Notifications.ActionNamesRole) || [],
            value(row, NotificationManager.Notifications.ActionLabelsRole) || [],
            Number(value(row, NotificationManager.Notifications.UrgencyRole) || 1),
            Number(value(row, NotificationManager.Notifications.TimeoutRole)),
            created ? created.toISOString() : "")
    }

    NotificationManager.WatchedNotificationsModel {
        id: watched

        onRowsInserted: function(parent, first, last) {
            for (let row = first; row <= last; ++row)
                root.publish(row)
        }
        onDataChanged: function(topLeft, bottomRight, roles) {
            for (let row = topLeft.row; row <= bottomRight.row; ++row)
                root.publish(row)
        }
    }

    Connections {
        target: notificationServer
        function onCompatibilityActionRequested(id, actionKey) {
            watched.invokeAction(id, actionKey, NotificationManager.Notifications.Close)
        }
    }

    Component.onCompleted: {
        for (let row = 0; row < watched.rowCount(); ++row)
            publish(row)
    }
}
