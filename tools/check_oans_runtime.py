"""Observe real A320 ND rendering via native X-Plane commands and screenshots.

This checks render output and command dispatch; it does not simulate mouse
clicks and must not be reported as a real mouse-input test.
"""
import json
import argparse
from contextlib import ExitStack
import re
import socket
import statistics
import struct
import subprocess
import sys
import time
from pathlib import Path
from xplane_udp import XPlaneUDP

ROOT=Path(__file__).resolve().parents[1]
PLUGIN=Path(r'G:\SteamLibrary\steamapps\common\X-Plane 12\Aircraft\ToLissA320_V1p3p2\plugins\00_ToLissOANS')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version',default='0.4.2')
    args=parser.parse_args()
    OUT=ROOT/'reports/runtime'/args.version
    OUT.mkdir(parents=True,exist_ok=True)
    with XPlaneUDP() as client, ExitStack() as cleanup:
        def command(name):
            client.command(name);time.sleep(.2)
        def write(name,value):
            allowed={'sim/graphics/view/pilots_head_psi','sim/graphics/view/pilots_head_the',
                     'sim/graphics/view/field_of_view_deg',
                     'AirbusFBW/NDrangeCapt','AirbusFBW/NDrangeFO','AirbusFBW/NDmodeCapt','AirbusFBW/NDmodeFO'}
            assert name in allowed
            client.sock.sendto(b'DREF\0'+struct.pack('<f',value)+name.encode().ljust(500,b'\0'),client.peer)
            time.sleep(.2)
        def capture(name):
            subprocess.run([sys.executable,str(ROOT/'tools/capture_xplane.py'),str(OUT/name)],check=True)
        def report(name):
            command('diagnostics');text=(PLUGIN/'interfaces.txt').read_text(encoding='utf-8')
            (OUT/name).write_text(text,encoding='utf-8');return text
        initial=report('interfaces-before-command-tests.txt')
        assert initial.startswith('ToLissOANS '+args.version+' '), 'Expected requested installed version'
        captain=re.search(r'^view=0 panel=0 range_nm=([\d.]+) follow=1 ',initial,re.M)
        assert captain, 'Start with the captain panel closed and follow enabled'
        ranges=(.2,.5,1.,2.,5.)
        initial_range=float(captain.group(1))
        assert initial_range in ranges, 'Unknown captain map range'
        assert 'ND_state side=1 enabled=0' in initial, 'FO starts disabled'
        fo=client.read(['AirbusFBW/NDrangeFO','AirbusFBW/NDmodeFO'])
        camera=client.read(['sim/graphics/view/field_of_view_deg','sim/graphics/view/pilots_head_the','sim/graphics/view/pilots_head_psi'])
        (OUT/'camera-before-tests.json').write_text(json.dumps(camera,indent=2),encoding='utf-8')
        def restore_camera():
            for name,value in camera.items():write(name,value)
            restored=client.read(camera)
            (OUT/'camera-after-tests.json').write_text(json.dumps(restored,indent=2),encoding='utf-8')
            for name,value in camera.items():
                assert abs(restored[name]-value)<.01, f'Camera restore failed: {name}'
        # Runs on success, test failure, and Ctrl+C, before the UDP socket closes.
        cleanup.callback(restore_camera)
        write('sim/graphics/view/field_of_view_deg',24)
        write('sim/graphics/view/pilots_head_the',-24);write('sim/graphics/view/pilots_head_psi',8)
        write('AirbusFBW/NDrangeCapt',0);write('AirbusFBW/NDmodeCapt',3)
        for _ in range(ranges.index(initial_range)):command('capt_range_in')
        capture('ARC-0.2NM-closed.png')
        command('capt_panel');capture('ARC-0.2NM-panel.png')
        for mode,label in ((2,'NAV'),(4,'PLAN')):
            write('AirbusFBW/NDmodeCapt',mode);capture(label+'-0.2NM-panel.png')
            report(label+'-interfaces.txt')
        write('AirbusFBW/NDmodeCapt',3)
        command('fo_nd');write('AirbusFBW/NDrangeFO',0);write('AirbusFBW/NDmodeFO',3)
        command('fo_range_in');command('fo_range_in')
        write('sim/graphics/view/pilots_head_psi',38);write('sim/graphics/view/pilots_head_the',-21)
        capture('FO-ARC-0.2NM.png');report('FO-interfaces.txt')
        command('fo_nd')
        for name,value in fo.items():write(name,value)
        write('sim/graphics/view/pilots_head_psi',8);write('sim/graphics/view/pilots_head_the',-24)
        def sample():
            values=[]
            for _ in range(15):
                period=client.read(['sim/operation/misc/frame_rate_period'])['sim/operation/misc/frame_rate_period']
                if period>0:values.append(1/period)
                time.sleep(.15)
            return {'samples':values,'median_fps':statistics.median(values),'min_fps':min(values),'max_fps':max(values)}
        fps={'scope':'Stationary same-camera whole-simulator sample, not isolated map CPU/GPU cost','map_on':sample()}
        command('capt_nd')
        try:fps['map_off']=sample()
        finally:command('capt_nd')
        (OUT/'stationary-fps.json').write_text(json.dumps(fps,indent=2),encoding='utf-8')
        final=report('interfaces-after-command-tests.txt')
        assert 'ND_state side=0 enabled=1 blocked=\n' in final
        assert 'experimental_write_brakes' not in final
        assert 'sim/flightmodel2/wing/speedbrake1_deg' not in final
        assert 'AirbusFBW/BrakePedalInput' not in final
        capture('ARC-0.2NM-panel-final.png')
        print('Real native screenshots and command tests complete; mouse input remains separately scoped')
        print('Stationary FPS medians:',fps['map_on']['median_fps'],fps['map_off']['median_fps'])


if __name__=='__main__':
    main()
