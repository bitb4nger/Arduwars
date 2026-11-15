import sys,glob
from PIL import Image, ImageDraw
d=sys.argv[1]; out=sys.argv[2]; cols=int(sys.argv[3]) if len(sys.argv)>3 else 3
fs=sorted(glob.glob(d+'/*.pgm'))
rows=(len(fs)+cols-1)//cols
S=Image.new('RGB',(cols*262,rows*142),(70,30,70)); dr=ImageDraw.Draw(S)
for i,f in enumerate(fs):
    x,y=(i%cols)*262,(i//cols)*142
    S.paste(Image.open(f).convert('RGB').resize((256,128),Image.NEAREST),(x+3,y+12)); dr.text((x+3,y),f.split('/')[-1],fill=(255,255,0))
S.save(out)
