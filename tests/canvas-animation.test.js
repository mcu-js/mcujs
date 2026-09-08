const test = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const path = require('node:path');
test('animation is bounded, moves within the canvas and stops once', () => {
  let now=0, next, complete=0, rects=[], lines=0;
  const context={fillRect(x,y,w,h){if(w===16)rects.push(x);},beginPath(){},moveTo(){},lineTo(){},stroke(){lines++;}};
  const module={exports:{}};
  vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../examples/waveshare_rp2040_pizero/canvas-animation.js'),'utf8'), {
    module, Date:{now:()=>now},setTimeout(fn){next=fn;return 1;},clearTimeout(){next=null;}
  });
  const controller=module.exports({width:160,height:120,getContext(type){assert.equal(type,'2d');return context;}},()=>complete++);
  while(controller.running){now+=50;const fn=next;next=null;assert.ok(fn);fn();assert.ok(now<=60000);}
  assert.equal(complete,1);assert.equal(lines,controller.frames);assert.ok(lines>100);
  assert.equal(Math.min(...rects),12);assert.equal(Math.max(...rects),128);
  assert.equal(next,null);controller.stop();assert.equal(complete,1);
});
