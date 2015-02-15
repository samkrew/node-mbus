var mbus = require('bindings')('mbus');

setInterval(function() {
	var myMbus = new mbus('/dev/ttys003',2400);
},1000);
