import json, re, os, glob, sys

SRC = r"C:\Users\rober\AppData\Local\Temp\claude\c--Users-rober-Desktop-A\7da8f292-c795-4084-8308-db127ce8b2ac\scratchpad\amxxpkg\styles\editor"
OUT = r"C:\Users\rober\Desktop\ScribaCpp\src\amx_themes.inc"

def strip_comments(t):
    out = []
    i = 0
    n = len(t)
    instr = False
    while i < n:
        c = t[i]
        if instr:
            out.append(c)
            if c == '\\':
                out.append(t[i + 1]); i += 2; continue
            if c == '"': instr = False
            i += 1; continue
        if c == '"':
            instr = True; out.append(c); i += 1; continue
        if t.startswith('//', i):
            while i < n and t[i] != '\n': i += 1
            continue
        if t.startswith('/*', i):
            j = t.find('*/', i + 2)
            i = n if j < 0 else j + 2
            continue
        out.append(c); i += 1
    s = ''.join(out)
    s = re.sub(r',(\s*[}\]])', r'\1', s)
    return s

def load(path):
    return json.loads(strip_comments(open(path, encoding='utf-8-sig').read()))

import colorsys
def split_top(s):
    """split on whitespace at paren depth 0"""
    parts, cur, d = [], '', 0
    for ch in s:
        if ch == '(': d += 1
        if ch == ')': d -= 1
        if ch.isspace() and d == 0:
            if cur: parts.append(cur); cur = ''
        else: cur += ch
    if cur: parts.append(cur)
    return parts

def parse_color(v, vars_, bg=None):
    """-> (r,g,b,a) or None"""
    if not isinstance(v, str): return None
    v = v.strip()
    m = re.fullmatch(r'var\((.+)\)', v)
    if m:
        return parse_color(vars_.get(m.group(1)), vars_, bg)
    m = re.fullmatch(r'hsla?\(\s*([\d.]+)\s*,\s*([\d.]+)%\s*,\s*([\d.]+)%\s*\)', v)
    if m:
        h, s_, l_ = float(m.group(1)) / 360, float(m.group(2)) / 100, float(m.group(3)) / 100
        r, g, b = colorsys.hls_to_rgb(h, l_, s_)
        return (round(r * 255), round(g * 255), round(b * 255), 255)
    if v.startswith('color(') and v.endswith(')'):
        parts = split_top(v[6:-1])
        base = parse_color(parts[0], vars_, bg)
        if base is None: return None
        r, g, b, a = base
        for mod in parts[1:]:
            mm = re.fullmatch(r'(\w+)\((.*)\)', mod)
            if not mm: continue
            fn, arg = mm.group(1), mm.group(2).strip()
            if fn == 'a':
                a = round(255 * float(arg.rstrip('%')) / 100)
            elif fn in ('l', 's'):
                hh, ll, ss = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
                rel = arg.startswith('+') or arg.startswith('-')
                val = float(arg.replace(' ', '').rstrip('%')) / 100
                if fn == 'l': ll = ll + val if rel else val
                else: ss = ss + val if rel else val
                rr, gg, bb = colorsys.hls_to_rgb(hh, min(1, max(0, ll)), min(1, max(0, ss)))
                r, g, b = round(rr * 255), round(gg * 255), round(bb * 255)
            elif fn == 'blend':
                args = split_top(arg)
                other = parse_color(args[0], vars_, bg)
                pct = float(args[1].rstrip('%')) / 100
                if other:
                    r, g, b = (round(r + (other[0] - r) * pct), round(g + (other[1] - g) * pct), round(b + (other[2] - b) * pct))
        return (r, g, b, a)
    if not v.startswith('#'): return None
    h = v[1:]
    if len(h) == 3: h = ''.join(c * 2 for c in h) + 'ff'
    elif len(h) == 4: h = ''.join(c * 2 for c in h)
    elif len(h) == 6: h += 'ff'
    elif len(h) != 8: return None
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4, 6))

def blend(fg, bg):
    if fg is None: return None
    a = fg[3] / 255.0
    return tuple(round(fg[i] * a + bg[i] * (1 - a)) for i in range(3))

def hexs(c): return '0x%02x%02x%02x' % tuple(c)
def mix(a, b, t): return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))
def lum(c): return (0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2]) / 255.0

