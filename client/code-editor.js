"use strict";

const CE_FLAG_PREVENT_DEFAULT = 1;
const CE_FLAG_REFRESH = 2;
const CE_FLAG_LISTENER = 4;

class CodeEditor {
	constructor(canv) {
		this.text = "";
		this.cur1 = 0;
		this.cur2 = 0;
		this.cursor_col = 0;
		this.cursor_row = 0;
		this.target_vis_col = -1;
		this.view_x = 0;
		this.view_y = 0;
		this.view_w = 0;
		this.view_h = 0;
		this.prev_action = "";
		this.blink_timer = null;
		this.blink_state = true;

		this.canv_elem = canv;
		this.plane_elem = canv.parentElement;
		this.outer_elem = this.plane_elem.parentElement;

		var outer_style = getComputedStyle(this.outer_elem);

		this.tab_w = 4;
		this.fore = outer_style.getPropertyValue("color");
		this.back = outer_style.getPropertyValue("background-color");
		this.hl   = "#48f";

		this.canv_pad_x = 400;
		this.canv_pad_y = 400;

		this.canvas = canv.getContext("2d");

		var px_height = 12;
		this.font_string = "" + px_height + "px monospace";
		this.char_base = px_height / 5;

		this.canvas.fillStyle = this.fore;
		this.canvas.font = this.font_string;
		this.canvas.imageSmoothingEnabled = false

		this.resize_canvas();

		var metrics = this.canvas.measureText("W");
		this.char_w = metrics.width;
		this.char_h = px_height + 2 * this.char_base;
	}

	resize_canvas() {
		var styles = getComputedStyle(this.outer_elem);
		var w = Math.floor(parseFloat(styles.getPropertyValue("width"))  + this.canv_pad_x);
		var h = Math.floor(parseFloat(styles.getPropertyValue("height")) + this.canv_pad_y);

		if (w == this.view_w && h == this.view_h)
			return;

		this.view_w = w;
		this.view_h = h;

		var scale = window.devicePixelRatio || 1;
		this.canv_elem.width  = Math.floor(this.view_w * scale);
		this.canv_elem.height = Math.floor(this.view_h * scale);
		this.canvas.scale(scale, scale);
		this.canv_elem.style.width  = "" + this.view_w + "px";
		this.canv_elem.style.height = "" + this.view_h + "px";
		this.canvas.font = this.font_string;
	}

	blink() {
		if (this.blink_timer)
			clearTimeout(this.blink_timer);
		if (this.cur1 != this.cur2)
			return;

		this.blink_timer = setTimeout(function(editor){editor.blink();}, 1000, this);

		var x = Math.floor(this.cursor_col * this.char_w - this.view_x + 0.5);
		var y = Math.floor(this.cursor_row * this.char_h - this.view_y + 0.5);
		var w = 1;
		var h = Math.floor(this.char_h + 0.5);

		this.canvas.fillStyle = this.blink_state ? this.fore : this.back;
		this.canvas.fillRect(x, y, w, h);
		this.blink_state = !this.blink_state;
	}

	find_target_column(start, end) {
		var vis_cols = 0;
		for (var i = start; i < end && i < this.text.length; i++) {
			var code = this.text.charCodeAt(i);
			if (code == 10)
				break;

			if (code == 9) {
				vis_cols += this.tab_w - (vis_cols % this.tab_w);
			}
			else {
				vis_cols++;
			}
		}

		this.target_vis_cols = vis_cols;
	}

	get_delim_kind(c) {
		if (c == 0x20 || c == 9)
			return 0;
		else if ((c >= 0x30 && c <= 0x39) || (c >= 0x41 && c <= 0x5a) || c == 0x5f || (c >= 0x61 && c <= 0x7a))
			return 1;
		return 2;
	}

