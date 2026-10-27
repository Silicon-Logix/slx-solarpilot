# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Exercise real SHIP/TLS/SPINE through the public CLI, including peer loss.

Runs only on the PC. This is an integration test, not an EEBUS certification.
"""
import json
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time


class Node:
    def __init__(self, exe):
        self.p = subprocess.Popen([str(exe), '--json-lines'], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                  text=True, encoding='utf-8')
        self.lines = queue.Queue()
        def collect():
            for line in self.p.stdout:
                self.lines.put(line)
        threading.Thread(target=collect, daemon=True).start()

    def call(self, **q):
        self.p.stdin.write(json.dumps(q) + '\n')
        self.p.stdin.flush()
        r = json.loads(self.lines.get(timeout=15))
        assert 'error' not in r, r
        return r

    def close(self):
        if self.p.poll() is None:
            self.p.stdin.close()
            self.p.wait(timeout=15)


def until(fn, timeout=15):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        value = fn()
        if value:
            return value
        time.sleep(.15)
    raise AssertionError('EEBUS discovery/control deadline exceeded')


def walk(value):
    if isinstance(value, dict):
        yield from value.keys()
        for v in value.values():
            yield from walk(v)
    elif isinstance(value, list):
        for v in value:
            yield from walk(v)


def main():
    exe = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='slx-eebus-') as folder:
        plant, cem = Node(exe), Node(exe)
        try:
            pair = plant.call(op='eebus_start', role='pair', state_dir=folder+'/pair')['link']['identity']
            assert pair['plant']['ski'] != pair['controller']['ski']
            until(lambda: plant.call(op='eebus_status')['entities'] == 12)
            seen = set()
            successful = 0
            after=0
            for _ in range(12):
                s = plant.call(op='advance')
                if s['link']['fresh']:
                    successful += 1
                    assert s['link']['controller_source'] == 'eebus-cem-local'
                    assert s['link']['ack']['status'] == 'executed'
                    assert abs(s['balance_w']) < 1e-6
                for _drain in range(20):
                    batch=plant.call(op='packets',after=after)
                    for p in batch:
                        after=p['id'];seen.update(walk(p['payload']))
                    if len(batch)<16:break
            assert successful >= 10, successful
            assert {'datagram', 'measurementListData', 'loadControlLimitListData'} <= seen, seen
            plant.call(op='reset',scenario=1)
            plant.call(op='configure',patch={'auto':False,'ev_target':.450001,'manual_ev_w':3680.,'manual_battery_w':0.})
            terminal=until(lambda: (s if (s:=plant.call(op='advance'))['link']['fresh'] else None))
            assert abs(terminal['ev_soc']-.450001)<1e-12 and 0<terminal['ev_w']<1380,terminal
            assert terminal['ev_setpoint_w']>=1380 and 0<terminal['ev_active_s']<1
            plant.call(op='reset')
            plant.call(op='configure', patch={'drop_link': True})
            s = plant.call(op='advance')
            assert not s['link']['fresh'] and s['ev_w'] == 0 and s['battery_w'] == 0
            plant.call(op='configure', patch={'drop_link': False})
            # SHIP randomizes retries: 0..3, 3..10, then 10..20 seconds.
            # Allow the documented retry cycle while still requiring real recovery.
            recovery_started=time.monotonic()
            until(lambda: plant.call(op='advance')['link']['fresh'],45)
            recovery_seconds=time.monotonic()-recovery_started
            # The plant starts unpaired. Each peer explicitly trusts the other's
            # certificate identity, as in the future two-host bench.
            a = plant.call(op='eebus_start', role='plant', state_dir=folder+'/physical')['link']['identity']['plant']
            assert not plant.call(op='eebus_status')['connected']
            b = cem.call(op='eebus_start', role='controller', state_dir=folder+'/ems')['link']['identity']['controller']
            plant.call(op='eebus_pair', peer=b)
            cem.call(op='eebus_pair', peer=a)
            cem.call(op='configure', patch={'auto':False,'manual_ev_w':1840.,'manual_battery_w':500.})
            until(lambda: cem.call(op='eebus_status')['entities'] == 12)
            def control():
                plant.call(op='advance')
                cem.call(op='eebus_tick')
                time.sleep(.05)
                s = plant.call(op='advance')
                return s if s['link']['fresh'] else None
            s = until(control)
            assert s['link']['controller_source'] == 'eebus-cem-external'
            assert abs(s['ev_w']-1840) < .01 and abs(s['battery_w']-500) < .01, s
            cem.call(op='configure',patch={'auto':True,'battery_schedule_enabled':True,
                                         'battery_schedule_start_h':12.,'battery_schedule_end_h':13.})
            scheduled=cem.call(op='eebus_tick',sim_s=3600.)
            assert 'programmata' in scheduled['decision']['decision']['battery'],scheduled
            cem.call(op='configure',patch={'auto':False,'battery_schedule_enabled':False})
            until(control)
            time.sleep(3.2)
            assert plant.call(op='eebus_status')['connected']
            s=plant.call(op='advance')
            assert not s['link']['fresh'] and s['ev_w']==0 and s['battery_w']==0
            until(control)
            cem.close()
            time.sleep(.4)
            s = plant.call(op='advance')
            assert not s['link']['fresh'] and s['ev_w']==0 and s['battery_w']==0
            assert s['link']['controller_source']=='eebus-cem-external'
            try:
                plant.call(op='eebus_start',role='plant',port=-1,state_dir=folder+'/invalid')
                raise RuntimeError('Invalid bridge configuration accepted')
            except AssertionError:
                pass
            s=plant.call(op='advance')
            assert not s['link']['fresh'] and s['link']['protocol']=='EEBUS unavailable'
            print(json.dumps({'protocol':'EEBUS SHIP/SPINE','pair_exchanges':successful,
                              'standard_fields':sorted(seen),'separate_processes':True,
                              'fault_recovery':True,'fault_recovery_s':recovery_seconds,'peer_loss_fallback':True}))
        finally:
            cem.close()
            plant.close()


if __name__ == '__main__':
    main()
