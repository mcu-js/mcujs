/* The Moon Mender. A 240x280 portrait, drawn with the portable Canvas subset. */
module.exports=function(canvas,complete){
  var c=canvas.getContext('2d'),s=Math.min(canvas.width/240,canvas.height/280);
  var ox=(canvas.width-240*s)/2,oy=(canvas.height-280*s)/2;
  function box(x,y,w,h,col){c.fillStyle=col;c.fillRect(ox+x*s,oy+y*s,w*s,h*s);}
  function path(p,col,fill,lw){c.beginPath();c.moveTo(ox+p[0][0]*s,oy+p[0][1]*s);for(var i=1;i<p.length;i++)c.lineTo(ox+p[i][0]*s,oy+p[i][1]*s);if(fill){c.closePath();c.fillStyle=col;c.fill();}else{c.strokeStyle=col;c.lineWidth=(lw||1)*s;c.stroke();}}
  function oval(x,y,rx,ry,col){var p=[];for(var i=0;i<32;i++){var a=i*Math.PI/16;p.push([x+Math.cos(a)*rx,y+Math.sin(a)*ry]);}path(p,col,true);}
  function line(p,col,w){path(p,col,false,w);}
  function star(x,y,r,col){path([[x,y-r],[x+r*.3,y-r*.25],[x+r,y],[x+r*.25,y+r*.3],[x,y+r],[x-r*.25,y+r*.25],[x-r,y],[x-r*.3,y-r*.25]],col,true);}
  var seed=169;function rand(){seed=(seed*25173+13849)%65536;return seed/65536;}
  function sky(){
    box(-ox/s,-oy/s,canvas.width/s,canvas.height/s,'#0b132a');
    for(var y=0;y<280;y+=4){var t=y/280;c.fillStyle='rgb('+Math.round(11+9*t)+','+Math.round(19+16*t)+','+Math.round(42+13*t)+')';c.fillRect(ox,oy+y*s,240*s,4*s);}
    /* The last blush of dusk, below a sky already being repaired. */
    path([[0,213],[240,170],[240,249],[0,263]],'#202d43',true);
    path([[0,239],[240,197],[240,260],[0,279]],'#2a3547',true);
    for(var i=0;i<63;i++){var x=10+rand()*220,y=12+rand()*203;box(x,y,i%9?1:1.6,1,i%4?'#5a6d86':'#b4c4c6');}
    star(33,56,3,'#c2e0da');star(203,101,2.8,'#87bcbe');star(53,171,2.4,'#d2b895');star(184,28,2.1,'#f2d2a0');
    line([[25,115],[36,128],[30,146],[46,151]],'#334960',.6);
    for(var j=0;j<4;j++){var points=[[25,115],[36,128],[30,146],[46,151]];oval(points[j][0],points[j][1],1,1,'#86a7b5');}
    /* Sparse, dark rooftops make room for a small, improbable occupation. */
    for(var i=0;i<9;i++){var x=i*31-7,y=230+rand()*13;box(x,y,23,50,'#142539');path([[x-3,y],[x+11,y-14],[x+26,y]],'#122033',true);if(i%3===0)box(x+8,y+9,3,5,'#8e7660');}
  }
  function moon(){
    oval(114,90,67,67,'#202e41');oval(114,90,61,61,'#344047');
    oval(114,90,56,56,'#80795c');oval(114,89,53.5,53.5,'#d9c58a');
    oval(110,85,49,49,'#eddba0');oval(105,81,43,43,'#f4e6b5');
    /* Quiet craters, rather than a perfectly featureless disc. */
    var craters=[[83,67,8,6],[95,109,10,7],[140,86,7,9],[103,53,4,3],[82,92,4,3],[135,115,5,4]];
    for(var i=0;i<craters.length;i++){var a=craters[i];oval(a[0],a[1],a[2],a[3],'#dacb98');oval(a[0]-1,a[1]-1,a[2]-1,a[3]-1,'#e8d9a7');}
    /* A real missing sliver of night; the gold bridges it instead of hiding it. */
    var crack=[[120,36],[112,53],[122,68],[113,83],[121,98],[110,113],[119,127],[111,143],[117,143],[125,127],[116,113],[127,98],[119,83],[128,68],[118,53],[126,37]];
    path(crack,'#142139',true);
    line([[120,37],[113,53],[123,68],[114,83],[122,98],[111,113],[120,127],[112,142]],'#a28c61',.8);
    /* Upper stitches are finished. The last one is still on the needle. */
    var stitches=[[118,47],[116,60],[122,73],[116,86],[123,99]];
    for(var i=0;i<stitches.length;i++){var x=stitches[i][0],y=stitches[i][1];line([[x-6,y-2],[x+7,y+2]],'#ad8146',2.2);line([[x-6,y-3],[x+7,y+1]],'#fff1b9',1);oval(x-6,y-2,1,1,'#786748');oval(x+7,y+2,1,1,'#786748');}
    star(121,103,2.2,'#fff6cf');
  }
  function roofs(){
    path([[-4,258],[79,222],[168,258],[168,280],[-4,280]],'#213b49',true);
    path([[-6,258],[79,217],[173,258],[165,263],[79,228],[0,263]],'#426371',true);
    line([[0,256],[79,220],[170,259]],'#78928e',1.1);
    for(var i=0;i<6;i++){var y=236+i*6,half=(y-222)*2.1;line([[79-half,y],[79+half,y]],'#2e4c58',.7);}
    box(51,245,29,35,'#314c54');path([[47,247],[65,233],[84,247]],'#1a2d3f',true);
    box(57,250,16,24,'#ac885a');box(59,252,12,20,'#ffd598');box(64,251,1.5,23,'#624e42');box(58,261,14,1.5,'#624e42');
    /* On the next roof, a ladder whose top rests against the moonlight. */
    path([[141,268],[192,238],[247,262],[247,280],[141,280]],'#162b3d',true);
    line([[141,269],[192,239],[245,263]],'#506b78',2);
    line([[172,248],[139,144]],'#263546',5);line([[186,244],[153,140]],'#263546',5);
    line([[172,248],[139,144]],'#aa906a',2);line([[186,244],[153,140]],'#b7a382',2);
    for(var i=0;i<11;i++){var t=i/10;line([[141+31*t,149+96*t],[154+31*t,145+96*t]],'#af9975',1.5);}
    /* A spool of actual moon-thread, left beside the warm attic window. */
    oval(91,236,9,3,'#543e36');box(85,223,12,13,'#c09553');
    for(var i=0;i<6;i++)line([[85,225+i*1.7],[96,225+i*1.7]],i%2?'#f5cf80':'#d8ad66',.8);
    oval(91,223,9,3,'#c29363');oval(91,223,3,1,'#6d5040');
    line([[92,222],[96,211],[113,201],[124,187],[128,171],[143,157]],'#c9b784',.7);
  }
  function mender(){
    /* A fox, far too small for the job, doing it anyway. */
    path([[171,187],[183,189],[191,182],[195,170],[202,183],[202,195],[195,203],[183,206],[173,201]],'#b6623f',true);
    path([[191,182],[195,170],[202,183],[202,190],[195,190]],'#eee0bc',true);
    path([[165,190],[162,202],[166,211],[172,211]],'#b16a49',false,4);
    path([[175,194],[180,200],[177,209],[183,209]],'#d48b59',false,4);
    path([[160,174],[172,173],[179,191],[174,199],[162,195],[158,182]],'#ca7946',true);
    path([[161,177],[167,177],[172,194],[165,195]],'#eed6a7',true);
    line([[162,180],[152,172],[145,156]],'#e29b64',4);
    oval(145,156,2.1,2.1,'#f3d7a0');
    /* Pointed ears and a pale muzzle remain readable at native resolution. */
    path([[153,160],[151,149],[161,157],[166,155],[172,148],[174,162],[171,173],[160,175],[149,169],[146,166]],'#df8d53',true);
    path([[154,158],[153,152],[159,158]],'#82514a',true);
    path([[168,158],[171,152],[172,160]],'#87504a',true);
    path([[149,165],[157,168],[167,169],[163,174],[154,172],[146,167]],'#f2dfb9',true);
    oval(147,166,1.7,1.4,'#172537');oval(156,162,1,1.4,'#152838');
    line([[152,159],[156,158],[158,159]],'#7d493d',.7);
    /* A turquoise scarf, tugged sideways by the rooftop wind. */
    path([[159,173],[170,171],[173,176],[161,178]],'#65bbb4',true);
    path([[169,175],[183,179],[192,187],[188,192],[178,182],[168,178]],'#379597',true);
    line([[171,176],[182,180],[190,187]],'#92d0bd',.7);
    /* The needle and loop connect our tiny worker to the unfinished seam. */
    line([[149,164],[133,135]],'#fff0bb',1);
    line([[134,138],[139,135],[142,126],[132,116],[119,111]],'#ffe7a4',.8);
    line([[149,164],[153,164],[155,160],[153,156],[145,156]],'#d3bd88',.7);
    star(119,110,2.7,'#fff5c3');
    /* Two scraps of light that fell out of the repair. */
    path([[98,151],[102,147],[104,152],[101,155]],'#e4ce91',true);
    path([[90,168],[93,165],[94,169]],'#a79976',true);
    if(complete)complete();
  }
  sky();setTimeout(function(){moon();setTimeout(function(){roofs();setTimeout(mender,30);},30);},30);
};