	move_prev_word(selecting) {
		var idx = this.cur2;

		for (var i = 0; i < 2; i++) {
			if (idx <= 1) {
				this.cur2 = 0;
				if (!selecting)
					this.cur1 = this.cur2;
				return;
			}

			idx--;
			var start_kind = this.get_delim_kind(this.text.charCodeAt(idx));
			while (idx > 0) {
				idx--;
				var kind = this.get_delim_kind(this.text.charCodeAt(idx));
				if (kind != start_kind) {
					idx++;
					break;
				}
			}
		}

		this.cur2 = idx;
		if (!selecting)
			this.cur1 = this.cur2;
	}

	move_next_word(selecting) {
		var idx = this.cur2;
		var len = this.text.length;

		for (var i = 0; i < 2; i++) {
			if (idx >= len-1) {
				this.cur2 = len;
				if (!selecting)
					this.cur1 = this.cur2;
				return;
			}

			var start_kind = this.get_delim_kind(this.text.charCodeAt(idx));
			while (idx < len) {
				idx++;
				var kind = this.get_delim_kind(this.text.charCodeAt(idx));
				if (kind != start_kind) {
					break;
				}
			}
		}

		this.cur2 = idx;
		if (!selecting)
			this.cur1 = this.cur2;
	}

	move_distance(distance, selecting) {
		var idx = this.cur2 + distance;
		if (idx < 0 || idx > this.text.length)
			return;

		this.cur2 = idx;
		if (!selecting)
			this.cur1 = this.cur2;
	}

	move_up(selecting) {
		if (this.prev_action != "arrowup" && this.prev_action != "arrowdown")
			this.target_vis_cols = -1;

		var idx = this.cur2;
		if (idx <= 0) {
			this.target_vis_cols = -1;
			this.cur2 = 0;
			if (!selecting)
				this.cur1 = this.cur2;
			return;
		}

		while (idx > 0) {
			idx--;
			if (this.text.charCodeAt(idx) == 10) {
				idx++;
				break;
			}
		}

		if (idx <= 0) {
			this.cur2 = 0;
			if (!selecting)
				this.cur1 = this.cur2;
			return;
		}

		var vis_cols = this.target_vis_cols;
		if (vis_cols < 0) {
			this.find_target_column(idx, this.cur2);
			vis_cols = this.target_vis_cols;
		}

		idx--;
		while (idx > 0) {
			idx--;
			if (this.text.charCodeAt(idx) == 10) {
				idx++;
				break;
			}
		}

		var vis = 0;
		while (vis < vis_cols) {
			var code = this.text.charCodeAt(idx);
			if (code == 10) {
				break;
			}
			if (code == 9) {
				vis += this.tab_w - (vis % this.tab_w);
			}
			else {
				vis++;
			}
			idx++;
		}

		this.cur2 = idx;
		if (!selecting)
			this.cur1 = this.cur2;
		return;
	}

	move_down(selecting) {
		if (this.prev_action != "arrowup" && this.prev_action != "arrowdown")
			this.target_vis_cols = -1;

		var len = this.text.length;
		var idx = this.cur2;
		if (idx >= len) {
			this.target_vis_cols = -1;
			this.cur2 = len;
			if (!selecting)
				this.cur1 = this.cur2;
			return;
		}

		var end = idx;
		while (end < len && this.text.charCodeAt(end) != 10) {
			end++;
		}

		if (end >= len) {
			this.cur2 = len;
			if (!selecting)
				this.cur1 = this.cur2;
			return;
		}

		while (idx > 0) {
			idx--;
			if (this.text.charCodeAt(idx) == 10) {
				idx++;
				break;
			}
		}

		var vis_cols = this.target_vis_cols;
		if (vis_cols < 0) {
			this.find_target_column(idx, this.cur2);
			vis_cols = this.target_vis_cols;
		}

		idx = end + 1;
		var vis = 0;
		while (idx < len && vis < vis_cols) {
			var code = this.text.charCodeAt(idx);
			if (code == 10) {
				break;
			}
			if (code == 9) {
				vis += this.tab_w - (vis % this.tab_w);
			}
			else {
				vis++;
			}
			idx++;
		}

		this.cur2 = idx;
		if (!selecting)
			this.cur1 = this.cur2;
		return;
	}

