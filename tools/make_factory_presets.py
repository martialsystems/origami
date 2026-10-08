#!/usr/bin/env python3
# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
# Writes presets/factory.xml, ORIGAMI's factory bank:  python3 tools/make_factory_presets.py presets/factory.xml
# Each preset lists only what differs from the defaults; the XML gets every parameter (a complete ORIGAMI state).
# Levels are checked by the origami_plugin tests (every preset below 0 dBFS on the test material, LEVEL COMP on).
ids = ["gain","vca_depth","vca_source","wave","emph","sym","stage1","stage2","stage3","mix","level","level_comp","bypass",
       "sym1","sym2","sym3","vc1_amt","vc2_amt","vc3_amt","vc1_sym","vc2_sym","vc3_sym","vc1_src","vc2_src","vc3_src",
       "attack","release","cv_scale","vca_law","quality"]
defaults = dict(gain=0,vca_depth=0,vca_source=0,wave=0,emph=0,sym=0,stage1=0,stage2=0,stage3=0,mix=1,level=0,level_comp=1,bypass=0,
       sym1=0,sym2=0,sym3=0,vc1_amt=0,vc2_amt=0,vc3_amt=0,vc1_sym=0,vc2_sym=0,vc3_sym=0,vc1_src=0,vc2_src=0,vc3_src=0,
       attack=1,release=80,cv_scale=1,vca_law=0,quality=0)
