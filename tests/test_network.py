# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Real TCP interoperability, basic control, rejection and non-SLX EEBUS discovery.
Optional --pysunspec adds an independent SunSpec Alliance client implementation.
No certification or commercial-device qualification is implied by this test.
"""
import argparse, json, socket, struct, sys, tempfile, time
from pathlib import Path
from test_eebus import Node, until
from sunspec_fixture import Fixture

def rpc(node, **q):
    return node.call(op='network_request',request=q)['network']
def adu(port, unit, pdu, fragmented=False):
    raw=struct.pack('>HHHB',77,0,len(pdu)+1,unit)+pdu
    with socket.create_connection(('127.0.0.1',port),3) as s:
        if fragmented:
            for b in raw:s.sendall(bytes([b]))
        else:s.sendall(raw)
        def exact(n):
            out=b''
            while len(out)<n:
                chunk=s.recv(n-len(out));assert chunk,'truncated response';out+=chunk
            return out
        h=exact(7);assert h[:4]==raw[:4] and h[6]==unit
        return exact(struct.unpack('>H',h[4:6])[0]-1)
def write(port,unit,address,value):
    pdu=struct.pack('>BHH',6,address,value & 65535)
    assert adu(port,unit,pdu)==pdu

def main():
    p=argparse.ArgumentParser();p.add_argument('exe',type=Path);p.add_argument('--pysunspec',type=Path);p.add_argument('--report',type=Path);args=p.parse_args()
    checks=[]
    with tempfile.TemporaryDirectory(prefix='slx-network-') as folder:
        plant,cem=Node(args.exe.resolve()),Node(args.exe.resolve())
        try:
            a=plant.call(op='eebus_start',role='plant',profile='standard',device='pv-inverter',lan=True,state_dir=folder+'/plant')['link']['identity']['plant']
            b=cem.call(op='eebus_start',role='controller',lan=True,state_dir=folder+'/cem')['link']['identity']['controller']
            info=rpc(cem,op='probe',host='127.0.0.1',port=a['port'])['probe']
            assert info['ski']==a['ski'] and info['fingerprint']==a['fingerprint'] and not info['verified']
            checks.append('IP TLS identity probe without implicit trust')
            plant.call(op='eebus_pair',peer=b)
            # Trust the probed certificate; SHIP ID is intentionally unknown.
            cem.call(op='eebus_pair',peer=info)
            def discovered():
                ds=rpc(cem,op='devices')['devices']
                return ds if len(ds)==2 and any(any(f['type']=='LoadControl' and f['role']=='server' for f in d['features']) for d in ds) else None
            devices=until(discovered,20)
            assert all('SLX-SolarPilot-plant' not in str(d['address']) for d in devices)
            pv=next(d for d in devices if any(f['type']=='LoadControl' and f['role']=='server' for f in d['features']))
            result=rpc(cem,op='write_limit',entity=pv['key'],direction='produce',value_w=500,duration_s=7200)['write_limit']
            assert result['accepted']
            state=until(lambda: s if (s:=plant.call(op='advance'))['link']['fresh'] else None)
            assert 0<state['pv_w']<=500.00001 and abs(state['balance_w'])<1e-6,state
            checks.append('Generic EEBUS discovery and single LPP control, no six-channel SLX dependency')
            # mDNS is checked independently of explicit IP pairing when the host permits multicast.
            peers=rpc(cem,op='network_status')['network_status']['peers']
            mdns_seen=any(d['ski']==a['ski'] for d in peers)
            server=rpc(plant,op='sunspec_start',address='127.0.0.1:0')['sunspec_start'];port=int(server['address'].rsplit(':',1)[1])
            state=plant.call(op='advance')
            assert not state['link']['fresh'] # Listening alone does not pretend that a control arrived.
            assert adu(port,1,struct.pack('>BHH',3,40000,2),True)==b'\x03\x04SunS'
            assert adu(port,1,struct.pack('>BHH',4,40000,2))==b'\x04\x04SunS'
            assert adu(port,1,struct.pack('>BHH',3,40000,0))==b'\x83\x03'
            assert adu(port,1,struct.pack('>BHH',3,40000,126))==b'\x83\x03'
            assert adu(port,1,struct.pack('>BHH',6,40000,1))==b'\x86\x02'
            assert adu(port,1,b'\x2b')==b'\xab\x01'
            assert adu(port,99,struct.pack('>BHH',3,40000,2))==b'\x83\x0b'
            assert adu(port,1,struct.pack('>BHH',3,40000,2)+b'X')==b'\x83\x03'
            checks.append('MBAP fragmentation, FC03/04/06, invalid count/unit/read-only/trailing-byte rejection')
            scan=rpc(cem,op='sunspec_scan',host='127.0.0.1',port=port,unit=1)['sunspec_scan']
            assert [m['id'] for m in scan['models']]==[1,101,120,123]
            for point,value in [('WMaxLimPct',25),('WMaxLim_Ena',1)]:
                rpc(cem,op='sunspec_write',host='127.0.0.1',port=port,unit=1,model=123,point=point,value=value)
            control=next(m for m in scan['models'] if m['id']==123)['address']
            invalid=struct.pack('>BHHBHH',16,control+4,2,4,0,10001)
            assert adu(port,1,invalid)==bytes([0x90,3])
            assert adu(port,1,struct.pack('>BHH',3,control+4,2))==bytes([3,4,0,1,9,196]) # Conn=1, WMaxLimPct=2500
            checks.append('Atomic FC16 invalid range rolls back every register')
            state=plant.call(op='advance');assert abs(state['pv_w']-min(state['pv_available_w'],state['config']['inverter_w']*.25))<1e-6,state
            checks.append('SunSpec remote client discovery and scaled PV control applied to energy model')
            if args.pysunspec:
                sys.path.insert(0,str(args.pysunspec.resolve()))
                from sunspec2.modbus.client import SunSpecModbusClientDeviceTCP
                clients=[]
                try:
                    for unit,ids in [(1,[1,101,120,123]),(2,[1,201]),(3,[1,101,120,124])]:
                        d=SunSpecModbusClientDeviceTCP(slave_id=unit,ipaddr='127.0.0.1',ipport=port,timeout=3)
                        clients.append(d);d.scan();assert {k for k in d.models if isinstance(k,int)}==set(ids),d.models
                        assert d.models[1][0].Mn.value=='Silicon LogiX'
                        if unit==1:
                            inv=d.models[101][0];assert abs(inv.W.cvalue-state['pv_w'])<=.5
                            ctl=d.models[123][0];assert ctl.WMaxLimPct.cvalue==25
                            ctl.WMaxLimPct.cvalue=10;ctl.WMaxLimPct.write()
                        if unit==2:assert abs(d.models[201][0].W.cvalue-state['grid_w'])<=5.01
                        if unit==3:
                            bat=d.models[124][0];assert abs(bat.ChaState.cvalue-state['soc']*100)<=.011
                            bat.ChaGriSet.value=1;bat.ChaGriSet.write()
                            bat.InWRte.cvalue=50;bat.InWRte.write()
                            bat.OutWRte.cvalue=-50;bat.OutWRte.write()
                            bat.StorCtl_Mod.value=3;bat.StorCtl_Mod.write()
                    state=plant.call(op='advance')
                    rating=max(state['config']['charge_w'],state['config']['discharge_w'])
                    assert abs(state['battery_w']+rating*.5)<1e-6,state
                    assert state['pv_w']<=state['config']['inverter_w']*.1+1e-6
                    bat=clients[2].models[124][0]
                    bat.ChaGriSet.value=0;bat.ChaGriSet.write()
                    no_grid=plant.call(op='advance');assert no_grid['grid_w']<=.000001,no_grid
                    bat.ChaGriSet.value=1;bat.ChaGriSet.write()
                    checks.append('SunSpec ChaGriSet prevents battery charging from grid')
                    checks.append('Independent SunSpec Alliance pysunspec2: all units, signed grid, SF, PV writes and forced storage charging')
                finally:
                    for d in clients:d.disconnect()
            fixture=Fixture()
            try:
                data=rpc(cem,op='sunspec_scan',host='127.0.0.1',port=fixture.port,unit=1)['sunspec_scan']
                assert data['base']==0
                models={m['id']:m for m in data['models']}
                assert models[1]['points']['Mn']=='Independent TCP fixture'
                assert models[65000]['length']==2
                assert models[103]['points']['W']==1234.5
                assert models[113]['points']['W']==5654.25 and models[113]['points']['WH']==123456.5
                assert models[213]['points']['W']==-4321.5 and models[213]['points']['TotWhImp']==54321.25
                changed=rpc(cem,op='sunspec_write',host='127.0.0.1',port=fixture.port,unit=1,model=123,point='WMaxLimPct',value=37.5)['sunspec_write']
                assert next(m for m in changed['models'] if m['id']==123)['points']['WMaxLimPct']==37.5
                checks.append('SunSpec client writes to FC16-only device')
                checks.append('Independent non-SLX TCP fixture: base 0, unknown-model skip, three-phase integer and float decoding')
            finally:fixture.close()
            legacy=Fixture(write_function=6)
            try:
                result=rpc(cem,op='sunspec_write',host='127.0.0.1',port=legacy.port,unit=1,model=123,point='WMaxLimPct',value=42.25)['sunspec_write']
                assert next(m for m in result['models'] if m['id']==123)['points']['WMaxLimPct']==42.25
                checks.append('SunSpec client FC06 fallback on legacy device')
            finally:legacy.close()
            # Optional SLX register bank is separate from every standard model chain.
            for offset,v in [(3,1840),(6,1),(7,1)]:write(port,10,60000+offset,v)
            state=plant.call(op='advance');assert state['ev_w']==1840,state
            time.sleep(1.1);state=plant.call(op='advance');assert state['ev_w']==0
            checks.append('Optional SLX EV setpoint and watchdog expiry')
            write(port,1,control+7,1)
            time.sleep(1.1);state=plant.call(op='advance')
            assert abs(state['pv_w']-state['pv_available_w'])<1e-6,state
            checks.append('SunSpec PV reversion timer restores local production')
            packets=[];after=0
            for _ in range(24):
                batch=plant.call(op='packets',after=after);packets.extend(batch)
                if batch:after=batch[-1]['id']
                if len(batch)<16:break
            assert any(p['type'].startswith('Modbus') and bytes.fromhex(p['hex'])[2:4]==b'\x00\x00' for p in packets)
            checks.append('Actual Modbus ADU monitor capture')
        finally:plant.close();cem.close()
    report={'result':'passed','checks':checks,'mdns_peer_observed':mdns_seen,'independent_client':bool(args.pysunspec),'scope':'PC loopback, no commercial device or Raspberry hardware certification'}
    if args.report:args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_text(json.dumps(report,indent=2)+'\n','utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
