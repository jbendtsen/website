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

function mdedit_listener() {
    var contents = document.getElementById("mdedit-editor").innerHTML;
    contents = contents.replaceAll("<div>", "");
    contents = contents.replaceAll("</div>", "\n");
    contents = contents.replaceAll("<br>", "\n");
    contents = contents.replaceAll("&nbsp;", " ");
    contents = contents.replaceAll("&amp;", "&");
    contents = contents.replaceAll("&lt;", "<");
    contents = contents.replaceAll("&gt;", ">");
    get_markdown_render(contents).then(set_markdown_preview);
}