	move_home(selecting) {
		var len = this.text.length;
		var idx = this.cur2;

		var was_home = true;
		while (idx > 0) {
			idx--;
			if (this.text.charCodeAt(idx) == 10) {
				idx++;
				break;
			}
			was_home = false;
		}

		if (was_home) {
			while (idx < len && this.text.charCodeAt(idx) == 9) {
				idx++;
			}
		}

		this.cur2 = idx;
		if (!selecting)
			this.cur1 = this.cur2;
		return;
	}

	move_end(selecting) {
		var len = this.text.length;
		var idx = this.cur2;

		while (idx < len && this.text.charCodeAt(idx) != 10) {
			idx++;
		}

		this.cur2 = idx;
		if (!selecting)
			this.cur1 = this.cur2;
		return;
	}

	get_selected(should_delete) {
		if (this.cur1 == this.cur2)
			return "";

		var start = this.cur1 < this.cur2  ? this.cur1 : this.cur2;
		var end   = this.cur1 >= this.cur2 ? this.cur1 : this.cur2;

		var selected = this.text.substring(start, end);

		if (should_delete)
			this.text = this.text.substring(0, start) + this.text.substring(end);

		return selected;
	}

	clear_selection() {
		if (this.cur1 == this.cur2)
			return false;

		var start = this.cur1 < this.cur2  ? this.cur1 : this.cur2;
		var end   = this.cur1 >= this.cur2 ? this.cur1 : this.cur2;

		var len = this.text.length;
		this.text = this.text.substring(0, start) + this.text.substring(end, len);
		this.cur1 = start;
		this.cur2 = this.cur1;

		return true;
	}

	insert(str) {
		this.clear_selection();

		var len = this.text.length;
		this.text = this.text.substring(0, this.cur1) + str + this.text.substring(this.cur1, len);
		this.cur1 += str.length;
		this.cur2 = this.cur1;
	}

	delete(distance) {
		if (this.clear_selection())
			return;

		var idx = this.cur1 + distance;
		var len = this.text.length;
		if (idx < 0 || idx >= len)
			return;

		this.text = this.text.substring(0, idx) + this.text.substring(idx + 1, len);
		this.cur1 = idx;
		this.cur2 = this.cur1;
	}

	indent(forwards) {
		if (this.cur1 == this.cur2) {
			this.insert("\t");
			return;
		}

		var start = this.cur1 < this.cur2  ? this.cur1 : this.cur2;
		var end   = this.cur1 >= this.cur2 ? this.cur1 : this.cur2;

		var lines = 0;
		for (var i = start; i < end + lines && i < this.text.length; i++) {
			var c = this.text.charCodeAt(i);
			if (c == 10) {
				lines++;

				var next = 0;
				if (i < this.text.length-1)
					next = this.text.charCodeAt(i+1);

				if (next != 10) {
					if (forwards) {
						this.text = this.text.substring(0, i+1) + "\t" + this.text.substring(i+1);
					}
					else if (next == 9) {
						this.text = this.text.substring(0, i+1) + this.text.substring(i+2);
					}
				}
			}
		}

		if (lines == 0) {
			this.insert("\t");
			return;
		}

		while (start > 0 && this.text.charCodeAt(start) != 10) {
			start--;
		}

		if (forwards) {
			this.text = this.text.substring(0, start) + "\t" + this.text.substring(start);
		}
		else if (this.text.charCodeAt(start) == 9) {
			this.text = this.text.substring(0, start) + this.text.substring(start+1);
		}

		var start_diff = forwards ? 1 : -1;
		var end_diff = lines + 1;
		end_diff *= start_diff;

		if (this.cur1 < this.cur2) {
			this.cur1 += start_diff;
			this.cur2 += end_diff;
		}
		else {
			this.cur1 += end_diff;
			this.cur2 += start_diff;
		}
	}

