/* Slow, irregular blinks. The display adapter owns refresh and power policy. */
module.exports = function (canvas) {
  var scene = require('/wandering-library.js');
  var ctx = canvas.getContext('2d');
  var timer = null, running = true, blinks = 0, cleanings = 0;
  var state = 'awake', nextDelay = 0;
  if (canvas.width !== 200 || canvas.height !== 200) throw Error('This scene expects 200x200');
  function oval(x,y,rx,ry,color) {
    ctx.fillStyle=color;ctx.beginPath();
    for(var i=0;i<32;i++) {
      var a=i*Math.PI/16,px=x+Math.cos(a)*rx,py=y+Math.sin(a)*ry;
      if(i===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);
    }
    ctx.closePath();ctx.fill();
  }
  function eyes(open) {
    oval(37,104,4,4,'black');oval(55,113,3.5,3.5,'black');
    if(open) {
      oval(37,103,2,2,'white');oval(36.5,103,1,1,'black');
      oval(55,112,1.6,1.6,'white');
    } else {
      ctx.fillStyle='white';ctx.fillRect(35,103,4,1);ctx.fillRect(53.5,112,3,1);
    }
  }
  function later(fn,ms) {
    nextDelay=ms;
    timer=setTimeout(function(){timer=null;if(running)fn();},ms);
  }
  function rest() {
    state='awake';
    var delay=12000+Math.floor(Math.random()*16001);
    if(Math.random()<0.2)delay+=10000+Math.floor(Math.random()*15001);
    later(closeEyes,delay);
  }
  function clean() {
    state='cleaning';scene(canvas);cleanings++;rest();
  }
  function openEyes() {
    eyes(true);blinks++;state='awake';
    /* Four partial presentations (two blinks), then one full cleaning task. */
    if(blinks%2===0)later(clean,4000);else rest();
  }
  function closeEyes() {
    state='blinking';eyes(false);later(openEyes,1600);
  }
  scene(canvas);later(closeEyes,4000+Math.floor(Math.random()*4001));
  return {
    stop:function(){running=false;if(timer!==null)clearTimeout(timer);timer=null;state='stopped';},
    status:function(){return {running:running,state:state,blinks:blinks,cleanings:cleanings,nextDelayMs:nextDelay};}
  };
};
