import os
import clr #package pythonnet, not clr
import json
import serial
import serial.tools.list_ports
from time import sleep

#class defines
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

HardwareHandle = initialize_openhardwaremonitor()
while True:
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        if(p.device != 'COM1'):
            print(p.device)
            ans = b"ack"
            while ans == b"ack":
                try:
                    ans = ""
                    fetch_stats(HardwareHandle)
                    payload = json.dumps(data)
                    print(payload)
                    payload += '\n'
                    ser = serial.Serial(p.device , 115200 , timeout = 1)
                    ser.write(payload.encode('utf-8'))
                    ans = ser.read(3)
                    ser.close()
                except:
                    pass
                sleep(0.2)