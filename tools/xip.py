import struct,sys,os
o=open(sys.argv[1],'rb').read(); X=int(sys.argv[2],0); out=sys.argv[3]
rh=struct.unpack('<I',o[X+0x44:X+0x48])[0]
v=lambda va,n: o[X+va-0x80000000:X+va-0x80000000+n]
f=struct.unpack('<17I2H3I',v(rh,84))
dllfirst,dlllast,physfirst,physlast,nummods=f[:5]; numfiles=f[12]
print(hex(physfirst),hex(physlast),nummods,numfiles)
def cstr(va):
    s=v(va,256); return s[:s.index(b'\0')].decode()
toc=rh+84; mods=[]
for i in range(nummods):
    a,ft,sz,nm,e32,o32,ld=struct.unpack('<IQIIIII',v(toc+32*i,32))
    mods.append((cstr(nm),a,sz,e32,o32,ld))
ft_=toc+32*nummods; files=[]
for i in range(numfiles):
    a,ft,rs,cs,nm,ld=struct.unpack('<IQIIII',v(ft_+28*i,28))
    files.append((cstr(nm),a,rs,cs,ld))
os.makedirs(out,exist_ok=True)
open(os.path.join(out,'_list.txt'),'w').write('\n'.join(f'M {m[0]} size={m[2]:#x} e32={m[3]:#x} o32={m[4]:#x}' for m in mods)+'\n'+'\n'.join(f'F {x[0]} {x[2]:#x}/{x[3]:#x}' for x in files))
import pickle; pickle.dump((mods,files,physfirst),open(os.path.join(out,'_toc.pkl'),'wb'))
