/* Somebody's Still Awake. Portable Canvas; uniform 320x240 composition. */
module.exports=function(canvas,complete){
  var c=canvas.getContext('2d'),s=Math.min(canvas.width/320,canvas.height/240);
  var ox=(canvas.width-320*s)/2,oy=(canvas.height-240*s)/2;
  function box(x,y,w,h,col){c.fillStyle=col;c.fillRect(ox+x*s,oy+y*s,w*s,h*s);}
  function path(p,col,fill,lw){c.beginPath();c.moveTo(ox+p[0][0]*s,oy+p[0][1]*s);for(var i=1;i<p.length;i++)c.lineTo(ox+p[i][0]*s,oy+p[i][1]*s);if(fill){c.closePath();c.fillStyle=col;c.fill();}else{c.strokeStyle=col;c.lineWidth=(lw||1)*s;c.stroke();}}
  function oval(x,y,rx,ry,col,fill){var p=[];for(var i=0;i<=24;i++){var a=i*Math.PI/12;p.push([x+Math.cos(a)*rx,y+Math.sin(a)*ry]);}path(p,col,fill,.8);}
  function rgb(r,g,b){return 'rgb('+Math.round(r)+','+Math.round(g)+','+Math.round(b)+')';}
  var seed=283;function rand(){seed=(seed*25173+13849)%65536;return seed/65536;}
  function water(){
    c.fillStyle='#030b16';c.fillRect(0,0,canvas.width,canvas.height);
    for(var y=0;y<240;y+=4){var t=y/240;box(0,y,320,4,rgb(4-2*t,24-15*t,40-19*t));}
    path([[126,0],[151,0],[113,177],[83,224]],'#072131',true);
    path([[189,0],[204,0],[154,181],[126,238]],'#072330',true);
    path([[243,0],[250,0],[223,163],[199,209]],'#082130',true);
    for(var i=0;i<100;i++){var x=rand()*320,y=rand()*230;box(x,y,i%7?1:2,1,i%5?'#214355':'#729b9d');}
    /* Faraway, uninhabited cousins. */
    for(var j=0;j<3;j++){
      var x=25+j*113,y=44+j*15;
      oval(x,y,9-j,5,'#173d50',true);
      for(var k=0;k<3;k++){var p=[];for(var n=0;n<12;n++)p.push([x-5+k*4+Math.sin(n*.4+k)*2,y+n*1.8]);path(p,'#193c4a',false,.5);}
    }
    /* A shoal dissolving into the dark. */
    for(var f=0;f<12;f++){var xx=26+rand()*85,yy=67+rand()*30;path([[xx-3,yy],[xx+2,yy-1],[xx+4,yy+1],[xx+2,yy+1],[xx-3,yy+3]],'#345764',true);}
  }
  function tendrils(){
    for(var i=0;i<13;i++){
      var x=125+i*10,len=68+rand()*43,p=[],am=7+rand()*9,phase=i*.67;
      for(var j=0;j<30;j++){var t=j/29;p.push([x+Math.sin(t*5+phase)*am+t*9,111+t*len]);}
      path(p,i%3===0?'#294860':'#174654',false,i%3===0?4:2);
      path(p,i%3===0?'#8c799b':'#56a59d',false,i%3===0?1:.65);
      if(i%3===1){for(var b=2;b<9;b++){var t=b/9;oval(x+Math.sin(t*5+phase)*am+t*9,111+t*len,1.4,1.4,'#acd6bc',true);}}
    }
    /* Broad, folded feeding ribbons behind the bell. */
    for(var r=0;r<3;r++){
      var p=[],x=159+r*29;
      for(var j=0;j<=16;j++){var t=j/16;p.push([x+Math.sin(t*8+r)*11,117+t*88]);}
      for(var j=16;j>=0;j--){var t=j/16;p.push([x+Math.sin(t*8+r)*11+7*(1-t),117+t*88]);}
      path(p,r===1?'#395368':'#244855',true);
    }
  }
  function bell(){
    var p=[];
    for(var i=0;i<=24;i++){var a=Math.PI+i*Math.PI/24;p.push([190+82*Math.cos(a),110+66*Math.sin(a)]);}
    for(var i=0;i<=12;i++)p.push([272-i*164/12,110+12*Math.sin(i*Math.PI/12)+2*Math.sin(i*Math.PI/3)]);
    path(p,'#103b46',true);path(p,'#79d3bf',false,1.4);
    var inner=[];for(var i=0;i<=24;i++){var a=Math.PI+i*Math.PI/24;inner.push([190+72*Math.cos(a),107+54*Math.sin(a)]);}path(inner,'#174b50',true);
    /* Light caught in the glass canopy. */
    path([[142,79],[153,66],[175,56],[165,68],[153,78]],'#39776f',true);
    path([[178,53],[192,51],[204,55],[185,55]],'#70ae96',true);
    /* A warm little city, sheltered inside a living lantern. */
    var houses=[[131,96,12,16],[145,87,14,26],[161,91,12,23],[201,88,15,26],[219,95,12,19],[234,99,12,14]];
    for(var h=0;h<houses.length;h++){
      var b=houses[h],x=b[0],y=b[1],w=b[2],height=b[3];
      box(x,y,w,height,h%2?'#344c4e':'#4c5350');
      path([[x-2,y],[x+w/2,y-7],[x+w+2,y]],'#253744',true);
      for(var yy=y+4;yy<y+height-2;yy+=7){box(x+3,yy,2,3,'#ffd18c');if(w>12)box(x+9,yy,2,3,'#dd9c62');}
    }
    /* The tower keeps its porch light on. */
    box(178,76,20,37,'#5c6660');box(182,72,12,39,'#8b8270');
    path([[175,76],[188,63],[201,76]],'#202f3b',true);
    box(185,80,5,8,'#ffdea0');box(184,94,7,18,'#f8bb71');box(187,95,1,17,'#905c43');
    path([[173,114],[205,114],[212,120],[165,120]],'#649b8a',true);
    box(181,113,18,2,'#ffe2a2');box(178,116,24,1,'#d9b986');
    /* Canopy ribs curve around the town, not through its windows. */
    for(var r=0;r<5;r++){
      var q=[],base=119+r*35;
      for(var j=0;j<=20;j++){var t=j/20;q.push([190+(base-190)*Math.sin(t*Math.PI/2),47+66*t]);}
      path(q,'#478d85',false,.55);
    }
    var rim=[];for(var i=0;i<=32;i++)rim.push([109+i*162/32,111+4*Math.sin(i*Math.PI/4)]);
    path(rim,'#b8efd0',false,1.4);
    for(var i=0;i<22;i++)oval(114+i*7.1,114+4*Math.sin(i*.7),1.2,1.2,i%3?'#87cbb0':'#ffe3a1',true);
  }
  function visitor(){
    /* Pool of light from a lantern small enough to carry. */
    oval(98,163,15,13,'#18383b',true);oval(98,163,9,8,'#36544a',true);oval(98,163,5,5,'#9b9568',true);
    path([[66,168],[76,177],[70,188],[63,184],[64,175],[58,171]],'#b56857',true);
    path([[61,173],[56,182],[60,190],[56,195]],'#b97b63',false,4);
    path([[67,184],[76,193],[84,191]],'#cc8d70',false,4);
    path([[55,192],[47,198],[59,197],[62,192]],'#689b9d',true);
    path([[81,190],[88,192],[94,188],[84,187]],'#689b9d',true);
    oval(62,164,8,10,'#294b56',true);oval(68,162,10,10,'#a8b9af',true);oval(71,161,7,7,'#183744',true);
    path([[68,156],[72,155],[75,157]],'#f0e2b3',false,1.3);
    path([[73,172],[82,173],[88,163]],'#d29b77',false,3);
    path([[89,162],[96,155],[99,156],[99,159]],'#bdc5a0',false,.8);
    box(95,160,6,7,'#ffdfa0');box(97,161,2,5,'#fff2c9');box(94,159,8,1,'#b4a881');box(94,167,8,1,'#b4a881');
    oval(60,143,3,3,'#679ca6',false);oval(52,132,2,2,'#518896',false);oval(56,120,1.5,1.5,'#446e7d',false);
    /* The seafloor is almost beyond the reach of either light. */
    path([[0,233],[25,222],[54,229],[77,238],[117,232],[149,238],[188,234],[237,238],[286,226],[320,230],[320,240],[0,240]],'#020912',true);
    for(var k=0;k<12;k++){
      var x=k<6?k*9:276+(k-6)*9,len=12+rand()*24,q=[];
      for(var n=0;n<16;n++)q.push([x+Math.sin(n*.21+k)*5,242-n*len/15]);
      path(q,k%2?'#12313b':'#1a4348',false,k%3===0?3:1.4);
    }
    if(complete)complete();
  }
  water();setTimeout(function(){tendrils();setTimeout(function(){bell();setTimeout(visitor,30);},30);},30);
};
