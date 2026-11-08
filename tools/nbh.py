import struct,sys,os
src,out=sys.argv[1],sys.argv[2]
d=open(src,'rb').read(); p=0x17; inner=bytearray(); n=0
while p<len(d):
    bl,sl,fl=struct.unpack('<IIB',d[p:p+9]); p+=9
    inner+=d[p:p+bl]; p+=bl+sl; n+=1
    if fl==2: break
print('blocks',n,'end',p,'filelen',len(d),'inner',len(inner))
h=inner
dev=h[0x20:0x40].split(b'\0')[0]
types=struct.unpack('<32I',h[0x40:0xC0]);offs=struct.unpack('<32I',h[0xC0:0x140]);lens=struct.unpack('<32I',h[0x140:0x1C0])
print(dev,h[0x1C0:0x1E0].split(b'\0')[0],h[0x1E0:0x1F0].split(b'\0')[0])
os.makedirs(out,exist_ok=True)
for t,o,l in zip(types,offs,lens):
    if l: print(hex(t),hex(o),hex(l)); open(f'{out}/sec_{t:03x}.bin','wb').write(h[o:o+l])