JACK, INPUT, FOLLOW, SIDECHAIN = 0, 1, 2, 3
P = [
 ("INIT", "INIT", {}),
 # RONIN bank: tuned for RONIN's VCO/VCF output (near full scale, bright saw and resonant filter).
 ("Acid Grit", "RONIN", dict(gain=-3, wave=0.45, emph=6, sym=0.15, quality=1)),
 ("Acid Squelch Fold", "RONIN", dict(gain=-3, wave=0.3, emph=4, vc1_src=FOLLOW, vc1_amt=0.6, attack=0.5, release=60)),
 ("Reese Growl", "RONIN", dict(gain=-2, wave=0.55, sym=0.25, emph=2, stage2=0.1, level=-4)),
 ("Wavefolded Bass", "RONIN", dict(gain=-2, wave=0.4, stage2=-0.15, stage3=-0.35, mix=0.8)),
 ("Lead Bite", "RONIN", dict(gain=-3, wave=0.5, emph=6)),
 ("Pluck Edge", "RONIN", dict(wave=0.35, vca_depth=0.5, attack=0.5, release=60, vc1_src=FOLLOW, vc1_amt=0.3)),
 ("Hard Sync-Style Lead", "RONIN", dict(gain=-4, wave=0.7, stage1=0.1, emph=8, quality=1)),
 ("Sub-Safe Bass Drive", "RONIN", dict(wave=0.35, stage3=-0.4, mix=0.6)),
 # SHOGUN bank: tuned for SHOGUN's drum outputs (sharp transients, full-scale hits).
 ("Drum Smash", "SHOGUN", dict(gain=3, wave=0.65, emph=6, mix=0.7, level=-5)),
 ("Kick Fold", "SHOGUN", dict(wave=0.4, vc1_src=FOLLOW, vc1_amt=0.6, attack=0.3, release=30, mix=0.6)),
 ("Hat Sizzle", "SHOGUN", dict(wave=0.55, emph=12, sym=0.1, mix=0.5, quality=1)),
 ("Drum-Bus Glue", "SHOGUN", dict(wave=0.15, sym=0.05, mix=0.8, level=-0.5)),
 ("Snare Crack", "SHOGUN", dict(wave=0.5, emph=8, vc2_src=FOLLOW, vc2_amt=0.4, attack=0.2, release=20, mix=0.6, level=-1.5)),
 ("Transient Fold", "SHOGUN", dict(vca_depth=0.6, attack=0.1, release=50, wave=0.6, mix=0.5)),
 ("Tom Bloom", "SHOGUN", dict(wave=0.35, sym=0.3, vc1_src=FOLLOW, vc1_amt=0.5, release=200, level=-4)),
 ("Parallel Drum Grit", "SHOGUN", dict(wave=0.5, emph=3, mix=0.35)),
 # BUSHIDO bank: VC 1-3 on JACK, depths set for BUSHIDO's 0..5 V CV rows so each step moves the fold musically;
 # unpatched, each is a moderate fold.
 ("Stepped Fold Sequence", "BUSHIDO", dict(wave=0.2, vc1_amt=0.5, vc2_amt=0.4, vc3_amt=0.3)),
 ("Stepped Symmetry", "BUSHIDO", dict(wave=0.4, vc1_sym=0.6, vc2_sym=-0.4)),
 ("Row C Fold Accent", "BUSHIDO", dict(wave=0.25, vc3_amt=0.7)),
 ("Pitch-Tracked Fold", "BUSHIDO", dict(wave=0.3, vc1_amt=0.25, vc2_amt=0.25)),
 ("Fold Arpeggio", "BUSHIDO", dict(wave=0.1, vc1_amt=0.8, vc2_sym=0.4, emph=4)),
 ("Gate-Opened Fold", "BUSHIDO", dict(vca_source=1, vca_depth=0.4, wave=0.5)),
 ("Stepped Bite", "BUSHIDO", dict(wave=0.45, emph=6, vc1_amt=0.4, vc3_sym=0.5)),
 # GENERIC bank: any source.
 ("Warm Bus Glue", "GENERIC", dict(wave=0.12, sym=0.06)),
 ("Analog Edge", "GENERIC", dict(wave=0.22, emph=3, sym=0.1, mix=0.6)),
 ("Even-Order Bloom", "GENERIC", dict(wave=0.35, sym=0.4, mix=0.75)),
 ("Lopsided Fold", "GENERIC", dict(wave=0.45, sym=0.55, sym1=0.2, level=-6)),
 ("Self-Modulated Fold", "GENERIC", dict(wave=0.3, vc1_src=INPUT, vc1_amt=0.4)),
 ("Stereo Shimmer Fold", "GENERIC", dict(wave=0.45, vc2_src=INPUT, vc2_sym=0.5, vc3_src=INPUT, vc3_amt=0.3, quality=1, level=-1.5)),
 ("Sidechain Pump Fold", "GENERIC", dict(wave=0.3, vc1_src=SIDECHAIN, vc1_amt=0.6, vc2_src=SIDECHAIN, vc2_sym=0.3)),
 ("Full Fold", "GENERIC", dict(wave=1.0, stage1=0.2, stage2=0.2, stage3=0.2, quality=1, level=-3.5)),
 ("Shattered", "GENERIC", dict(gain=12, wave=1.0, sym=0.7, emph=12, vc1_src=INPUT, vc1_amt=0.8, vc3_src=INPUT, vc3_sym=-0.6, level=-14, quality=1)),
]
import sys
def fmt(v):
    s = repr(float(v))
    return s[:-2] if s.endswith(".0") else s
out = ['<?xml version="1.0" encoding="UTF-8"?>',
       '<!-- ORIGAMI factory presets. Copyright (c) 2026 Martial Systems LLC. All rights reserved.',
       '     Banks: INIT, then RONIN, SHOGUN, BUSHIDO and GENERIC. Each PRESET holds one complete ORIGAMI state (format 1),',
       '     the same XML the plugin saves. -->',
       '<ORIGAMI_FACTORY version="1">']
names = set()
for name, cat, d in P:
    assert name not in names; names.add(name)
    for k in d: assert k in defaults, k
    v = dict(defaults); v.update(d)
    out.append(f'  <PRESET name="{name}" bank="{cat}">')
    out.append('    <ORIGAMI format="1" unit="ORIGAMI">')
    for i in ids:
        out.append(f'      <PARAM id="{i}" value="{fmt(v[i])}"/>')
    out.append('    </ORIGAMI>')
    out.append('  </PRESET>')
out.append('</ORIGAMI_FACTORY>')
open(sys.argv[1], 'w').write("\n".join(out) + "\n")
print(len(P), "presets")
