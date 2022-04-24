"use strict";

async function get_markdown_render(md_text) {
    var res = await fetch("/markdown", {
        method: "POST",
        headers: {
            "Content-Type": "text/plain",
            "Accept": "text/plain"
        },
        body: md_text
    });
    return res.text();
}

function set_markdown_preview(html) {
    document.getElementById("mdedit-preview").innerHTML = html;
}

function mdedit_listener(text) {
    //var contents = document.getElementById("mdedit-editor").innerHTML;
    /*
    text = text.replaceAll("<div>", "")
    	.replaceAll("</div>", "\n")
    	.replaceAll("<br>", "\n")
    	.replaceAll("&nbsp;", " ")
    	.replaceAll("&amp;", "&")
    	.replaceAll("&lt;", "<")
    	.replaceAll("&gt;", ">");
    */
    get_markdown_render(text).then(set_markdown_preview);
}

function setup_mdedit_editor() {
	/*
	var editor = document.getElementById('mdedit-editor').code_editor;
	editor.size_ref_elem = document.getElementById('mdedit-editor-container');
	editor.refresh();
	*/
}

var dragging = false;

function start_dragging() {
	document.body.style.userSelect = "none";
	dragging = true;
}

function stop_dragging() {
	dragging = false;
	document.body.style.userSelect = "auto";
}

function global_mouse_handler(e) {
	if ((e.buttons & 1) == 0)
		stop_dragging();
	if (!dragging)
		return;

	e = e || window.event;
	e.preventDefault();
	e.stopPropagation();
	e.cancelBubble = true;

	document.getElementById("mdedit-left").style.width = "" + (e.clientX - 13) + "px";
}
