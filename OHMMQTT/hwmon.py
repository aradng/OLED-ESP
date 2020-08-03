
import os
import clr #package pythonnet, not clr
import jsonpickle
import socket
import logging
from time import sleep
from coapthon.server.coap import CoAP
from coapthon.resources.resource import Resource
from zeroconf import IPVersion, ServiceInfo, Zeroconf

#disable logging
logging.config.fileConfig("logging.conf", disable_existing_loggers=True)

#class defines
openhardwaremonitor_hwtypes = ['Mainboard','SuperIO','CPU','RAM','GpuNvidia','GpuAti','TBalancer','Heatmaster','HDD']
openhardwaremonitor_sensortypes = ['Voltage','Clock','Temperature','Load','Fan','Flow','Control','Level','Factor','Power','Data','SmallData']

class Data:
    def __init__(self):
        self.gpu = self.sens()
        self.cpu = self.sens()
        self.mem = self.sens()
    class sens:
        def __init__(self):
            self.name = ""
            self.load = 0
            self.temp = 0
data = Data();
del data.mem.temp
del data.mem.name
#hwmonitor init
def initialize_openhardwaremonitor():
    file = (os.path.dirname(os.path.realpath(__file__)) + "/OpenHardwareMonitorLib.dll");
    clr.AddReference(file)

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
                data.gpu.temp = int(sensor.Value)
                data.gpu.name = sensor.Hardware.Name
            if sensor.Name == "CPU Package" :
                data.cpu.temp = int(sensor.Value)
                data.cpu.name = sensor.Hardware.Name
        if sensor.SensorType == sensortypes.index('Load') :
            if(sensor.Name == "CPU Total") :
                data.cpu.load = int(sensor.Value)
            if(sensor.Name == "GPU Core") :
                data.gpu.load = int(sensor.Value)
        if sensor.Name == "Used Memory" :
            data.mem.load = format(sensor.Value ,'.1f')
#COAP Server
class hwmon(Resource):

    def __init__(self,name="hwmon",coap_server=None):
        super(hwmon,self).__init__(name,coap_server,visible=True,observable=True,allow_children=True)
        self.content_type = "application/json"

    def render_GET(self,request):    
        fetch_stats(HardwareHandle)
        self.payload = jsonpickle.encode(data,unpicklable=False)
        return self

class CoAPServer(CoAP):
	def __init__(self, host, port):
		CoAP.__init__(self, (host, port))
		self.add_resource('/hwmon', hwmon())
#MDNS Zerconf config
info = ServiceInfo(
        "_CoAP._udp.local.",
        "hwmon._CoAP._udp.local.",
        addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))],
        port=5683,
        server="hwmon.local.",
    )
zeroconf = Zeroconf(ip_version=IPVersion.All)
zeroconf.register_service(info)
print("MDNS Registered as " + info.server)

if __name__ == '__main__':
    HardwareHandle = initialize_openhardwaremonitor()
    server = CoAPServer(socket.gethostbyname(socket.gethostname()), 5683)
    try:
        print ("CoAP Server Start")
        server.listen(10)
    except KeyboardInterrupt:
        print ("Server Shutdown")
        server.close()
        zeroconf.unregister_service(info)
        print ("unresgiterd")
        zeroconf.close()
        print ("Exiting...")