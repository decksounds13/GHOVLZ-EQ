import math, cmath

kPi = math.pi

def addmul(c, v, c1):
    return complex(c.real + v * c1.real, c.imag + v * c1.imag)

def stab(z):
    m = abs(z)
    return z * (0.9995 / max(m, 1e-12)) if m >= 1 else z

def design(n, gdb):
    n2 = n * 2
    g = abs(10 ** (gdb / 20)) ** (1 / n2)
    gp = -1 / max(g, 1e-12)
    gz = -g
    out = []
    for i in range(1, n // 2 + 1):
        th = kPi * (0.5 - (2 * i - 1) / n2)
        out.append((cmath.rect(gp, th), cmath.rect(gz, th)))
    return out

def makebp(fc, fw):
    half = 0.5 * fw
    if fc - half < 0.0015:
        half = fc - 0.0015
    if fc + half > 0.485:
        half = 0.485 - fc
    if half < 0.0005:
        return None
    fw = 2 * half
    ww = 2 * kPi * fw
    wc2 = 2 * kPi * fc - ww * 0.5
    wc = wc2 + ww
    wc2 = max(wc2, 1e-6)
    wc = min(wc, kPi - 1e-6)
    ch = math.cos((wc - wc2) * 0.5)
    a = math.cos((wc + wc2) * 0.5) / ch
    b = 1 / math.tan((wc - wc2) * 0.5)
    return dict(a=a, b=b, a2=a * a, b2=b * b, ab=a * b, ab2=2 * a * b)

def xform(bp, s, ispole):
    c = (1 + s) / (1 - s)
    v = 0j
    v = addmul(v, 4 * (bp["b2"] * (bp["a2"] - 1) + 1), c)
    v += 8 * (bp["b2"] * (bp["a2"] - 1) - 1)
    v *= c
    v += 4 * (bp["b2"] * (bp["a2"] - 1) + 1)
    v = cmath.sqrt(v)
    u = -v
    u = addmul(u, bp["ab2"], c) + bp["ab2"]
    v = addmul(v, bp["ab2"], c) + bp["ab2"]
    d = addmul(0j, 2 * (bp["b"] - 1), c) + 2 * (1 + bp["b"])
    o0, o1 = u / d, v / d
    if ispole:
        o0, o1 = stab(o0), stab(o1)
    return o0, o1

def stages_for(fs, f0, q, gdb, ordr, bw_map="pow"):
    if bw_map == "pow":
        bw = max(0.25, min(3.5, 1.05 / (q ** 0.55)))
    else:
        # gentler: keep more plateau at high Q
        bw = max(0.40, min(3.2, 1.25 / (q ** 0.45)))
    wh = f0 * (2 ** (0.5 * bw) - 2 ** (-0.5 * bw))
    wh = max(f0 * 0.04, min(f0 * 3.5, wh))
    fc = f0 / fs
    fw = wh / fs
    bp = makebp(fc, fw)
    if not bp:
        return None, bw
    stages = []
    for p, z in design(ordr, gdb):
        p0, p1 = xform(bp, p, True)
        z0, z1 = xform(bp, z, False)
        for P, Z in ((p0, z0), (p1, z1)):
            stages.append((1.0, -2 * Z.real, abs(Z) ** 2, 1.0, -2 * P.real, abs(P) ** 2))

    def mag_lin(f):
        w = 2 * kPi * f / fs
        zm1 = cmath.exp(-1j * w)
        zm2 = zm1 * zm1
        H = 1 + 0j
        for b0, b1, b2, a0, a1, a2 in stages:
            H *= (b0 + b1 * zm1 + b2 * zm2) / (a0 + a1 * zm1 + a2 * zm2)
        return abs(H)

    oob = fs * 0.5 * 0.999 if fc < 0.25 else 0.0
    sc = 1.0 / mag_lin(oob)
    b0, b1, b2, a0, a1, a2 = stages[0]
    stages[0] = (b0 * sc, b1 * sc, b2 * sc, a0, a1, a2)
    return stages, bw

def mag(stages, fs, f):
    w = 2 * kPi * f / fs
    zm1 = cmath.exp(-1j * w)
    zm2 = zm1 * zm1
    H = 1 + 0j
    for b0, b1, b2, a0, a1, a2 in stages:
        H *= (b0 + b1 * zm1 + b2 * zm2) / (a0 + a1 * zm1 + a2 * zm2)
    return 20 * math.log10(max(1e-15, abs(H)))

fs = 48000
f0 = 1000
gdb = -12
print("order map  Q   bwOct  centre  midL   midR   edgeL  edgeR  farL   farR  flatErr")
for ordr in (6, 8):
    for bmap in ("pow", "gentle"):
        for q in (0.3, 0.5, 0.7, 1.0, 2.0, 4.0, 8.0, 10.0):
            st, bw = stages_for(fs, f0, q, gdb, ordr, bmap)
            if st is None:
                print(f"{ordr:5d} {bmap:6s} {q:4.1f} FAIL")
                continue
            r = 2 ** (0.5 * bw)
            # mid of each half-band (geometric)
            mids = (f0 / (r ** 0.5), f0 * (r ** 0.5))
            edges = (f0 / r, f0 * r)
            fars = (max(20, f0 / r / 4), min(fs * 0.45, f0 * r * 4))
            def m(f):
                return mag(st, fs, f)
            c = m(f0)
            ml, mr = m(mids[0]), m(mids[1])
            el, er = m(edges[0]), m(edges[1])
            fl, fr = m(fars[0]), m(fars[1])
            # flatness: max deviation of mid points from centre
            flat = max(abs(ml - c), abs(mr - c))
            print(
                f"{ordr:5d} {bmap:6s} {q:4.1f} {bw:6.2f} {c:+7.2f} {ml:+7.2f} {mr:+7.2f} "
                f"{el:+7.2f} {er:+7.2f} {fl:+7.2f} {fr:+7.2f} {flat:7.2f}"
            )
