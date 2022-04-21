"use strict";

// I'm sure there's a cleaner way to do this but don't care lmao
function code_editor_blink(editor) {
	editor.blink();
}

class CodeEditor {
	constructor(element) {
		this.text = "";
		this.cur1 = 0;
		this.cur2 = 0;
		this.cursor_col = 0;
		this.cursor_row = 0;
		this.blink_timer = null;
		this.blink_state = true;

		this.tab_w = 4;
		this.fore = "#111";
		this.back = "#eee";

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

		this.blink_timer = setTimeout(code_editor_blink, 1000, this);

		this.canvas.fillStyle = this.blink_state ? this.fore : this.back;
		this.canvas.fillRect(this.cursor_col * this.char_w, (20 + this.char_base - this.char_h) + this.cursor_row * this.char_h, 2, this.char_h);

		this.blink_state = !this.blink_state;
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
		var idx = this.cur2;
		if (idx <= 0) {
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
	}

	move_down(selecting) {
		
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

	refresh() {
		this.canvas.fillStyle = this.back;
		this.canvas.fillRect(0, 0, this.elem.width, this.elem.height);
		this.canvas.fillStyle = this.fore;

		var col = 0;
		var row = 0;
		var len = this.text.length;

		for (var i = 0; i < len; i++) {
			var code = 0;
			var test_col = col;

			for (var j = i; j < len; j++) {
				code = this.text.charCodeAt(j);
				if (j == this.cur1) {
					this.cursor_col = test_col;
					this.cursor_row = row;
				}
				if (code < 0x20)
					break;

				test_col++;
			}

			if (j > i)
				this.canvas.fillText(this.text.substring(i, j), 1 + col * this.char_w, 20 + row * this.char_h);

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

	if (event.ctrlKey) {
		
	}
	else {
		if (event.key.length == 1) {
			editor.insert(event.key);
		}
		else {
			var code = event.key.toLowerCase();
			console.log(code);
			if (code == "enter") {
				editor.insert("\n");
			}
			else if (code == "tab") {
				editor.insert("\t");
				event.preventDefault();
			}
			else if (code == "backspace") {
				editor.delete(-1);
			}
			else if (code == "delete") {
				editor.delete(0);
			}
			else if (code == "arrowleft") {
				editor.move_distance(-1, event.shiftKey);
			}
			else if (code == "arrowright") {
				editor.move_distance(1, event.shiftKey);
			}
			else if (code == "arrowup") {
				editor.move_up(event.shiftKey);
			}
			else if (code == "arrowdown") {
				editor.move_down(event.shiftKey);
			}
		}
	}

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
