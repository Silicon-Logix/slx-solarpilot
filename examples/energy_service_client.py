# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Public integration example for the reserved Windows/Linux protocol service.

Reads a remote SunSpec device or inspects an EEBUS peer and its identity.
Pairing requires --trust-ski equal to the independently verified peer SKI.
The application policy and wire implementation remain in the supplied binary.
"""
import argparse, json, queue, subprocess, threading
from pathlib import Path

class EnergyService:
    """One serialized JSON-Lines session; protocol state belongs to the service."""
    def __init__(self, binary, state_dir, port=4712, lan=False):
        flags=[str(binary),'--role','controller','--state-dir',str(state_dir),'--port',str(port)]
        if lan:flags.append('--lan')
        self.process=subprocess.Popen(flags,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                                      text=True,encoding='utf-8',bufsize=1)
        self.lines=queue.Queue()
        def collect():
            for line in self.process.stdout:self.lines.put(line)
            self.lines.put(None)
        threading.Thread(target=collect,daemon=True).start()
        self.identity=self._read()
    def _read(self):
        line=self.lines.get(timeout=12)
        if line is None:raise RuntimeError('Protocol service exited')
        value=json.loads(line)
        if isinstance(value,dict) and 'error' in value:raise RuntimeError(value['error'])
        return value
    def call(self, **request):
        """Call sequentially from the owning application thread."""
        self.process.stdin.write(json.dumps(request,allow_nan=False)+'\n')
        self.process.stdin.flush()
        return self._read()
    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            try:self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:self.process.terminate();self.process.wait(5)
    def __enter__(self):return self
    def __exit__(self,*_):self.close()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--service',type=Path,required=True);p.add_argument('--state-dir',type=Path,default=Path('workspace/energy-service'))
    p.add_argument('--ip',required=True);p.add_argument('--port',type=int,default=4712)
    p.add_argument('--local-port',type=int,default=4713);p.add_argument('--lan',action='store_true')
    p.add_argument('--sunspec',action='store_true');p.add_argument('--unit',type=int,default=1)
    p.add_argument('--trust-ski');args=p.parse_args()
    with EnergyService(args.service.resolve(),args.state_dir,args.local_port,args.lan) as service:
        print(json.dumps(service.identity,indent=2))
        if args.sunspec:
            result=service.call(op='sunspec_scan',host=args.ip,port=args.port,unit=args.unit)
        else:
            result=service.call(op='probe',host=args.ip,port=args.port)
            if args.trust_ski:
                if result['ski'].lower()!=args.trust_ski.lower():raise RuntimeError('Peer SKI differs from explicitly trusted identity')
                result=service.call(op='pair',peer=result)
        print(json.dumps(result,indent=2))
if __name__=='__main__':main()
