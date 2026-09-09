/* A small elsewhere. Drawing uses only the portable Canvas subset. */
function starFisher(canvas, complete) {
  var ctx=canvas.getContext('2d'), W=canvas.width,H=canvas.height;
  function col(r,g,b){return 'rgb('+Math.round(r)+','+Math.round(g)+','+Math.round(b)+')';}
  function box(x,y,w,h,c){ctx.fillStyle=c;ctx.fillRect(x*W/320,y*H/172,w*W/320,h*H/172);}
  function path(points,c,fill,width){ctx.beginPath();ctx.moveTo(points[0][0]*W/320,points[0][1]*H/172);for(var i=1;i<points.length;i++)ctx.lineTo(points[i][0]*W/320,points[i][1]*H/172);if(fill){ctx.closePath();ctx.fillStyle=c;ctx.fill();}else{ctx.strokeStyle=c;ctx.lineWidth=(width||1)*W/320;ctx.stroke();}}
  function oval(x,y,rx,ry,c,fill,a,start,end){var p=[];a=a||0;start=start||0;end=end===undefined?Math.PI*2:end;for(var i=0;i<=28;i++){var t=start+(end-start)*i/28,u=Math.cos(t)*rx,v=Math.sin(t)*ry;p.push([x+u*Math.cos(a)-v*Math.sin(a),y+u*Math.sin(a)+v*Math.cos(a)]);}path(p,c,fill,1);}
  function star(x,y,s,c){path([[x,y-s],[x+s*.25,y-s*.25],[x+s,y],[x+s*.25,y+s*.25],[x,y+s],[x-s*.25,y+s*.25],[x-s,y],[x-s*.25,y-s*.25]],c,true);}
  var seed=73;function rand(){seed=(seed*25173+13849)%65536;return seed/65536;}
  function sky(){
    for(var y=0;y<112;y+=4){var t=y/112;box(0,y,320,4,col(4+24*t,7+9*t,23+35*t));}
    for(var i=0;i<49;i++){var x=rand()*320,y=rand()*98;box(x,y,1,1,i%4?'#57769d':'#d0f9ff');}
    star(35,23,3,'#b6f5ff');star(126,47,2,'#ffffff');star(288,17,2,'#ffdba4');
    path([[64,39],[80,31],[99,36],[108,22]],'#285273',false);
    for(var j=0;j<4;j++){var pts=[[64,39],[80,31],[99,36],[108,22]];box(pts[j][0],pts[j][1],2,2,'#89cee7');}
  }
  function planet(){
    oval(225,53,58,14,'#754c8e',false,-.30);
    oval(225,53,54,12,'#d4a1c4',false,-.30);
    oval(225,53,27,27,'#342955',true);
    oval(221,50,24,25,'#af6e9b',true);
    oval(215,46,17,21,'#edb9ba',true);
    path([[203,40],[216,43],[232,40],[242,36]],'#e7a8ab',false,3);
    path([[199,51],[213,55],[229,53],[248,46]],'#c384a6',false,3);
    path([[205,65],[220,69],[239,65]],'#865985',false,2);
    oval(225,53,58,14,'#ffe5c6',false,-.30,0,Math.PI);
    oval(225,53,55,12,'#cfb4f0',false,-.30,0,Math.PI);
    oval(225,53,52,10,'#996bac',false,-.30,0,Math.PI);
    star(169,69,3,'#ffffff');
  }
  function landscape(){
    path([[0,108],[0,91],[17,98],[38,83],[59,98],[87,92],[110,105],[143,92],[166,104],[190,98],[206,108],[243,87],[266,102],[290,92],[320,99],[320,113]],'#304166',true);
    path([[0,109],[0,103],[29,98],[52,106],[88,102],[117,110],[158,103],[193,110],[236,103],[276,108],[301,101],[320,108],[320,115]],'#152c49',true);
    for(var y=111;y<172;y+=3){var t=(y-111)/61;box(0,y,320,3,col(16-10*t,53-32*t,72-32*t));}
    box(0,111,320,1,'#48a4ab');
    for(var i=0;i<43;i++){var yy=115+i*1.23,xx=224+(rand()-.5)*(12+i),ww=3+rand()*(5+i*.45);box(xx-ww/2,yy,ww,1,i%3?'#577795':'#c6a5b4');}
    for(var j=0;j<25;j++){var y=117+rand()*53;box(rand()*320,y,3+rand()*13,1,'#1d4a61');}
  }
  function fisher(){
    path([[0,151],[19,142],[41,140],[49,145],[70,149],[79,156],[118,172],[0,172]],'#050c1c',true);
    path([[7,149],[27,142],[42,142],[58,149]],'#436579',false);
    /* Boots, suit, backpack, round helmet and reflected visor. */
    box(36,133,7,11,'#8199ad');box(43,140,10,4,'#dce7ee');box(30,132,7,12,'#c1d5e0');box(27,142,10,4,'#e3edf2');
    box(27,119,7,14,'#506d8c');box(33,121,14,15,'#e7e3ed');box(34,128,11,8,'#a5b9d2');
    oval(39,116,10,10,'#d8e8f5',true);oval(41,115,7,6,'#132842',true);
    path([[36,112],[40,110],[44,111]],'#5ce1eb',false,2);box(45,115,2,3,'#c3a6ec');
    path([[43,123],[51,127],[57,122]],'#f2e8ef',false,3);
    path([[53,127],[71,101],[79,97]],'#daaac0',false,2);
    path([[79,97],[88,104],[94,118],[97,134]],'#9ccee0',false);
    oval(99,146,15,3,'#2c9da6',false);oval(99,146,8,2,'#63d1ce',false);
    star(98,139,7,'#285571');star(98,138,5,'#ffe1a0');star(98,138,2,'#ffffff');
    /* A second tiny world near the horizon. */
    oval(145,84,3,3,'#67b4c8',true);
    if(complete)complete();
  }
  sky();setTimeout(function(){planet();setTimeout(function(){landscape();setTimeout(fisher,20);},20);},20);
}
if(typeof module!=='undefined')module.exports=starFisher;
