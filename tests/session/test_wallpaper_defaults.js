const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const root = path.resolve(__dirname, "../..");
const scripts = ["services/session/aero7-wallpaper-defaults.js",
    "theme/plasma/shells/io.gitgud.wackyideas.desktop/contents/main.js"];
const defaultImage = "file:///usr/share/wallpapers/Aero7ShellDefault/contents/images/1672x941.png";
function desktop(plugin, image) {
    return { wallpaperPlugin: plugin, currentConfigGroup: ["General"], writes: [], image,
        readConfig(key, fallback) { assert.equal(key, "Image"); return this.image || fallback; },
        writeConfig(key, value) { this.writes.push([key, value]); if (key === "Image") this.image = value; } };
}
for (const script of scripts) {
    const source = fs.readFileSync(path.join(root, script), "utf8");
    const existing = desktop("org.kde.image", "file:///home/test/Pictures/my-wallpaper.png");
    const second = desktop("org.kde.image", "file:///home/test/Pictures/other-monitor.jpg");
    const slideshow = desktop("org.kde.slideshow", "");
    const solid = desktop("org.kde.color", "");
    const fresh = desktop("", "");
    const emptyImage = desktop("org.kde.image", "");
    const screens = [existing, second, slideshow, solid, fresh, emptyImage];
    const context = { desktops: () => screens, loadTemplate: () => {} };
    vm.runInNewContext(source, context, { filename: script });
    for (const item of [existing, second, slideshow, solid]) assert.deepEqual(item.writes, [], script);
    for (const item of [fresh, emptyImage]) {
        assert.equal(item.image, defaultImage, script);
        assert.equal(item.wallpaperPlugin, "org.kde.image");
        assert.equal(item.writes.length, 2);
    }
    // Repeated login, hotplug reconciliation and appearance reload must leave
    // a subsequently chosen wallpaper untouched; no destructive global apply.
    fresh.image = "file:///home/test/Pictures/changed-after-first-login.png";
    for (const item of screens) item.writes.length = 0;
    for (let repeat = 0; repeat < 3; ++repeat) vm.runInNewContext(source, context);
    for (const item of screens) {
        assert.deepEqual(item.writes, [], script);
        assert.equal(JSON.stringify(item.currentConfigGroup), '["General"]');
    }
}
console.log("Wallpaper defaults preserve images, per-monitor choices and non-image plugins; fresh defaults are idempotent.");
