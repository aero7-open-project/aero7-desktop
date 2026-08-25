// SPDX-License-Identifier: MIT
// Closes only the synthetic windows opened by the identity VM test.
for (const window of workspace.windowList()) {
    if (window.caption.includes("AERO7_IDENTITY_TEST")) {
        window.closeWindow();
    }
}
