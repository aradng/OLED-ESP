import os
import clr #package pythonnet, not clr
import paho.mqtt.client as mqtt
import time 


client = mqtt.Client();
client.connect("localhost");

openhardwaremonitor_hwtypes = ['Mainboard','SuperIO','CPU','RAM','GpuNvidia','GpuAti','TBalancer','Heatmaster','HDD']
openhardwaremonitor_sensortypes = ['Voltage','Clock','Temperature','Load','Fan','Flow','Control','Level','Factor','Power','Data','SmallData']


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
                client.publish("GPU/Temperature" , int(sensor.Value));
                client.publish("GPU/Name" , sensor.Hardware.Name);
                #print(sensor.Name)
                #print(int(sensor.Value))
            if sensor.Name == "CPU Package" :
                client.publish("CPU/Temperature" , int(sensor.Value));
                client.publish("CPU/Name" , sensor.Hardware.Name);
        if sensor.SensorType == sensortypes.index('Load') :
            if(sensor.Name == "CPU Total") :
                client.publish("CPU/Load" , int(sensor.Value));
                #printsensor(sensor.Name)
                #print(int(sensor.Value))
            if(sensor.Name == "GPU Core") :
                client.publish("GPU/Load" , int(sensor.Value));
                #print(sensor.Name)
                #print(int(sensor.Value))
        if sensor.Name == "Used Memory" :
            client.publish("Mem/Load" , format(sensor.Value ,'.1f'));
            #print(sensor.Name)
            #print(format(sensor.Value ,'.1f'))

if __name__ == "__main__":
    print("OpenHardwareMonitor:")
    HardwareHandle = initialize_openhardwaremonitor()
    while(True) :
        fetch_stats(HardwareHandle)
        time.sleep(0.3);