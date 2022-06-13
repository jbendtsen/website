"use strict";

function md_load_canvas_response(js, sources) {
	var test = js + "; init_canvas";
	console.log(test);
	var init_func = eval(test);
	for (var s of sources) {
		var elem = s[0];
		var params = s[1];
		init_func(elem, params);
	}
}

function md_load_canvases() {
	if (window.canvas_scripts_loaded)
		return;

	window.canvas_scripts_loaded = true;

	var list = document.getElementsByClassName("needs-canvas-js");
	var js_files = {};
	for (var e of list) {
		var name = e.dataset.js;
		if (name && name.length > 0) {
			var value = [e, e.dataset.params];
			if (name in js_files)
				js_files[name].push(value);
			else
				js_files[name] = [value];
		}
	}

	for (var name in js_files)
		fetch(name).then(res => { res.text().then(txt => { md_load_canvas_response(txt, js_files[name]); }); });
}

window.addEventListener("load", md_load_canvases);
