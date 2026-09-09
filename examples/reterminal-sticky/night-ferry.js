/* THE NIGHT FERRY — original procedural ink for a sleeping world.
 * Canvas 2D subset only. One still frame; no network, fonts, bitmaps or timers.
 * Design 800x480. Uniformly fitted to the supplied display canvas.
 */
module.exports=function(canvas,complete){
 var c=canvas.getContext('2d'),s=Math.min(canvas.width/800,canvas.height/480),ox=(canvas.width-800*s)/2,oy=(canvas.height-480*s)/2;
 var W='#ffffff',B='#000000',seed=90726;
 function rand(){seed=(seed*25173+13849)%65536;return seed/65536;}
 function box(x,y,w,h,col){c.fillStyle=col;c.fillRect(ox+x*s,oy+y*s,w*s,h*s);}
 function path(p,col,fill,width){if(p.length>40){var small=[];for(var j=0;j<40;j++)small.push(p[Math.round(j*(p.length-1)/39)]);p=small;}c.beginPath();c.moveTo(ox+p[0][0]*s,oy+p[0][1]*s);for(var i=1;i<p.length;i++)c.lineTo(ox+p[i][0]*s,oy+p[i][1]*s);if(fill){c.closePath();c.fillStyle=col;c.fill();}else{c.strokeStyle=col;c.lineWidth=(width||1)*s;c.stroke();}}
 function line(p,col,width){path(p,col,false,width);}
 function oval(x,y,rx,ry,col){var p=[];for(var i=0;i<48;i++){var a=i*Math.PI/24;p.push([x+rx*Math.cos(a),y+ry*Math.sin(a)]);}path(p,col,true);}
 function ring(x,y,rx,ry,col,width){var p=[];for(var i=0;i<=64;i++){var a=i*Math.PI/32;p.push([x+rx*Math.cos(a),y+ry*Math.sin(a)]);}line(p,col,width);}
 function bez(p0,p1,p2,p3,n){var p=[];for(var i=0;i<=(n||24);i++){var t=i/(n||24),u=1-t;p.push([u*u*u*p0[0]+3*u*u*t*p1[0]+3*u*t*t*p2[0]+t*t*t*p3[0],u*u*u*p0[1]+3*u*u*t*p1[1]+3*u*t*t*p2[1]+t*t*t*p3[1]]);}return p;}
 function curve(a,b,d,e,col,width){line(bez(a,b,d,e),col,width);}
 function star(x,y,r){path([[x,y-r],[x+r*.22,y-r*.22],[x+r,y],[x+r*.22,y+r*.22],[x,y+r],[x-r*.22,y+r*.22],[x-r,y],[x-r*.22,y-r*.22]],W,true);}
 function hatch(p,q,count,col){for(var i=0;i<count;i++){var t=i/(count-1);line([[p[0]+t*p[2],p[1]+t*p[3]],[q[0]+t*q[2],q[1]+t*q[3]]],col,.8);}}
 function pine(x,y,h){line([[x,y],[x,y-h]],W,1);for(var i=0;i<8;i++){var t=(i+1)/9;line([[x-h*t*.23,y-h+h*t],[x,y-h+h*t-h*.12],[x+h*t*.23,y-h+h*t]],W,.9);}}
 function window(x,y,w,h){box(x-1,y-1,w+2,h+2,B);box(x,y,w,h,W);line([[x+w/2,y],[x+w/2,y+h]],B,.8);line([[x,y+h*.45],[x+w,y+h*.45]],B,.8);}
 function house(x,y,w,h,roof){box(x,y-h,w,h,W);box(x+2,y-h+2,w-4,h-2,B);path([[x-4,y-h],[x+w*.48,y-h-roof],[x+w+4,y-h]],W,true);line([[x+2,y-h-1],[x+w*.48,y-h-roof+4],[x+w-1,y-h-1]],B,1.1);box(x+w*.65,y-h-roof+1,4,roof*.6,W);for(var j=0;j<2;j++){for(var i=0;i<2;i++)window(x+5+i*(w-12)/2,y-h+7+j*(h-15)/2,4.5,7);}box(x+w/2-3,y-10,6,10,W);box(x+w/2-1,y-8,3,8,B);}
 // A fine double rule and an ink-blue sky, rendered in actual black and white.
 box(0,0,canvas.width/s,canvas.height/s,W);box(13,13,774,454,B);box(15,15,770,450,W);box(19,19,762,406,B);
 for(var i=0;i<200;i++){var x=28+rand()*742,y=27+rand()*340;box(Math.floor(x),Math.floor(y),rand()<.13?1.6:1,rand()<.13?1.6:1,W);}
 // Deliberately quiet sky on the right; a little navigational constellation.
 var stars=[[580,58,3],[621,77,2],[657,47,3],[708,63,2],[685,111,2],[741,125,3]];
 for(var i=0;i<stars.length-1;i++){var a=stars[i],b=stars[i+1];for(var j=0;j<11;j++){var t=j/11;box(Math.floor(a[0]+(b[0]-a[0])*t),Math.floor(a[1]+(b[1]-a[1])*t),1,1,W);}}
 for(var i=0;i<stars.length;i++)star(stars[i][0],stars[i][1],stars[i][2]);
 star(52,64,5);star(89,215,3);star(262,63,3);star(525,104,4);star(746,221,4);star(571,329,3);
 // The old moon: concentric engraved halo and a cratered paper disc.
 ring(158,120,77,77,W,.7);ring(158,120,72,72,W,1.3);oval(158,120,65,65,W);
 for(var i=0;i<48;i++){var a=i*Math.PI/24;line([[158+80*Math.cos(a),120+80*Math.sin(a)],[158+(i%4===0?87:83)*Math.cos(a),120+(i%4===0?87:83)*Math.sin(a)]],W,.7);}
 var craters=[[131,81,9],[181,95,15],[143,140,17],[188,141,7],[124,111,5],[163,168,7],[155,102,4]];
 for(var i=0;i<craters.length;i++){var a=craters[i];ring(a[0],a[1],a[2],a[2]*.72,B,.7);curve([a[0]-a[2],a[1]],[a[0]-a[2]*.2,a[1]+a[2]],[a[0]+a[2]*.9,a[1]+a[2]*.6],[a[0]+a[2],a[1]],B,.6);}
 for(var i=0;i<280;i++){var x=97+rand()*122,y=59+rand()*122;if((x-158)*(x-158)+(y-120)*(y-120)<3900&&rand()<(x-93)/140)box(x,y,.65,.65,B);}
 // Remote islands: three tones made of contour lines rather than gray pixels.
 var ridges=[[[20,336],[63,313],[112,326],[160,296],[219,320],[251,304],[306,333],[346,317],[390,342]],[[404,349],[453,320],[503,339],[550,306],[593,328],[636,294],[695,320],[745,310],[781,337]]];
 for(var i=0;i<ridges.length;i++){line(ridges[i],W,.65);for(var j=1;j<4;j++){var p=[];for(var k=0;k<ridges[i].length;k++)p.push([ridges[i][k][0],ridges[i][k][1]+j*5]);line(p,W,.5);}}
 // Tiny towns below the night ferry, each window a person asleep.
 for(var i=0;i<19;i++){var x=29+i*17.7,y=374+Math.sin(i*.7)*7,h=10+rand()*16;box(x,y-h,13,h,W);path([[x-2,y-h],[x+6,y-h-6-rand()*5],[x+15,y-h]],W,true);box(x+2,y-h+2,9,h-2,B);box(x+4,y-h+5,2,3,W);box(x+8,y-h+5,2,3,W);if(h>19)box(x+5,y-6,3,4,W);}
 curve([20,388],[145,366],[248,401],[379,380],W,1);
 // A pale wake, suspended above the landscape. Breaks are purposeful.
 for(var i=0;i<12;i++){var y=321+i*4.5;curve([127-i*6,y],[236,y+30],[329,y-9],[471-i*8,y+4],W,i%3===0?1.1:.5);}
 // The ferry is a whale. A long, clean silhouette carries the architecture.
 var body=bez([197,250],[236,210],[339,211],[415,223],26);
 body=body.concat(bez([415,223],[503,223],[557,237],[601,255],22));
 body=body.concat(bez([601,255],[636,269],[626,299],[602,310],20));
 body=body.concat(bez([602,310],[510,352],[329,337],[260,293],30));
 body=body.concat(bez([260,293],[222,271],[204,269],[197,250],14));
 path(body,B,true);line(body,W,3);path(body,W,true);
 // Tail arcs backward into the moonlight, forked rather than a fish triangle.
 var tail=bez([239,270],[188,261],[170,233],[156,205],16);
 tail=tail.concat(bez([156,205],[135,203],[105,191],[98,164],16));
 tail=tail.concat(bez([98,164],[116,176],[149,172],[173,189],16));
 tail=tail.concat(bez([173,189],[182,155],[211,147],[237,143],16));
 tail=tail.concat(bez([237,143],[217,166],[208,193],[184,208],16));
 tail=tail.concat(bez([184,208],[193,230],[216,242],[252,251],16));path(tail,B,true);line(tail,W,2);
 curve([108,174],[143,201],[168,185],[177,204],W,.9);curve([176,203],[191,183],[211,163],[229,151],W,.9);
 for(var i=0;i<9;i++)curve([125+i*6,187+i*.5],[162+i*2,204],[170+i*3,242],[232+i*2,262],W,.6);
 // Black dorsal sweep, leaving a white throat and an expressive eye.
 var back=bez([212,253],[308,219],[428,224],[508,243],28);back=back.concat(bez([508,243],[554,248],[591,253],[612,272],18));back=back.concat(bez([612,272],[531,259],[419,298],[299,277],24));back=back.concat([[241,268]]);path(back,B,true);
 curve([229,253],[349,227],[458,234],[538,253],W,.9);
 for(var i=0;i<90;i++){var x=249+rand()*310,y=244+rand()*25;if(y>249+(x-420)*.015)box(x,y,.9,.6,W);}
 // Pleated whale throat: nested long curves, not uniform horizontal stripes.
 for(var i=0;i<12;i++){var y=277+i*3.7;curve([280+i*7,282+i*.4],[384,y+22],[536,y+34-i*2],[609-i*.75,286+i*1.6],B,.9);}
 oval(590,271,5,4,B);oval(591,270,1.35,1.35,W);
 curve([577,289],[595,291],[612,285],[621,279],B,1.4);
 // The nearest fin overlaps the engraving and points toward home.
 var fin=bez([424,281],[460,285],[476,329],[468,357],22);fin=fin.concat(bez([468,357],[445,346],[416,312],[400,290],22));path(fin,W,true);line(fin,B,1.8);
 for(var i=0;i<6;i++)curve([408+i*4,291],[434+i*3,312],[451+i*2,338],[465,351],B,.6);
 // A promenade on the whale's back, with a practical little railing.
 line([[254,226],[550,245]],B,6);line([[254,224],[550,243]],W,2);
 for(var i=0;i<38;i++){var x=258+i*7.7,y=224+(x-254)*19/296;line([[x,y],[x,y-8]],W,.8);}line([[257,216],[548,235]],W,.8);
 // The observatory's left annex and sleeping crew cottages.
 house(260,223,28,32,15);house(292,225,31,44,17);house(475,237,28,35,14);house(508,240,29,29,13);
 // Glass conservatory dome. Paper ribs against the sky, tropical leaves within.
 box(334,177,128,54,W);box(337,177,122,51,B);
 var dome=bez([334,178],[333,88],[463,88],[462,178],40);dome=dome.concat([[462,179],[334,179]]);path(dome,W,true);
 var inner=bez([338,177],[340,95],[456,95],[458,177],36);inner=inner.concat([[458,179],[338,179]]);path(inner,B,true);
 curve([350,178],[350,119],[383,103],[398,108],W,1);curve([377,178],[376,113],[390,105],[398,107],W,1);
 curve([446,178],[446,119],[414,103],[398,108],W,1);curve([420,178],[421,113],[406,105],[398,107],W,1);
 line([[398,105],[398,228]],W,1.2);
 curve([340,158],[374,147],[422,147],[457,158],W,1);curve([347,135],[379,128],[420,128],[451,135],W,.8);
 line([[334,179],[462,179]],W,3);line([[335,205],[460,205]],W,1.6);
 for(var i=0;i<8;i++)line([[338+i*17,178],[338+i*17,228]],W,1.1);
 box(330,226,137,5,W);box(330,229,137,1,B);
 // Plants, a spiral stair, and hanging lamps make it a place rather than a logo.
 for(var i=0;i<6;i++){var x=348+i*18,y=224,h=20+rand()*18;line([[x,y],[x+2,y-h]],W,.8);for(var j=0;j<4;j++){var yy=y-7-j*6;path([[x+1,yy],[x-6,yy-7],[x-9,yy-8],[x-5,yy-1]],W,true);path([[x+2,yy-3],[x+8,yy-9],[x+10,yy-8],[x+6,yy]],W,true);}}
 line([[408,161],[408,225]],W,1);for(var i=0;i<12;i++){var yy=164+i*5;line([[402+(i%2)*2,yy],[416-(i%2)*2,yy+2]],W,.8);}
 for(var i=0;i<3;i++){var x=365+i*33;line([[x,144],[x,165]],W,.65);oval(x,169,2.4,4,W);}
 box(390,100,17,5,W);box(396,91,5,10,W);oval(398.5,88,3,3,W);line([[399,86],[399,78]],W,.8);
 // A tiny telescope on the roof of the right-hand navigation cabin.
 house(436,228,29,26,10);line([[448,192],[440,201]],W,2);line([[448,192],[455,201]],W,2);
 path([[443,189],[470,176],[473,183],[446,196]],W,true);line([[466,177],[470,185]],B,1.1);
 // Smoke becomes a string of small stars, not a loud exhaust plume.
 curve([307,165],[289,146],[312,142],[309,127],W,.8);curve([309,127],[306,116],[321,112],[329,110],W,.65);star(333,105,2);
 // A soft cargo net and a single hanging lantern under the tail.
 curve([263,289],[263,310],[267,326],[280,331],W,.7);line([[280,330],[280,345]],W,.8);box(276,344,9,12,B);box(277,345,7,10,W);box(280,345,1,10,B);path([[275,344],[280,340],[286,344]],W,true);
 // Foreground: the landing where the keeper waits. A small human scale.
 var cliff=[[575,398],[605,381],[628,382],[657,359],[684,373],[712,369],[735,384],[780,373],[780,425],[570,425]];path(cliff,B,true);line(cliff,W,1.3);
 for(var i=0;i<22;i++){var x=588+i*8;line([[x,402+Math.sin(i)*6],[x-7,420]],W,.6);}
 line([[572,375],[674,375]],W,3);line([[575,379],[674,379]],W,1);
 for(var i=0;i<5;i++){var x=579+i*20;line([[x,375],[x+6,402]],W,1.2);}line([[584,399],[665,381]],W,.8);
 // The keeper, coat lifted slightly by the whale's passing.
 oval(616,343,4,4,W);path([[612,349],[619,348],[623,366],[609,367]],W,true);line([[613,364],[611,374]],W,2);line([[619,364],[621,374]],W,2);
 line([[618,351],[626,355],[631,350]],W,1.7);line([[630,341],[630,350]],W,.7);box(627,337,7,9,W);box(630,339,1,6,B);path([[626,337],[630,333],[635,337]],W,true);
 // A mooring cable catches the light between impossible creature and ordinary dock.
 curve([605,309],[619,327],[590,354],[575,375],W,.7);
 // Keeper's house on the far bank, roof tiles and a crescent above the door.
 house(686,391,48,41,25);box(718,328,7,24,W);box(719,330,4,20,B);
 for(var j=0;j<4;j++)line([[686+j*4,348+j*4],[713+j*4,348+j*4]],B,.7);
 oval(710,373,3,3,W);oval(712,372,2.5,2.5,B);
 pine(757,394,39);pine(774,403,50);pine(666,393,24);
 // Starfall above the house: one shooting star, three diminishing echoes.
 curve([740,169],[714,181],[695,199],[679,216],W,1.1);star(677,219,3);
 for(var i=0;i<3;i++)curve([749-i*5,169+i*8],[732,181+i*8],[717,191+i*8],[700,204+i*8],W,.55);
 // Sparse sea-current marks along the bottom, beneath the remote town.
 for(var i=0;i<29;i++){var x=31+rand()*508,y=393+rand()*24;line([[x,y],[x+8+rand()*20,y]],W,rand()<.3?1:.55);}
 // Letterpress-style title in our own tiny geometric alphabet (no text API).
 var glyph={A:['01110','10001','10001','11111','10001','10001','10001'],B:['11110','10001','10001','11110','10001','10001','11110'],C:['01111','10000','10000','10000','10000','10000','01111'],D:['11110','10001','10001','10001','10001','10001','11110'],E:['11111','10000','10000','11110','10000','10000','11111'],F:['11111','10000','10000','11110','10000','10000','10000'],G:['01111','10000','10000','10111','10001','10001','01110'],H:['10001','10001','10001','11111','10001','10001','10001'],I:['111','010','010','010','010','010','111'],K:['10001','10010','10100','11000','10100','10010','10001'],L:['10000','10000','10000','10000','10000','10000','11111'],M:['10001','11011','10101','10101','10001','10001','10001'],N:['10001','11001','10101','10011','10001','10001','10001'],O:['01110','10001','10001','10001','10001','10001','01110'],P:['11110','10001','10001','11110','10000','10000','10000'],R:['11110','10001','10001','11110','10100','10010','10001'],S:['01111','10000','10000','01110','00001','00001','11110'],T:['11111','00100','00100','00100','00100','00100','00100'],U:['10001','10001','10001','10001','10001','10001','01110'],W:['10001','10001','10001','10101','10101','11011','10001'],Y:['10001','10001','01010','00100','00100','00100','00100']};
 function text(t,y,k,space){var len=0;for(var i=0;i<t.length;i++)len+=(t[i]===' '?3:glyph[t[i]][0].length)+space;var x=(800-(len-space)*k)/2;for(var i=0;i<t.length;i++){if(t[i]===' '){x+=(3+space)*k;continue;}var g=glyph[t[i]];for(var r=0;r<7;r++)for(var q=0;q<g[r].length;q++)if(g[r][q]==='1')box(x+q*k,y+r*k,k,k,B);x+=(g[0].length+space)*k;}}
 text('THE NIGHT FERRY',434,1.5,2);text('SOMEONE KEEPS THE STARS LIT',451,1,2);
 line([[39,440],[246,440]],B,.65);line([[554,440],[761,440]],B,.65);
 for(var x=30;x<=770;x+=740){path([[x,437],[x+3,440],[x,443],[x-3,440]],B,true);}
 if(complete)complete();
};
