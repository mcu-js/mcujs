import sys,re,json
from pathlib import Path
s=Path(sys.argv[1]).read_text(); names=['__HeapLimit','__end__','__StackBottom','__StackTop','__StackOneBottom','__StackOneTop','__scratch_x_start__','__scratch_x_end__']
v={k:int(re.search(r'0x([0-9a-f]+)\s+'+k+r'\s*=',s).group(1),16) for k in names}
assert v['__StackTop']-v['__StackBottom']>=12288, 'Canvas requires a genuinely reserved 12 KiB main stack'
ranges={'heap':(v['__end__'],v['__HeapLimit']),'main stack':(v['__StackBottom'],v['__StackTop']),'core1 stack':(v['__StackOneBottom'],v['__StackOneTop']),'TMDS code':(v['__scratch_x_start__'],v['__scratch_x_end__'])}
for i,(a,(lo,hi)) in enumerate(ranges.items()):
 for b,(bl,bh) in list(ranges.items())[i+1:]:assert hi<=bl or bh<=lo, f'{a} overlaps {b}'
print(json.dumps({'stack_layout_pass':True,'ranges':ranges}))
