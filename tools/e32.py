import struct,sys,os
def load(d):
    h=open(os.path.join(d,'_header'),'rb').read()
    objcnt,flags,entry,vbase,smaj,smin,stack,vsize,s14r,s14s,ts=struct.unpack('<HHIIHHIIIII',h[:36])
    units=[struct.unpack('<II',h[36+8*i:44+8*i]) for i in range(9)]
    subsys=struct.unpack('<H',h[108:110])[0]
    off=len(h)-24*objcnt
    o32=[struct.unpack('<6I',h[off+24*i:off+24*i+24]) for i in range(objcnt)]
    secs=sorted(f for f in os.listdir(d) if f.startswith('S'))
    mem={}
    for (vs,rva,ps,dp,ra,fl),sn in zip(o32,secs):
        mem[rva]=(open(os.path.join(d,sn),'rb').read(),vs,fl)
    return dict(entry=entry,vbase=vbase,vsize=vsize,units=units,o32=o32,mem=mem,hlen=len(h),ts=ts,subsys=subsys,flags=flags)
def rd(m,rva,n):
    for b,(data,vs,fl) in m['mem'].items():
        if b<=rva<b+max(vs,len(data)):
            x=data[rva-b:rva-b+n]; return x+b'\0'*(n-len(x))
    return b'\0'*n
def cstr(m,rva):
    s=rd(m,rva,256); return s[:s.find(b'\0')].decode('latin1')
def exports(m):
    r,sz=m['units'][0]
    if not r: return []
    e=rd(m,r,40); name,base,nf,nn,af,anames,aord=struct.unpack('<IIIIIII',e[12:40])
    out=[]
    for i in range(nn):
        nr=struct.unpack('<I',rd(m,anames+4*i,4))[0]; od=struct.unpack('<H',rd(m,aord+2*i,2))[0]
        out.append((od+base,cstr(m,nr)))
    names={o for o,_ in out}
    return cstr(m,name),out,nf
def imports(m):
    r,sz=m['units'][1]; res=[]
    i=0
    while r:
        ilt,ts,fc,name,iat=struct.unpack('<5I',rd(m,r+20*i,20))
        if not name: break
        dll=cstr(m,name); fns=[]; j=0
        while True:
            t=struct.unpack('<I',rd(m,(ilt or iat)+4*j,4))[0]
            if not t: break
            fns.append(f'#{t&0xffff}' if t&0x80000000 else cstr(m,t+2)); j+=1
        res.append((dll,fns)); i+=1
    return res
if __name__=='__main__':
    for d in sys.argv[1:]:
        m=load(d); print('=====',d,'vbase',hex(m['vbase']),'vsize',hex(m['vsize']),'hdr',m['hlen'],'objs',len(m['o32']))
        ex=exports(m)
        if ex: print('exports',ex[0],ex[2]); [print('  ',o,n) for o,n in ex[1]]
        for dll,f in imports(m): print('import',dll,':',' '.join(f))
