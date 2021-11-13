import os
import clr #package pythonnet, not clr
import json
import time 
import socket
import paho.mqtt.client as mqtt
from zeroconf import IPVersion, ServiceInfo, Zeroconf

client = mqtt.Client();
client.connect("localhost");

openhardwaremonitor_hwtypes = ['Mainboard','SuperIO','CPU','RAM','GpuNvidia','GpuAti','TBalancer','Heatmaster','HDD']
openhardwaremonitor_sensortypes = ['Voltage','Clock','Temperature','Load','Fan','Flow','Control','Level','Factor','Power','Data','SmallData']


data = {
    "bigmode"  : True ,
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

#MDNS Zerconf config
info = ServiceInfo(
    "_mqtt._tcp.local.",
    "hwmon._mqtt._tcp.local.",
    addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))],
    port=1883,
    server="hwmon.local.",
)
zeroconf = Zeroconf(ip_version=IPVersion.All)

HardwareHandle = initialize_openhardwaremonitor()
while True:
    try :
        zeroconf.unregister_service(info)
        zeroconf.close()
        zeroconf = Zeroconf(ip_version=IPVersion.All)
        info.addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))]
        zeroconf.register_service(info)
    except :
        pass
    while(info.addresses == [socket.inet_aton(socket.gethostbyname(socket.gethostname()))]) :
        try :
            fetch_stats(HardwareHandle)
            client.publish("display/hwmon" ,json.dumps(data));
            time.sleep(0.2);
        except KeyboardInterrupt:
            zeroconf.unregister_service(info)
            zeroconf.close()
            exit()