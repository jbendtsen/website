"use strict";

var sb_toggled = false;

function toggle_project_sidebar() {
    sb_toggled = !sb_toggled;
    if (sb_toggled) {
        document.documentElement.style.setProperty("--sidebar-width", "30%");
        document.getElementById("proj-sb-shape-inner").classList.add("proj-sb-toggled");
        document.getElementById("proj-sb-shape-outer").classList.add("proj-sb-toggled");
    }
    else {
        document.documentElement.style.removeProperty("--sidebar-width");
        document.getElementById("proj-sb-shape-inner").classList.remove("proj-sb-toggled");
        document.getElementById("proj-sb-shape-outer").classList.remove("proj-sb-toggled");
    }
}