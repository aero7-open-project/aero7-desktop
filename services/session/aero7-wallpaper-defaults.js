// A default is not a policy: existing images and non-image wallpaper plugins
// belong to the user. Apply this independently to each unconfigured desktop.
(function () {
    var allDesktops = desktops();
    for (var i = 0; i < allDesktops.length; ++i) {
        var desktop = allDesktops[i];
        if (desktop.wallpaperPlugin && desktop.wallpaperPlugin !== "org.kde.image") {
            continue;
        }
        var previousGroup = desktop.currentConfigGroup;
        desktop.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
        if (!desktop.readConfig("Image", "")) {
            desktop.wallpaperPlugin = "org.kde.image";
            desktop.writeConfig("Image", "file:///usr/share/wallpapers/Aero7ShellDefault/contents/images/1672x941.png");
            desktop.writeConfig("PreviewImage", "file:///usr/share/wallpapers/Aero7ShellDefault/contents/images/1672x941.png");
        }
        desktop.currentConfigGroup = previousGroup;
    }
})();
