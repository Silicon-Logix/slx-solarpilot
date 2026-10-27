# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Independent wire fixture: base 0, unknown model, three-phase int and float.
Offsets and lengths follow the SunSpec published models, not the SLX encoder.
"""
import socketserver, struct, threading
class Fixture:
    def __init__(self, write_function=16):
        words=[0x5375,0x6e53]
        def model(mid,length):
            at=len(words);words.extend([65535]*(length+2));words[at:at+2]=[mid,length];return at
        common=model(1,66)
        words[common+2:common+18]=struct.unpack('>16H',b'Independent TCP fixture'.ljust(32,b'\0'))
        unknown=model(65000,2)
        words[unknown+2:unknown+4]=[42,43]
        integer=model(103,50);words[integer+14]=12345;words[integer+15]=65535 # SF -1
        def f32(at,value):words[at:at+2]=struct.unpack('>2H',struct.pack('>f',value))
        inverter=model(113,60);f32(inverter+22,5654.25);f32(inverter+32,123456.5)
        meter=model(213,124);f32(meter+28,-4321.5);f32(meter+60,12345.5);f32(meter+68,54321.25)
        control=model(123,24);words[control+5]=5000;words[control+23]=65534
        words.extend([65535,0])
        class Handler(socketserver.BaseRequestHandler):
            def handle(self):
                def exact(n):
                    data=b''
                    while len(data)<n:
                        b=self.request.recv(n-len(data))
                        if not b:raise EOFError()
                        data+=b
                    return data
                try:
                    h=exact(7);p=exact(struct.unpack('>H',h[4:6])[0]-1)
                    fc,addr,count=struct.unpack('>BHH',p[:5])
                    if fc==write_function and addr==control+5 and (fc==6 or count==1):
                        words[addr]=count if fc==6 else struct.unpack('>H',p[6:8])[0];r=p[:5]
                    elif fc!=3:r=bytes([fc|128,1])
                    elif addr+count>len(words):r=bytes([fc|128,2])
                    else:r=bytes([fc,count*2])+struct.pack('>'+str(count)+'H',*words[addr:addr+count])
                    self.request.sendall(h[:4]+struct.pack('>H',len(r)+1)+h[6:7]+r)
                except (EOFError,OSError):pass
        self.server=socketserver.ThreadingTCPServer(('127.0.0.1',0),Handler)
        self.server.daemon_threads=True
        self.port=self.server.server_address[1]
        self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start()
    def close(self):self.server.shutdown();self.server.server_close();self.thread.join(2)