	handle_keypress(action, ctrl, shift) {
		var res = CE_FLAG_PREVENT_DEFAULT | CE_FLAG_REFRESH;

		if (action.length == 1) {
			if (ctrl) {
				if (action == "r") {
					res &= ~CE_FLAG_PREVENT_DEFAULT;
				}
				else if (action == "a") {
					this.cur1 = 0;
					this.cur2 = this.text.length;
				}
				else if (action == "c" || action == "x" || action == "v") {
					res &= ~CE_FLAG_PREVENT_DEFAULT;
				}
			}
			else {
				this.insert(action);
				res |= CE_FLAG_LISTENER;
			}
		}
		else {
			action = action.toLowerCase();
			console.log(action);
			if (action == "control" || action == "shift" || action == "capslock") {
				action = null;
				res &= ~CE_FLAG_REFRESH;
			}
			else if (action == "enter") {
				this.insert("\n");
				res |= CE_FLAG_LISTENER;
			}
			else if (action == "tab") {
				this.indent(!shift);
				res |= CE_FLAG_LISTENER;
			}
			else if (action == "backspace") {
				this.delete(-1);
				res |= CE_FLAG_LISTENER;
			}
			else if (action == "delete") {
				this.delete(0);
				res |= CE_FLAG_LISTENER;
			}
			else if (action == "home") {
				this.move_home(shift);
			}
			else if (action == "end") {
				this.move_end(shift);
			}
			else if (action == "arrowleft") {
				if (ctrl) {
					this.move_prev_word(shift);
				}
				else {
					this.move_distance(-1, shift);
				}
			}
			else if (action == "arrowright") {
				if (ctrl) {
					this.move_next_word(shift);
				}
				else {
					this.move_distance(1, shift);
				}
			}
			else if (action == "arrowup") {
				this.move_up(shift);
			}
			else if (action == "arrowdown") {
				this.move_down(shift);
			}
		}
		if (action)
			this.prev_action = action;

		return res;
	}

	maybe_scroll(pos_x, pos_y) {
		var x = Math.floor(pos_x - (pos_x % this.canv_pad_x));
		var y = Math.floor(pos_y - (pos_y % this.canv_pad_y));

		if (x == this.view_x && y == this.view_y)
			return;

		this.view_x = x;
		this.view_y = y;

		requestAnimationFrame(() => {this.refresh();});
	}

	refresh() {
		this.resize_canvas();

		this.canvas.fillStyle = this.back;
		this.canvas.fillRect(0, 0, this.canv_elem.width, this.canv_elem.height);
		this.canvas.fillStyle = this.fore;

		var start_sel = this.cur1 <  this.cur2 ? this.cur1 : this.cur2;
		var end_sel   = this.cur1 >= this.cur2 ? this.cur1 : this.cur2;

		var total_cols = 0;
		var total_rows = 0;

		var col = 0;
		var row = 0;
		var len = this.text.length;

		for (var i = 0; i < len; i++) {
			var code = 0;
			var test_col = col;
			var x = col * this.char_w - this.view_x;
			var y = row * this.char_h - this.view_y;

			for (var j = i; j < len; j++) {
				if (j == this.cur1) {
					this.cursor_col = test_col;
					this.cursor_row = row;
				}

				code = this.text.charCodeAt(j);
				if (code < 0x20)
					break;

				if (j >= start_sel && j < end_sel &&
					x >= -this.char_w && x <= this.view_w &&
					y >= -this.char_h && y <= this.view_h
				) {
					this.canvas.fillStyle = this.hl;
					this.canvas.fillRect(x, y, this.char_w + 1, this.char_h + 1);
				}

				x += this.char_w;
				test_col++;
			}

			x = col * this.char_w - this.view_x;
			if (j > i && x <= this.view_w && y >= -this.char_h && y <= this.view_h) {
				this.canvas.fillStyle = this.fore;
				this.canvas.fillText(this.text.substring(i, j), x + 1, y + this.char_h - this.char_base);
			}

			col = test_col;

			if (code == 9) {
				col += this.tab_w - (col % this.tab_w);
			}
			else if (code == 10) {
				if (col > total_cols)
					total_cols = col + 1;
				col = 0;
				row++;
			}
			else if (j < len-1) {
				col++;
			}

			i = j;
		}

		total_rows = row + 1;
		if (col > total_cols)
			total_cols = col + 1;

		if (this.cur1 == this.text.length) {
			this.cursor_col = col;
			this.cursor_row = row;
		}

		this.canv_elem.style.left = "" + this.view_x + "px";
		this.canv_elem.style.top  = "" + this.view_y + "px";

		this.plane_elem.style.width  = "" + (total_cols * this.char_w) + "px";
		this.plane_elem.style.height = "" + (total_rows * this.char_h) + "px";

		this.blink_state = true;
		this.blink();
	}
}

