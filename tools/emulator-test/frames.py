import re,sys,subprocess
elf=sys.argv[1]; addrs=sys.argv[2:]
dis=subprocess.run(['avr-objdump','-d',elf],capture_output=True,text=True).stdout.split('\n')
cur=None; info={}
for l in dis:
    m=re.match(r'^([0-9a-f]+) <(.*)>:',l)
    if m: cur=m.group(2); info[cur]=[0,0,0]; continue
    if cur is None: continue
    if info[cur][2]>70: continue
    info[cur][2]+=1
    if re.search(r'\tpush\t',l): info[cur][0]+=1
    m=re.search(r'(subi\s+r28, 0x([0-9A-Fa-f]+))|(sbiw\s+r28, 0x([0-9A-Fa-f]+))',l)
    if m and not info[cur][1]: info[cur][1]=int(m.group(2) or m.group(4),16)
tot=0
for a in addrs:
    name=subprocess.run(['avr-addr2line','-f','-e',elf,'0x'+a],capture_output=True,text=True).stdout.split('\n')[0]
    v=info.get(name,[0,0]); tot+=v[0]+v[1]+2
    print(a,name[:55],'push',v[0],'frame',v[1])
print('total',tot)
