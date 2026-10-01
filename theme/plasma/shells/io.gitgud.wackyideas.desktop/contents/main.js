
// During a brand-new login currentActivity() is not guaranteed to be ready
// when the shell layout script runs.  Querying only that activity can return
// an empty list and leaves Plasma's stock wallpaper visible.  desktops()
// covers every containment that already exists during shell startup.
var desktopsArray = desktops();
for( var j = 0; j < desktopsArray.length; j++) {
    desktopsArray[j].wallpaperPlugin = 'org.kde.image';
    desktopsArray[j].currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];
    desktopsArray[j].writeConfig("Image", "file:///usr/share/wallpapers/Aero7ShellDefault/contents/images/1672x941.png");
    desktopsArray[j].writeConfig("PreviewImage", "file:///usr/share/wallpapers/Aero7ShellDefault/contents/images/1672x941.png");
    //var clock = desktopsArray[j].addWidget("org.kde.plasma.analogclock");
}

// Set the branded wallpaper before the panel template is loaded.  Creating
// the taskbar first lets Plasma render one frame with its stock wallpaper on a
// new profile, which is visible between the Welcome screen and the desktop.
loadTemplate("io.gitgud.wackyideas.taskbar")
