
// During a brand-new login currentActivity() is not guaranteed to be ready
// when the shell layout script runs.  Querying only that activity can return
// an empty list and leaves Plasma's stock wallpaper visible.  desktops()
// covers every containment that already exists during shell startup.
var desktopsArray = desktops();
for( var j = 0; j < desktopsArray.length; j++) {
    var desktop = desktopsArray[j];
    if (desktop.wallpaperPlugin && desktop.wallpaperPlugin !== "org.kde.image") {
        continue;
    }
    var previousGroup = desktop.currentConfigGroup;
    desktop.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
    if (!desktop.readConfig("Image", "")) {
        desktop.wallpaperPlugin = 'org.kde.image';
        desktop.writeConfig("Image", "file:///usr/share/wallpapers/Aero7ShellDefault/contents/images/1672x941.png");
        desktop.writeConfig("PreviewImage", "file:///usr/share/wallpapers/Aero7ShellDefault/contents/images/1672x941.png");
    }
    desktop.currentConfigGroup = previousGroup;
}

// Set the branded wallpaper before the panel template is loaded.  Creating
// the taskbar first lets Plasma render one frame with its stock wallpaper on a
// new profile, which is visible between the Welcome screen and the desktop.
loadTemplate("io.gitgud.wackyideas.taskbar")
