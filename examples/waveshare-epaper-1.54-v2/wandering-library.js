/* The Wandering Library. One ink drawing, one task, one full e-paper refresh. */
module.exports=function(canvas,complete){
 var c=canvas.getContext('2d'),s=Math.min(canvas.width/200,canvas.height/200),ox=(canvas.width-200*s)/2,oy=(canvas.height-200*s)/2;
 var ink='#000000',paper='#ffffff';
 function box(x,y,w,h,col){c.fillStyle=col;c.fillRect(ox+x*s,oy+y*s,w*s,h*s);}
 function path(p,col,fill,lw){c.beginPath();c.moveTo(ox+p[0][0]*s,oy+p[0][1]*s);for(var i=1;i<p.length;i++)c.lineTo(ox+p[i][0]*s,oy+p[i][1]*s);if(fill){c.closePath();c.fillStyle=col;c.fill();}else{c.strokeStyle=col;c.lineWidth=(lw||1)*s;c.stroke();}}
 function line(p,col,w){path(p,col,false,w);}
 function oval(x,y,rx,ry,col){var p=[];for(var i=0;i<32;i++){var a=i*Math.PI/16;p.push([x+Math.cos(a)*rx,y+Math.sin(a)*ry]);}path(p,col,true);}
 function star(x,y,r){path([[x,y-r],[x+.6,y-.6],[x+r,y],[x+.6,y+.6],[x,y+r],[x-.6,y+.6],[x-r,y],[x-.6,y-.6]],paper,true);}
 var seed=154;function rand(){seed=(seed*25173+13849)%65536;return seed/65536;}
 c.fillStyle=paper;c.fillRect(0,0,canvas.width,canvas.height);
 box(8,8,184,184,ink);box(10,10,180,180,paper);box(13,13,174,148,ink);
 /* A moon and pinpricks cut straight out of the ink. */
 oval(44,42,20,20,paper);oval(49,38,16,16,ink);
 for(var i=0;i<31;i++){var x=18+rand()*163,y=19+rand()*111;if((x-44)*(x-44)+(y-42)*(y-42)>520)box(x,y,.8,.8,paper);}
 star(80,25,3);star(171,31,2.6);star(28,80,2.3);star(97,41,1.8);
 /* A few hills are enough to say how far the little house has come. */
 line([[13,123],[27,113],[42,124],[59,105],[81,125]],paper,.7);
 line([[159,127],[176,115],[187,123]],paper,.7);
 /* The lamp's pool is negative space, not grey paint. */
 path([[64,111],[34,165],[94,165]],paper,true);
 /* The shell is architecture, but still unmistakably a shell. */
 oval(120,94,59,59,ink);oval(120,94,57,57,paper);oval(120,94,53,53,ink);oval(120,94,51.5,51.5,paper);
 var spiral=[];for(var i=0;i<37;i++){var a=i*.18,r=20-i*.45;spiral.push([140+r*Math.cos(a),67+r*Math.sin(a)]);}line(spiral,ink,1.1);
 for(var i=0;i<18;i++){var a=1.45+i*.10;line([[120+54*Math.cos(a),94+54*Math.sin(a)],[120+48*Math.cos(a+.035),94+48*Math.sin(a+.035)]],ink,.6);}
 /* A deep arched bookshop window, tucked into the white shell. */
 oval(116,86,30,30,ink);box(86,85,60,59,ink);
 oval(116,86,27,27,paper);box(89,85,54,57,paper);
 oval(116,85,23,23,ink);box(93,85,46,55,ink);
 /* Fanlight: spokes in a half-moon above the shelves. */
 line([[94,83],[138,83]],paper,1);
 for(var i=0;i<7;i++){var a=Math.PI+i*Math.PI/6;line([[116,83],[116+21*Math.cos(a),83+21*Math.sin(a)]],paper,.75);}
 /* Uneven spines, bindings and leaning books — no fonts or bitmaps. */
 for(var row=0;row<3;row++){
  var base=101+row*13;box(93,base,46,1.5,paper);
  var x=96;
  for(var j=0;j<8;j++){
   var h=6+Math.floor(rand()*5),w=2+Math.floor(rand()*3);
   if(x+w>137)break;
   box(x,base-h,w,h-1,paper);if(w>=3)box(x+.6,base-h+2,w-1.2,.6,ink);
   x+=w+1.5;
  }
 }
 /* Open door and a tiny stair, inviting rather than fortress-like. */
 box(109,118,16,28,paper);box(111,119,12,27,ink);
 path([[112,121],[121,123],[121,145],[112,145]],paper,true);oval(118,135,.8,.8,ink);
 box(105,145,24,2,ink);box(102,148,30,2,ink);box(99,151,36,2,ink);
 /* Hanging lamp, with a handle a person could actually mend. */
 line([[89,96],[73,96],[65,102],[65,111]],ink,3);line([[87,96],[73,96],[65,102],[65,111]],paper,1);
 box(60,111,11,14,ink);box(62,113,7,9,paper);box(65,113,1,9,ink);
 path([[58,111],[65,107],[73,111]],ink,true);box(59,124,13,2,ink);
 /* Our patient bookseller carries the entire establishment. */
 path([[33,137],[40,130],[48,129],[57,139],[78,148],[114,151],[155,148],[172,158],[183,162],[181,166],[153,170],[94,171],[58,166],[35,155]],ink,true);
 path([[36,138],[42,133],[48,133],[54,141],[77,153],[113,156],[154,153],[170,161],[176,163],[151,166],[96,167],[59,161],[38,152]],paper,true);
 line([[44,136],[37,105]],ink,3);line([[49,136],[55,114]],ink,3);
 line([[44,133],[37,105]],paper,.85);line([[49,133],[55,114]],paper,.85);
 oval(37,104,4,4,ink);oval(37,103,2,2,paper);oval(36.5,103,1,1,ink);
 oval(55,113,3.5,3.5,ink);oval(55,112,1.6,1.6,paper);
 line([[35,147],[41,150],[48,148]],ink,.8);
 for(var i=0;i<39;i++){var x=60+rand()*96,y=158+rand()*6;box(x,y,i%4?.7:1.2,.65,ink);}
 /* The trail catches just a little moonlight. */
 for(var i=0;i<5;i++){var y=173+i*2;line([[99+i*5,y],[143+i*4,y-1],[184,y+2]],ink,i===0?1:.5);}
 /* One reader has come out in their nightclothes. */
 oval(62,160,4.2,4.8,ink);path([[58,157],[61,151],[67,157]],ink,true);
 path([[58,166],[65,164],[70,172],[67,178],[58,179],[55,173]],ink,true);
 line([[59,177],[55,183],[63,184]],ink,2.3);line([[66,177],[71,181],[77,180]],ink,2.3);
 path([[67,166],[74,167],[79,164],[81,174],[75,177],[68,173]],ink,true);
 path([[68,167],[73,169],[74,175],[69,172]],paper,true);path([[75,169],[78,166],[79,173],[75,175]],paper,true);
 line([[65,170],[68,171]],paper,1.2);
 /* Paper-grain grass and small mushrooms, away from the reader's silhouette. */
 for(var i=0;i<20;i++){var x=17+i*2.1,h=2+rand()*5;line([[x,186],[x-1,186-h]],ink,.6);}
 line([[27,181],[27,173]],ink,1);oval(27,172,5,3,ink);box(23,173,8,2,paper);
 line([[36,183],[36,178]],ink,.8);oval(36,177,3.5,2,ink);
 line([[145,184],[148,180],[149,184],[154,181]],ink,.7);
 /* A bookplate ornament, without a single rendered letter. */
 line([[81,188],[95,188]],ink,.6);line([[105,188],[119,188]],ink,.6);
 path([[97,186],[100,187],[103,186],[103,190],[100,191],[97,190]],ink,true);
 if(complete)complete();
};
