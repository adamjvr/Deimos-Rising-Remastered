#!/usr/bin/env python3
"""Read-only first-pass PEF container/loader probe for Deimos Rising evidence."""
from __future__ import annotations
import argparse, json, struct
from pathlib import Path

def be16(b,o): return struct.unpack_from('>H',b,o)[0]
def be32(b,o): return struct.unpack_from('>I',b,o)[0]
def s32(b,o): return struct.unpack_from('>i',b,o)[0]

def probe(path: Path):
    b=path.read_bytes()
    if b[:8] != b'Joy!peff': raise SystemExit('not a PEF container')
    arch=b[8:12].decode('mac_roman','replace')
    version=be32(b,16); section_count=be16(b,32); inst_count=be16(b,34)
    sections=[]
    for i in range(section_count):
        o=40+i*28
        sections.append({'index':i,'name_offset':s32(b,o),'default_address':be32(b,o+4),
            'total_size':be32(b,o+8),'unpacked_size':be32(b,o+12),'packed_size':be32(b,o+16),
            'container_offset':be32(b,o+20),'section_kind':b[o+24],'share_kind':b[o+25],'alignment':b[o+26]})
    loader=next((s for s in sections if s['section_kind']==4),None)
    result={'path':str(path),'architecture':arch,'format_version':version,
            'section_count':section_count,'instantiated_section_count':inst_count,'sections':sections}
    if loader:
        o=loader['container_offset']
        result['loader_header']={'main_section':s32(b,o),'main_offset':be32(b,o+4),
            'init_section':s32(b,o+8),'init_offset':be32(b,o+12),'term_section':s32(b,o+16),
            'term_offset':be32(b,o+20),'imported_library_count':be32(b,o+24),
            'total_imported_symbol_count':be32(b,o+28),'reloc_section_count':be32(b,o+32),
            'reloc_instr_offset':be32(b,o+36),'loader_strings_offset':be32(b,o+40),
            'export_hash_offset':be32(b,o+44),'export_hash_table_power':be32(b,o+48),
            'exported_symbol_count':be32(b,o+52)}
    return result

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('pef',type=Path); ap.add_argument('-o','--output',type=Path)
    ns=ap.parse_args(); out=json.dumps(probe(ns.pef),indent=2)+'\n'
    ns.output.write_text(out) if ns.output else print(out,end='')
if __name__=='__main__': main()
