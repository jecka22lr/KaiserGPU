import pefile,sys,struct
from capstone import *
def load(path):
    p=pefile.PE(path); base=p.OPTIONAL_HEADER.ImageBase
    imp={}
    for i in getattr(p,'DIRECTORY_ENTRY_IMPORT',[]):
        for x in i.imports: imp[x.address]=f"{i.dll.decode()}!{(x.name or b'#%d'%x.ordinal).decode()}"
    ex={e.address+base:e.name.decode() for e in getattr(p,'DIRECTORY_ENTRY_EXPORT',type('',(),{'symbols':[]})).symbols if e.name}
    return p,base,imp,ex
def dis(path,name_or_va,n=80):
    p,base,imp,ex=load(path)
    va=name_or_va if isinstance(name_or_va,int) else [v for v,k in ex.items() if k==name_or_va][0]
    code=p.get_data(va-base,n*4); md=Cs(CS_ARCH_ARM,CS_MODE_ARM)
    for ins in md.disasm(code,va):
        c=''
        if ins.mnemonic=='ldr' and '[pc' in ins.op_str:
            try:
                off=int(ins.op_str.split('#')[-1].rstrip(']'),0); lit=ins.address+8+off
                v=struct.unpack('<I',p.get_data(lit-base,4))[0]
                c=f'  ; ={v:#x} {imp.get(v,"")}{ex.get(v,"")}'
                try:
                    s=p.get_string_at_rva(v-base,64)
                    if s and len(s)>3 and all(32<=ch<127 for ch in s): c+=f' "{s.decode()}"'
                except Exception: pass
            except Exception: pass
        if ins.mnemonic in('bl','b','blx') and ins.op_str.startswith('#'):
            t=int(ins.op_str[1:],0); c+=f'  ; {ex.get(t,"")}'
        print(f'{ins.address:08x}: {ins.mnemonic:8s}{ins.op_str}{c}')
        if ins.mnemonic in ('ldm','pop') and 'pc' in ins.op_str and n<0: break
if __name__=='__main__':
    a=sys.argv[2]; dis(sys.argv[1], int(a,0) if a.startswith('0x') else a, int(sys.argv[3]) if len(sys.argv)>3 else 60)
