/* One scene per power-on. No animation or repeated display refresh. */
var ferryDone=false;
require('/night-ferry.js')(require('canvas').display.canvas,function(){ferryDone=true;});
