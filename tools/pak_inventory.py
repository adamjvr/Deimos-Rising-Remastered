#!/usr/bin/env python3
"""Read-only inventory of Deimos Rising ZIP-format .pak files."""
from __future__ import annotations
import argparse, collections, hashlib, json, zipfile
from pathlib import Path

def sha256(path: Path) -> str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''): h.update(block)
    return h.hexdigest()

def inventory(path: Path) -> dict:
    members=[]
    with zipfile.ZipFile(path) as z:
        for i in z.infolist():
            if i.is_dir(): continue
            members.append({'path':i.filename,'size':i.file_size,'compressed':i.compress_size,
                            'crc32':f'{i.CRC:08x}','method':i.compress_type})
    ext=collections.Counter((Path(m['path']).suffix.lower() or '<none>') for m in members)
    return {'path':str(path),'size_bytes':path.stat().st_size,'sha256':sha256(path),
            'member_files':len(members),'unpacked_bytes':sum(m['size'] for m in members),
            'extensions':dict(sorted(ext.items())),'members':members}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('paks',nargs='+',type=Path); ap.add_argument('-o','--output',type=Path)
    ns=ap.parse_args(); result={p.name:inventory(p) for p in ns.paks}; text=json.dumps(result,indent=2)+'\n'
    ns.output.write_text(text) if ns.output else print(text,end='')
if __name__=='__main__': main()
