// SPDX-License-Identifier: MIT
// Loaded temporarily by the VM identity test to expose Wayland app IDs in logs.
for (const window of workspace.windowList()) {
    console.info("AERO7_IE_WINDOW", JSON.stringify({
        caption: window.caption,
        resourceClass: window.resourceClass,
        resourceName: window.resourceName,
        desktopFileName: window.desktopFileName,
        pid: window.pid
    }));
}