function code_editor_keydown_handler(event) {
	var editor = event.target.code_editor;
	var res = editor.handle_keypress(event.key, event.ctrlKey, event.shiftKey);
	if (res & 1)
		event.preventDefault();
	if (res & 2)
		editor.refresh();
	if ((res & 4) && editor.listener)
		editor.listener(editor.text);
}

function code_editor_keyup_handler(event) {
	//alert("key up");
}

function code_editor_mousedown_handler(event) {
	//alert("mouse down");
	event.target.tabIndex = 1;
}

function code_editor_mouseup_handler(event) {
	//alert("mouse up");
}

function code_editor_cut_copy_handler(event, elem, is_cut) {
	event.preventDefault();

	var selected = elem.code_editor.get_selected(is_cut);
	if (selected.length > 0)
		event.clipboardData.setData("text/plain", selected);

	elem.code_editor.refresh();

	if (elem.code_editor.listener)
		elem.code_editor.listener(elem.code_editor.text);
}

function code_editor_paste_handler(event, elem) {
	event.preventDefault();
	var text = (event.clipboardData || window.clipboardData).getData("text");
	if (text)
		elem.code_editor.insert(text);

	elem.code_editor.refresh();

	if (elem.code_editor.listener)
		elem.code_editor.listener(elem.code_editor.text);
}

function code_editor_scroll_handler(event) {
	var x = event.target.scrollLeft;
	var y = event.target.scrollTop;
	event.target.firstChild.firstChild.code_editor.maybe_scroll(x, y);
}

function init_code_editor(canv_elem, input_listener) {
	var editor = new CodeEditor(canv_elem);
	editor.listener = input_listener;
	canv_elem.code_editor = editor;

	canv_elem.addEventListener("keydown", code_editor_keydown_handler);
	canv_elem.addEventListener("keyup", code_editor_keyup_handler);
	canv_elem.addEventListener("mousedown", code_editor_mousedown_handler);
	canv_elem.addEventListener("mouseup", code_editor_mouseup_handler);

	canv_elem.parentElement.parentElement.addEventListener("scroll", code_editor_scroll_handler);

	canv_elem.addEventListener("blur", (event) => { console.log("BLUR!"); });
}

function load_code_editors() {
	var editor_elems = document.getElementsByClassName("code-editor");
	if (editor_elems.length == 0)
		return;

	for (var e of editor_elems) {
		e.style.overflow = "auto";

		var plane_elem = document.createElement("div");
		plane_elem.style.position = "relative";
		plane_elem.style.overflow = "hidden";
		plane_elem.style.minWidth = "100%";
		plane_elem.style.minHeight = "100%";

		var canv_elem = document.createElement("canvas");
		canv_elem.style.position = "absolute";
		canv_elem.style.top = "0";
		canv_elem.style.left = "0";
		canv_elem.style.outline = "0";

		plane_elem.appendChild(canv_elem);
		e.appendChild(plane_elem);

		init_code_editor(canv_elem, window[e.dataset.listener]);
		canv_elem.code_editor.refresh();
	}

	document.addEventListener("copy", (event) => {
		if (document.activeElement.code_editor)
			code_editor_cut_copy_handler(event, document.activeElement, false);
	});
	document.addEventListener("cut", (event) => {
		if (document.activeElement.code_editor)
			code_editor_cut_copy_handler(event, document.activeElement, true);
	});
	document.addEventListener("paste", (event) => {
		if (document.activeElement.code_editor)
			code_editor_paste_handler(event, document.activeElement);
	});
}
