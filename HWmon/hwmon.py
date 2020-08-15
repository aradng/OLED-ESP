import os
import clr #package pythonnet, not clr
import json
import time 
import socket
import serial
import logging
import threading
import serial.tools.list_ports
import paho.mqtt.client as mqtt
from coapthon.server.coap import CoAP
from coapthon.resources.resource import Resource
from zeroconf import IPVersion, ServiceInfo, Zeroconf

client = mqtt.Client();
client.connect("localhost");

#disable logging
logging.config.fileConfig("logging.conf", disable_existing_loggers=True)

openhardwaremonitor_hwtypes = ['Mainboard','SuperIO','CPU','RAM','GpuNvidia','GpuAti','TBalancer','Heatmaster','HDD']
openhardwaremonitor_sensortypes = ['Voltage','Clock','Temperature','Load','Fan','Flow','Control','Level','Factor','Power','Data','SmallData']


data = {
    "bigmode"  : 0 ,
    "gpu_name" : "",
    "gpu_load" : 0 ,
    "gpu_temp" : 0 ,
    "cpu_name" : "",
    "cpu_load" : 0 ,
    "cpu_temp" : 0 ,
    "mem_load" : 0 ,
}

#py2exe dogshit
def resource_path(relative_path):
    try:
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")
    return os.path.join(base_path, relative_path)

#hwmonitor init
def initialize_openhardwaremonitor():
    clr.AddReference(resource_path('OpenHardwareMonitorLib.dll'))
    from OpenHardwareMonitor import Hardware
    handle = Hardware.Computer()
    handle.CPUEnabled = True
    handle.RAMEnabled = True
    handle.GPUEnabled = True
    IsCpuEnabled = True
    IsGpuEnabled = True
    IsMemoryEnabled = True
    handle.Open()
    return handle

def fetch_stats(handle):
    for i in handle.Hardware:
        i.Update()
        for sensor in i.Sensors:
            parse_sensor(sensor)


def parse_sensor(sensor):
    if sensor.Value is not None:
        if type(sensor).__module__ == 'OpenHardwareMonitor.Hardware':
            sensortypes = openhardwaremonitor_sensortypes
            hardwaretypes = openhardwaremonitor_hwtypes
        else:
            return
        if sensor.SensorType == sensortypes.index('Temperature') :
            if sensor.Name == "GPU Core" :
                data['gpu_temp'] = int(sensor.Value)
                data['gpu_name'] = sensor.Hardware.Name
            if sensor.Name == "CPU Package" :
                data['cpu_temp'] = int(sensor.Value)
                data['cpu_name'] = sensor.Hardware.Name
        if sensor.SensorType == sensortypes.index('Load') :
            if(sensor.Name == "CPU Total") :
                data['cpu_load'] = int(sensor.Value)
            if(sensor.Name == "GPU Core") :
                data['gpu_load'] = int(sensor.Value)
        if sensor.Name == "Used Memory" :
            data['mem_load'] = format(sensor.Value ,'.1f')


def mDNS():
    #zerconf info
    infomqtt = ServiceInfo(
        "_mqtt._tcp.local.",
        "hwmon._mqtt._tcp.local.",
        addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))],
        port=1883,
        server="hwmon.local.",
    )
    infocoap = ServiceInfo(
        "_coap._udp.local.",
        "hwmon._coap._udp.local.",
        addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))],
        port=5683,
        server="hwmon.local.",
    )
    zeroconf = Zeroconf(ip_version=IPVersion.All)
    while (stop_threads == False):
        try :
            zeroconf.unregister_service(infomqtt)
            zeroconf.unregister_service(infocoap)
            zeroconf.close()
            zeroconf.close()
            zeroconf = Zeroconf(ip_version=IPVersion.All)
            infomqtt.addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))]
            infocoap.addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))]
            zeroconf.register_service(infomqtt)
            zeroconf.register_service(infocoap)
        except :
            pass
        while(infocoap.addresses == [socket.inet_aton(socket.gethostbyname(socket.gethostname()))] and stop_threads == False) :
            try:
                client.publish("display/hwmon" ,json.dumps(data));
                time.sleep(0.2)
            except:
                pass
    zeroconf.unregister_service(infomqtt)
    zeroconf.unregister_service(infocoap)
    zeroconf.close()

def Serial():
    while (stop_threads == False):
        ports = list(serial.tools.list_ports.comports())
        for p in ports:
            if(p.device != 'COM1'):
                ans = b"ack"
                while (ans == b"ack" and stop_threads == False):
                    try:
                        ans = ""
                        fetch_stats(HardwareHandle)
                        payload = json.dumps(data)
                        payload += '\n'
                        ser = serial.Serial(p.device , 115200 , timeout = 1)
                        ser.write(payload.encode('utf-8'))
                        ans = ser.read(3)
                        ser.close()
                    except:
                        pass
                    time.sleep(0.2)

global stop_threads
stop_threads = False
thread1 = threading.Thread(target=mDNS)
thread2 = threading.Thread(target=Serial)
thread1.start()
thread2.start()

#COAP Server
class hwmon(Resource):

    def __init__(self,name="hwmon",coap_server=None):
        super(hwmon,self).__init__(name,coap_server,visible=True,observable=True,allow_children=True)
        self.content_type = "application/json"

    def render_GET(self,request):    
        #fetch_stats(HardwareHandle)
        self.payload = json.dumps(data)
        return self

class CoAPServer(CoAP):
    def __init__(self, host, port):
        CoAP.__init__(self, (host, port))
        self.add_resource('/hwmon', hwmon())

server = CoAPServer("0.0.0.0", 5683)

HardwareHandle = initialize_openhardwaremonitor()

try:
    server.listen(10)
except KeyboardInterrupt:
    server.close()
    stop_threads = True
    thread1.join()
    thread2.join()
    exit()