# drawstate.py LOG MAPINDEX OUT : render the "S" state lines of an autoplay log as full-map pictures
import sys
from PIL import Image, ImageDraw
log, mi, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
D='/home/claude/awfx/repo/ArduwarsFX/fxdata/'
txt=open(D+'fxdata.txt').read()
names=['mapSkirmish','mapBattle','mapArduwars']
seg=txt[txt.index('uint8_t '+names[mi]):]
seg=seg[seg.index('=')+1:]
nums=[]
for tok in seg.split():
    if tok.isdigit(): nums.append(int(tok))
    elif nums: break
w,h=nums[0],nums[1]; tiles=nums[10:10+w*h]
T=Image.open(D+'worldTiles_16x16.png').convert('RGBA')
UA=Image.open(D+'unitsA_16x16.png').convert('RGBA'); UB=Image.open(D+'unitsB_16x16.png').convert('RGBA')
def tile(img,i): return img.crop((i*16,0,i*16+16,16))
states=[l for l in open(log) if l.startswith('S')]
days=[l for l in open(log) if l.startswith('D')]
pics=[]
for k,l in enumerate(states):
    parts=l.split(' B ')
    units=[tuple(map(int,x.split(','))) for x in parts[0].split()[1:]]
    blds=[tuple(map(int,x.split(','))) for x in parts[1].split()] if len(parts)>1 else []
    im=Image.new('RGBA',(w*16,h*16),(0,0,0,255))
    for y in range(h):
        for x in range(w):
            im.alpha_composite(tile(T,tiles[y*w+x]),(x*16,y*16))
    for y in range(h-1):
        for x in range(w):
            if tiles[(y+1)*w+x] in (30,31): im.alpha_composite(tile(T,32 if tiles[(y+1)*w+x]==30 else 33),(x*16,y*16))
    d=ImageDraw.Draw(im)
    for bx,by,o in blds:
        if o<9: d.rectangle([bx*16,by*16,bx*16+15,by*16+15],outline=(0,120,255,255) if o==0 else (255,60,0,255),width=2)
    for p,t,x,y,hp in units:
        im.alpha_composite(tile(UA if p==0 else UB,t*2),(x*16,y*16))
        d.rectangle([x*16,y*16,x*16+15,y*16+15],outline=(0,120,255,255) if p==0 else (255,60,0,255))
        if hp<10: d.text((x*16+10,y*16+7),str(hp),fill=(255,0,255,255))
    pics.append(im)
cols=2; W=w*16+8; H=h*16+8
S=Image.new('RGB',(cols*W,((len(pics)+cols-1)//cols)*H),(60,60,60))
for i,p in enumerate(pics): S.paste(p.convert('RGB'),((i%cols)*W,(i//cols)*H))
S.save(out); print(len(pics),'states')
