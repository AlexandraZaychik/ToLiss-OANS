"""Request a real screenshot through X-Plane's native command, then copy it.

No desktop capture, generated illustration, crop or image editing is involved.
"""
import argparse
import shutil
import socket
import time
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    parser.add_argument('--sim',type=Path,default=Path(r'G:\SteamLibrary\steamapps\common\X-Plane 12'))
    args=parser.parse_args()
    folders=[args.sim/'Output',args.sim/'Output/screenshots']
    def files():
        return {p:p.stat().st_mtime_ns for folder in folders if folder.exists() for p in folder.glob('*.png')}
    before=files()
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
        sock.sendto(b'CMND\0sim/operation/screenshot\0',('127.0.0.1',49000))
    deadline=time.monotonic()+8
    while time.monotonic()<deadline:
        found=[p for p,stamp in files().items() if before.get(p)!=stamp]
        if found:
            newest=max(found,key=lambda p:p.stat().st_mtime_ns)
            # X-Plane creates the file before its PNG encoder finishes. Wait
            # for IEND, otherwise high-resolution captures can be truncated.
            try:
                with newest.open('rb') as capture:
                    capture.seek(-12,2)
                    complete=capture.read()==b'\0\0\0\0IEND\xaeB`\x82'
            except (OSError,ValueError):
                complete=False
            if complete:
                args.output.parent.mkdir(parents=True,exist_ok=True)
                shutil.copy2(newest,args.output)
                print(f'Native X-Plane screenshot: {newest} -> {args.output}')
                return
        time.sleep(.2)
    raise SystemExit('No new native screenshot found; no image was fabricated')


if __name__=='__main__':
    main()
