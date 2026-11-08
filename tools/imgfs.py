import struct,sys,os
from dissect.util.compression import lzxpress
o=open(sys.argv[1],'rb').read(); B=int(sys.argv[2],0); out=sys.argv[3]
I=lambda off: o[B+off:]
def ent(p): return o[B+p:B+p+52]
def read_stream(size,loc,isz):
    n=(size+0xfff)//0x1000; t=o[B+loc:B+loc+8*n]; r=b''
    for i in range(n):
        c,u,l=struct.unpack('<HHI',t[8*i:8*i+8])
        if c==0 and u==0: r+=b'\0'*min(0x1000,size-len(r)); continue
        raw=o[B+l:B+l+c]
        r+= raw if c==u else lzxpress.decompress(raw)
    return r[:size]
def getname(e):
    nl,fl=struct.unpack('<HH',e[12:16])
    if nl<=4: return e[16:24].decode('utf-16le')[:nl]
    p=struct.unpack('<I',e[20:24])[0]
    raw=o[B+p+4:B+p+4+nl*2] if fl&2 else o[B+p:B+p+nl*2]
    return raw.decode('utf-16le','replace')
files=[]
mag=struct.pack('<I',0x2F5314CE)
for blk in range(0x800, len(o)-B, 0x800):
    if o[B+blk:B+blk+4]!=mag: continue
    for i in range(39):
        p=blk+8+52*i
        if ent(p)[:4] in (b'\xfe\xfe\xff\xff',b'\xfe\xf6\xff\xff'): files.append(p)
os.makedirs(out,exist_ok=True); long=0
for p in files:
    e=ent(p); name=getname(e); nl=struct.unpack('<H',e[12:14])[0]
    if nl>24: long+=1
    hsz,attr=struct.unpack('<II',e[24:32]); iloc,isz=struct.unpack('<II',e[44:52])
    sec=struct.unpack('<I',e[8:12])[0]
    if e[:4]==b'\xfe\xfe\xff\xff':
        d=os.path.join(out,name); os.makedirs(d,exist_ok=True)
        open(os.path.join(d,'_header'),'wb').write(read_stream(hsz,iloc,isz))
        while sec:
            s=ent(sec); sn=getname(s); ssz,sl,ssi=struct.unpack('<III',s[24:36])
            open(os.path.join(d,sn),'wb').write(read_stream(ssz,sl,ssi))
            sec=struct.unpack('<I',s[8:12])[0]
    else:
        open(os.path.join(out,name),'wb').write(read_stream(hsz,iloc,isz))
print(len(files),'files, long names',long)
