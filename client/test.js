var thing;

function print_thing(e) {
	console.log(thing);
}

document.addEventListener("keydown", print_thing);

thing = {
	"t": 3,
	"wild": "thing"
}
