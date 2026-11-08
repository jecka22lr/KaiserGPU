"""
Minimal LZX decoder (Microsoft LZX as used by CAB / Windows CE imgfs "LZX").
Decodes one independently-compressed chunk: decode(data, out_len, window_bits).
"""

EXTRA = []
BASE = []
_b = 0
for i in range(51):
    e = min(i // 2 - 1 if i >= 4 else 0, 17)
    EXTRA.append(e)
    BASE.append(_b)
    _b += 1 << e


class Bits:
    def __init__(self, data):
        self.d = data
        self.p = 0          # byte position
        self.buf = 0        # bit buffer
        self.n = 0          # bits in buffer

    def _fill(self):
        while self.n <= 16:
            if self.p + 1 < len(self.d):
                w = self.d[self.p] | (self.d[self.p + 1] << 8)
            elif self.p < len(self.d):
                w = self.d[self.p]
            else:
                w = 0
            self.p += 2
            self.buf |= w << (16 - self.n)
            self.n += 16

    def peek(self, k):
        self._fill()
        return (self.buf >> (32 - k)) & ((1 << k) - 1)

    def drop(self, k):
        self.buf = (self.buf << k) & 0xffffffff
        self.n -= k

    def read(self, k):
        if k == 0:
            return 0
        if k > 16:
            hi = self.read(16)
            return (hi << (k - 16)) | self.read(k - 16)
        v = self.peek(k)
        self.drop(k)
        return v

    def align16(self):
        # discard bits up to the next 16-bit boundary of the input stream
        r = self.n % 16
        if r:
            self.drop(r)


class Huff:
    def __init__(self, lengths):
        self.max = max(lengths) if lengths and max(lengths) else 0
        self.table = {}
        code = 0
        for L in range(1, 17):
            for sym, l in enumerate(lengths):
                if l == L:
                    self.table[(L, code)] = sym
                    code += 1
            code <<= 1

    def decode(self, bits):
        if self.max == 0:
            raise ValueError('empty huffman tree used')
        v = bits.peek(16)
        for L in range(1, self.max + 1):
            s = self.table.get((L, v >> (16 - L)))
            if s is not None:
                bits.drop(L)
                return s
        raise ValueError('bad huffman code')


def _read_lengths(bits, lens, first, last):
    pre = Huff([bits.read(4) for _ in range(20)])
    i = first
    while i < last:
        z = pre.decode(bits)
        if z == 17:
            n = bits.read(4) + 4
            for _ in range(n): lens[i] = 0; i += 1
        elif z == 18:
            n = bits.read(5) + 20
            for _ in range(n): lens[i] = 0; i += 1
        elif z == 19:
            n = bits.read(1) + 4
            z = pre.decode(bits)
            v = (lens[i] - z + 17) % 17
            for _ in range(n): lens[i] = v; i += 1
        else:
            lens[i] = (lens[i] - z + 17) % 17
            i += 1


def decode(data, out_len, window_bits=16):
    slots = {15: 30, 16: 32, 17: 34, 18: 36, 19: 38, 20: 42, 21: 50}[window_bits]
    bits = Bits(data)
    out = bytearray()
    R = [1, 1, 1]
    main_lens = [0] * (256 + 8 * slots)
    len_lens = [0] * 249
    if bits.read(1):           # E8 call translation header
        e8 = bits.read(32)
    else:
        e8 = 0
    while len(out) < out_len:
        btype = bits.read(3)
        bsize = (bits.read(16) << 8) | bits.read(8)
        aligned = None
        if btype == 2:
            aligned = Huff([bits.read(3) for _ in range(8)])
        if btype in (1, 2):
            _read_lengths(bits, main_lens, 0, 256)
            _read_lengths(bits, main_lens, 256, 256 + 8 * slots)
            main = Huff(main_lens)
            _read_lengths(bits, len_lens, 0, 249)
            lent = Huff(len_lens) if max(len_lens) else None
            end = len(out) + bsize
            while len(out) < end:
                s = main.decode(bits)
                if s < 256:
                    out.append(s)
                    continue
                s -= 256
                ml = s & 7
                if ml == 7:
                    ml += lent.decode(bits)
                ml += 2
                slot = s >> 3
                if slot == 0:
                    off = R[0]
                elif slot == 1:
                    off = R[1]; R[1] = R[0]; R[0] = off
                elif slot == 2:
                    off = R[2]; R[2] = R[0]; R[0] = off
                else:
                    eb = EXTRA[slot]
                    if btype == 2 and eb >= 3:
                        v = bits.read(eb - 3) << 3
                        v += aligned.decode(bits)
                    elif btype == 2 and eb > 0:
                        v = bits.read(eb)
                    else:
                        v = bits.read(eb)
                    off = BASE[slot] + v - 2
                    R[2] = R[1]; R[1] = R[0]; R[0] = off
                for _ in range(ml):
                    out.append(out[-off])
        elif btype == 3:
            bits.align16()
            # the bit buffer may still hold whole 16-bit words: rewind them
            bits.p -= bits.n // 8
            bits.n = 0; bits.buf = 0
            p = bits.p
            R = list(int.from_bytes(data[p + 4 * k:p + 4 * k + 4], 'little') for k in range(3))
            p += 12
            out += data[p:p + bsize]
            p += bsize
            if bsize & 1:
                p += 1
            bits.p = p
        else:
            raise ValueError('bad block type %d' % btype)
    out = out[:out_len]
    if e8 and out_len > 10:
        i = 0
        while i < out_len - 10:
            if out[i] == 0xE8:
                a = int.from_bytes(out[i + 1:i + 5], 'little', signed=True)
                if -i <= a < e8:
                    r = a - i if a >= 0 else a + e8
                    out[i + 1:i + 5] = (r & 0xffffffff).to_bytes(4, 'little')
                i += 5
            else:
                i += 1
    return bytes(out)
