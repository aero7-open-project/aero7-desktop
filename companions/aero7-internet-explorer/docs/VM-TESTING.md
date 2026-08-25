# VM testing

The release gate requires an Aero7 Plasma 6 Wayland guest and two installed
browsers. The test sequence must record evidence for:

1. selecting backend A in Control Panel;
2. pinning Internet Explorer and launching a real URL;
3. preserving the Start and pinned Internet Explorer identity;
4. changing to backend B without recreating the pin;
5. HTTP/HTTPS delegation through the wrapper;
6. new-window and InPrivate desktop actions;
7. direct backend launch retaining its own identity;
8. removing the selected backend and receiving the Aero7 fallback dialog;
9. no-browser Programs Center routing;
10. clean Wayland logs without X11-only helpers.

Running-window identity must be reported separately from pinned-launcher
identity. Screenshots or task-model state are required before claiming either.
