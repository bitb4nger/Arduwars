import subprocess,bisect,sys
elf=sys.argv[1]; pf=sys.argv[2]
syms=[]
for l in subprocess.run(['avr-nm','-C','-n',elf],capture_output=True,text=True).stdout.split('\n'):
    p=l.split(None,2)
    if len(p)==3 and p[1] in 'tTW': syms.append((int(p[0],16),p[2]))
addrs=[a for a,_ in syms]
agg={}; tot=0
for l in open(pf):
    a,c=map(int,l.split())
    i=bisect.bisect_right(addrs,a)-1
    n=syms[i][1] if i>=0 else '?'
    agg[n]=agg.get(n,0)+c; tot+=c
for n,c in sorted(agg.items(),key=lambda x:-x[1])[:15]: print('%5.1f%% %s'%(100*c/tot,n[:80]))