KINDS = ['PRE', 'INCPATH', 'PPCONST', 'PPGROUP', 'CONTROL', 'OP', 'BRACKET', 'PAREN', 'BRACE', 'COMMA', 'FNMOD', 'VARTYPE', 'TAG', 'ESC', 'CONSTVAR', 'FNDEF', 'FNCALL', 'BOOL', 'FLOAT', 'INVALID']
STACKS = {
    0: [],
    2: ['string.quoted.double.pawn'],
    3: ['constant.numeric.int.pawn'],
    4: ['comment.line.pawn'],
    'PRE': ['keyword.control.import', 'meta.preprocessor'],
    'INCPATH': ['keyword.control.import', 'meta.preprocessor.include.path.pawn'],
    'PPCONST': ['entity.name.constant.preprocessor.pawn'],
    'PPGROUP': ['storage.modifier', 'meta.preprocessor.group'],
    'CONTROL': ['keyword.control.pawn'],
    'OP': ['keyword.operator.assignment.pawn'],
    'BRACKET': ['meta.brackets', 'punctuation.section.brackets'],
    'PAREN': ['punctuation.section.group.begin'],
    'BRACE': ['punctuation.section.block.begin'],
    'COMMA': ['punctuation.separator.pawn'],
    'FNMOD': ['storage.modifier.function.pawn'],
    'VARTYPE': ['storage.type.vars.pawn'],
    'TAG': ['storage.modifier.tag'],
    'ESC': ['string.quoted.double.pawn', 'constant.character.escape.pawn'],
    'CONSTVAR': ['constant.vars.pawn'],
    'FNDEF': ['support.function.pawn'],
    'FNCALL': ['variable.function.pawn'],
    'BOOL': ['constant.language'],
    'FLOAT': ['constant.numeric.float.pawn'],
    'INVALID': ['invalid.illegal'],
}

def atom_match(atom, scope):
    return scope == atom or scope.startswith(atom + '.')

def score(selector, stack):
    atoms = selector.split()
    if not atoms: return None
    pos = len(stack) - 1
    sc = []
    for atom in reversed(atoms):
        found = -1
        while pos >= 0:
            if atom_match(atom, stack[pos]): found = pos; break
            pos -= 1
        if found < 0: return None
        sc.append((found + 1, atom.count('.') + 1))
        pos = found - 1
    return tuple(sc)

def resolve(rules, stack):
    best = {'foreground': None, 'background': None, 'font_style': None}
    bscore = {k: None for k in best}
    for idx, r in enumerate(rules):
        sel = r.get('scope', '')
        s = None
        for one in sel.split(','):
            sc = score(one.strip(), stack)
            if sc is not None and (s is None or sc > s): s = sc
        if s is None: continue
        key = (s, idx)
        for prop in best:
            if prop in r and (bscore[prop] is None or key >= bscore[prop]):
                best[prop] = r[prop]; bscore[prop] = key
    return best

