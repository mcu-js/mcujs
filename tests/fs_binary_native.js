/* Executed by the real JerryScript native binding test, not Node's fs. */
(function () {
  var fs = require('fs');
  function eq(actual, expected, label) {
    if (actual !== expected) throw Error(label + ': ' + actual + ' !== ' + expected);
  }
  function denied(fn, code) { var caught=false;try { fn(); } catch(e) {caught=true;eq(e.code,code,'error code');} if(!caught)throw Error('missing '+code); }
  var source = new Uint8Array([99, 0, 128, 255, 65, 99]);
  var fd = fs.openSync('/app/binary.bin', 'w');
  eq(fs.writeSync(fd, source.subarray(1, 5), 0, 4), 4, 'binary write count');
  fs.closeSync(fd);
  fd = fs.openSync('/app/binary.bin', 'r');
  var target = new Uint8Array([77, 77, 77, 77, 77, 77, 77]);
  eq(fs.readSync(fd, target.subarray(1, 6), 1, 4), 4, 'binary read count');
  eq(Array.prototype.join.call(target), '77,77,0,128,255,65,77', 'subarray boundaries and all byte values');
  eq(fs.readSync(fd, target, 0, 1), 0, 'EOF');
  eq(fs.readSync(fd,target,0,1,1),1,'positioned read');eq(target[0],128,'positioned byte');
  eq(fs.readSync(fd,target,0,1),0,'positioned read preserves EOF cursor');
  eq(fs.readSync(fd,target,0,1,999),0,'beyond EOF');
  eq(fs.readSync(fd,new Uint8Array(0),0,0),0,'empty buffer');
  [-1,1.2,NaN,Infinity,'1'].forEach(function(v){denied(function(){fs.readSync(fd,target,v,1)},'ERR_OUT_OF_RANGE');});
  denied(function(){fs.readSync(fd,target,0,8)},'ERR_OUT_OF_RANGE');
  denied(function(){fs.readSync(fd,new Uint8Array(4097),0,4097)},'ERR_OUT_OF_RANGE');
  denied(function(){fs.readSync(fd,target,0,1,2147483647)},'ERR_OUT_OF_RANGE');
  denied(function(){fs.readSync(fd,new ArrayBuffer(4),0,1)},'ERR_INVALID_ARG_TYPE');
  denied(function(){fs.writeSync(fd,target,0,1)},'EBADF');
  fs.closeSync(fd);
  denied(function(){fs.closeSync(fd)},'EBADF');
  var stale=fd, handles=[];
  for(var i=0;i<4;i++) handles.push(fs.openSync('/app/binary.bin','r'));
  denied(function(){fs.readSync(stale,target,0,1)},'EBADF');
  denied(function(){fs.openSync('/app/binary.bin','r')},'EMFILE');
  handles.forEach(function(h){fs.closeSync(h);});
  denied(function(){fs.openSync('/app/binary.bin','a')},'ERR_INVALID_ARG_VALUE');
})();
