"use strict";

class CodeEditor {
	constructor(element) {
		this.text = "";
		this.cur1 = 0;
		this.cur2 = 0;
		this.cursor_col = 0;
		this.cursor_row = 0;
		this.target_vis_col = -1;
		this.prev_action = "";
		this.blink_timer = null;
		this.blink_state = true;

		this.tab_w = 4;
		this.fore = "#111";
		this.back = "#eee";
		this.hl   = "#48f";

		this.elem = element;
		this.canvas = element.getContext("2d");

		var px_height = 12;
		this.char_base = px_height / 5;

		this.canvas.fillStyle = this.fore;
		this.canvas.font = "" + px_height + "px monospace";

		var metrics = this.canvas.measureText("W");
		this.char_w = metrics.width;
		this.char_h = px_height + 2 * this.char_base;
	}

	blink() {
		if (this.blink_timer)
			clearTimeout(this.blink_timer);
		if (this.cur1 != this.cur2)
			return;

		this.blink_timer = setTimeout(function(editor){editor.blink();}, 1000, this);

		this.canvas.fillStyle = this.blink_state ? this.fore : this.back;
		this.canvas.fillRect(this.cursor_col * this.char_w, this.cursor_row * this.char_h, 2, this.char_h);

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
		var block_default = true;

		if (action.length == 1) {
			if (ctrl) {
				if (action == "r") {
					block_default = false;
				}
			}
			else {
				this.insert(action);
			}
		}
		else {
			action = action.toLowerCase();
			console.log(action);
			if (action == "control" || action == "shift" || action == "capslock") {
				action = null;
			}
			else if (action == "enter") {
				this.insert("\n");
			}
			else if (action == "tab") {
				this.indent(!shift);
			}
			else if (action == "backspace") {
				this.delete(-1);
			}
			else if (action == "delete") {
				this.delete(0);
			}
			else if (action == "home") {
				this.move_home(shift);
			}
			else if (action == "end") {
				this.move_end(shift);
			}
			else if (action == "arrowleft") {
				if (ctrl) {
					
				}
				else {
					this.move_distance(-1, shift);
				}
			}
			else if (action == "arrowright") {
				if (ctrl) {
					
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

		return block_default;
	}

	refresh() {
		this.canvas.fillStyle = this.back;
		this.canvas.fillRect(0, 0, this.elem.width, this.elem.height);
		this.canvas.fillStyle = this.fore;

		var start_sel = this.cur1 <  this.cur2 ? this.cur1 : this.cur2;
		var end_sel   = this.cur1 >= this.cur2 ? this.cur1 : this.cur2;

		var col = 0;
		var row = 0;
		var len = this.text.length;

		for (var i = 0; i < len; i++) {
			var code = 0;
			var test_col = col;

			for (var j = i; j < len; j++) {
				if (j == this.cur1) {
					this.cursor_col = test_col;
					this.cursor_row = row;
				}

				code = this.text.charCodeAt(j);
				if (code < 0x20)
					break;

				if (j >= start_sel && j < end_sel) {
					this.canvas.fillStyle = this.hl;
					this.canvas.fillRect(test_col * this.char_w, row * this.char_h, this.char_w + 1, this.char_h + 1);
				}

				test_col++;
			}

			if (j > i) {
				this.canvas.fillStyle = this.fore;
				this.canvas.fillText(this.text.substring(i, j), 1 + col * this.char_w, (row+1) * this.char_h - this.char_base);
			}

			col = test_col;

			if (code == 9) {
				col += this.tab_w - (col % this.tab_w);
			}
			else if (code == 10) {
				col = 0;
				row++;
			}
			else if (j < len-1) {
				col++;
			}

			i = j;
		}

		if (this.cur1 == this.text.length) {
			this.cursor_col = col;
			this.cursor_row = row;
		}

		this.blink_state = true;
		this.blink();
	}
}

function code_editor_keydown_handler(event) {
	var editor = event.target.code_editor;
	if (editor.handle_keypress(event.key, event.ctrlKey, event.shiftKey))
		event.preventDefault();

	editor.refresh();
}

function code_editor_keyup_handler(event) {
	//alert("key up");
}

function code_editor_mousedown_handler(event) {
	//alert("mouse down");
	event.target.tabIndex = 0;
}

function code_editor_mouseup_handler(event) {
	//alert("mouse up");
}

function init_code_editor(elem, input_listener) {
	var editor = new CodeEditor(elem);
	editor.listener = input_listener;
	editor.blink();

	elem.code_editor = editor;
	elem.addEventListener("keydown", code_editor_keydown_handler);
	elem.addEventListener("keyup", code_editor_keyup_handler);
	elem.addEventListener("mousedown", code_editor_mousedown_handler);
	elem.addEventListener("mouseup", code_editor_mouseup_handler);
}

function load_code_editors() {
	var editor_elems = document.getElementsByClassName("code-editor");
	for (var e of editor_elems) {
		init_code_editor(e, e.dataset.listener);
	}
}