def main():
    files = sorted(glob.glob(os.path.join(SRC, '*.sublime-color-scheme')))
    rows, pals, names = [], [], []
    def emit(name, bg, fg, caret, sel, cols, sts, invbg):
        dark = lum(bg) < 0.5
        win = mix(bg, (0, 0, 0), 0.25) if dark else mix(bg, (0, 0, 0), 0.07)
        accent = cols['FNCALL']
        sidebar = mix(bg, win, .6)
        menu = mix(bg, win, .5)
        black, white = (0, 0, 0), (255, 255, 255)
        T = [
            win, bg, mix(bg, fg, .5), mix(bg, fg, .82), fg, win, fg, mix(win, fg, .1), menu, mix(menu, fg, .13), mix(menu, fg, .1),
            mix(bg, fg, .2) if dark else mix(bg, black, .12), mix(bg, fg, .72), fg, win, mix(bg, fg, .62), caret, sel,
            mix(bg, win, .5), mix(bg, fg, .12), mix(win, black, .25) if dark else mix(bg, white, .7), mix(win, black, .45) if dark else mix(bg, black, .2),
            accent, sidebar, mix(bg, fg, .74), mix(sidebar, fg, .1), mix(bg, fg, .5), mix(bg, fg, .25), mix(bg, fg, .4), mix(bg, fg, .22),
            accent, mix(bg, fg, .14), mix(bg, fg, .24), mix(bg, fg, .35), mix(bg, fg, .5),
        ]
        syn = [fg, cols['CONTROL'], cols[2], cols[3], cols[4], cols['FNCALL'], cols['VARTYPE'], cols['TAG'], cols['FNDEF'], cols['CONSTVAR'], cols['OP'], cols['FNCALL'], cols['INVALID']]
        names.append(name)
        row = '    {L"%s", %s, %s,\n     {%s}},\n' % (
            name, 'true' if dark else 'false', ', '.join(hexs(c) for c in T), ', '.join(hexs(c) for c in syn))
        rows.append(row)
        order = [0, 2, 3, 4] + KINDS
        # col[AK_N] indexed by kind number: 0 default,1 unused,2 str,3 num,4 comment,5..12 unused,13.. AK
        arr = [None] * 33
        sarr = [0] * 33
        for k in range(13): arr[k] = fg
        arr[0] = fg; arr[2] = cols[2]; arr[3] = cols[3]; arr[4] = cols[4]
        sarr[2], sarr[3], sarr[4] = sts[2], sts[3], sts[4]
        for i, k in enumerate(KINDS):
            arr[13 + i] = cols[k]; sarr[13 + i] = sts[k]
        pals.append('    {{%s}, {%s}, %s},  // %s\n' % (', '.join(hexs(c) for c in arr), ', '.join(str(x) for x in sarr), hexs(invbg) if invbg else '0xffffffff', name))

    for pi, f in enumerate(files):
        base = os.path.basename(f).replace('.pawn-editor.sublime-color-scheme', '')
        sc = load(f)
        vars_ = sc.get('variables', {})
        g = sc['globals']
        bg = parse_color(g['background'], vars_)[:3]
        fg = parse_color(g['foreground'], vars_)[:3]
        caret = (parse_color(g.get('caret'), vars_) or (fg + (255,)))[:3]
        sel_raw = parse_color(g.get('selection'), vars_)
        sel = blend(sel_raw, bg) if sel_raw else mix(bg, fg, 0.2)
        gutter = (parse_color(g.get('gutter'), vars_) or (bg + (255,)))[:3]
        gfg = (parse_color(g.get('gutter_foreground'), vars_) or (mix(bg, fg, .5) + (255,)))[:3]
        lh_raw = parse_color(g.get('line_highlight'), vars_)
        lhl = blend(lh_raw, bg) if lh_raw else mix(bg, fg, 0.06)
        rules = sc['rules']
        # resolved colours per kind
        cols, sts = {}, {}
        invbg = None
        for key, stack in STACKS.items():
            r = resolve(rules, stack)
            c = parse_color(r['foreground'], vars_)
            c = blend(c, bg) if c else fg
            cols[key] = c
            fs = (r['font_style'] or '')
            sts[key] = (1 if 'bold' in fs else 0) | (2 if 'italic' in fs else 0)
            if key == 'INVALID':
                b = parse_color(r['background'], vars_)
                invbg = blend(b, bg) if b else None
        emit('AMXX: ' + base.replace('(mirage)', 'Mirage'), bg, fg, caret, sel, cols, sts, invbg)
        if base == 'Dark': dark = (bg, fg, caret, sel, cols, sts, invbg)

    # ---- variants of the AMXX Dark scheme: other backgrounds and softer or stronger colours, every colour kept readable on its background
    import colorsys
    def lumi(c):
        f = lambda v: (v / 255.0 / 12.92) if v / 255.0 <= 0.03928 else (((v / 255.0) + 0.055) / 1.055) ** 2.4
        return 0.2126 * f(c[0]) + 0.7152 * f(c[1]) + 0.0722 * f(c[2])
    def contrast(a, b):
        la, lb = lumi(a), lumi(b)
        hi, lo = max(la, lb), min(la, lb)
        return (hi + 0.05) / (lo + 0.05)
    def tune(c, bg, sat=1.0, dl=0.0, need=4.2):
        h, l, sa = colorsys.rgb_to_hls(c[0] / 255, c[1] / 255, c[2] / 255)
        sa = min(1.0, sa * sat)
        l = min(0.95, max(0.05, l + dl))
        def rgb(l):
            r, g, b = colorsys.hls_to_rgb(h, l, sa)
            return (round(r * 255), round(g * 255), round(b * 255))
        out = rgb(l)
        while contrast(out, bg) < need and l < 0.95:
            l += 0.02
            out = rgb(l)
        return out
    dbg, dfg, dcaret, dsel, dcols, dsts, dinv = dark
    VARIANTS = [
        # name, background, text, saturation of the colours, lightness added
        ('Dark Soft', (0x1b, 0x1c, 0x1f), (0xc9, 0xcc, 0xd1), 0.72, 0.04),
        ('Dark Black', (0x00, 0x00, 0x00), (0xe6, 0xe6, 0xe6), 1.1, 0.06),
        ('Dark Ocean', (0x0b, 0x12, 0x20), (0xcb, 0xd5, 0xe1), 0.95, 0.03),
        ('Dark Warm', (0x17, 0x12, 0x0e), (0xd6, 0xcc, 0xc2), 0.9, 0.03),
        ('Dark Green', (0x0a, 0x13, 0x0c), (0xc5, 0xd6, 0xc5), 0.9, 0.03),
        ('Dark Slate', (0x1e, 0x29, 0x3b), (0xcb, 0xd5, 0xe1), 0.85, 0.05),
    ]
    for vname, vbg, vfg, vsat, vdl in VARIANTS:
        vcols = {k: tune(c, vbg, vsat, vdl) for k, c in dcols.items()}
        vcols[0] = vfg
        vcaret = vfg
        vsel = mix(vbg, vcols['FNCALL'], 0.28)
        vinv = mix(vbg, (0xff, 0x2d, 0x56), 0.6)
        emit('AMXX: ' + vname, vbg, vfg, vcaret, vsel, vcols, dsts, vinv)
    open(OUT, 'w', encoding='utf-8').write('// generated by genamx.py from the AMXX-Editor colour schemes: complete colour schemes (included inside THEMES) \n' + ''.join(rows))
    pal_out = OUT.replace('amx_themes.inc', 'amx_pals.inc')
    open(pal_out, 'w', encoding='utf-8').write('// generated by genamx.py: colour and font style of every syntax kind (13 + AK_*) of each AMXX colour scheme\n' + ''.join(pals))
    print(len(files), 'schemes'); print('\n'.join(names))

main()
