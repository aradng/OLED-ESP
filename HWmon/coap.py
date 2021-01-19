import os
import sys
import clr #package pythonnet, not clr
import json
import time 
import socket
import logging
import threading
from PySide2 import QtWidgets, QtGui
from coapthon.server.coap import CoAP
from coapthon.resources.resource import Resource
from zeroconf import IPVersion, ServiceInfo, Zeroconf,InterfaceChoice

#disable logging
#logging.config.fileConfig("logging.conf", disable_existing_loggers=True)

openhardwaremonitor_hwtypes = ['Mainboard','SuperIO','CPU','RAM','GpuNvidia','GpuAti','TBalancer','Heatmaster','HDD']
openhardwaremonitor_sensortypes = ['Voltage','Clock','Temperature','Load','Fan','Flow','Control','Level','Factor','Power','Data','SmallData']


data = {
	"bigmode"  : False ,
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

global stop_threads
stop_threads = False

def mDNS():
	#zerconf info
	info = []
	stream = os.popen('for /f "usebackq tokens=2 delims=:" %f in (`ipconfig ^| findstr /c:"IPv4 Address"`) do @echo off && echo%f')
	output = stream.read()
	output = output[:-1]
	for ip in output.split('\n'):
		print(ip)
		info.append(ServiceInfo(
			"_coap._udp.local.",
			"hwmon._coap._udp.local.",
			addresses=[socket.inet_aton("0.0.0.0")],
			port=5683,
			server="hwmon.local.",
			#host_ttl=0,
			#other_ttl=0,
		))
	zeroconf = Zeroconf(InterfaceChoice.All)
	#logging.basicConfig(level=logging.DEBUG)
	#logging.getLogger('zeroconf').setLevel(logging.DEBUG)
	while (stop_threads == False):
		stream = os.popen('for /f "usebackq tokens=2 delims=:" %f in (`ipconfig ^| findstr /c:"IPv4 Address"`) do @echo off && echo%f')
		output = stream.read()
		output = output[:-1]
		i = 0
		for ip in output.split('\n'):
			if(info[i].addresses != [socket.inet_aton(ip)]):
				zeroconf.unregister_service(info[i])
				info[i].addresses=[socket.inet_aton(ip)]
				zeroconf.register_service(info[i])
			i += 1
		time.sleep(1)
	zeroconf.close()

HardwareHandle = initialize_openhardwaremonitor()

#COAP Server
class hwmon(Resource):

	def __init__(self,name="hwmon",coap_server=None):
		super(hwmon,self).__init__(name,coap_server,visible=True,observable=True,allow_children=True)
		self.content_type = "application/json"

	def render_GET(self,request):    
		fetch_stats(HardwareHandle)
		self.payload = json.dumps(data)
		#print(self.payload)
		return self

class CoAPServer(CoAP):
	def __init__(self, host, port):
		CoAP.__init__(self, (host, port))
		self.add_resource('/hwmon', hwmon())

server = CoAPServer("0.0.0.0", 5683)

def systray():
	server.listen(10)

threadmdns = threading.Thread(target=mDNS)
threadcoap = threading.Thread(target=systray)

class SystemTrayIcon(QtWidgets.QSystemTrayIcon):
	def __init__(self, icon, parent=None):
		QtWidgets.QSystemTrayIcon.__init__(self, icon, parent)
		self.setToolTip(f'Hardware Monitor')
		menu = QtWidgets.QMenu(parent)

		big_mode = menu.addAction("Big Mode")
		big_mode.setCheckable(True)
		big_mode.triggered.connect(self.big_mode)

		menu.addSeparator()
		exit = menu.addAction("Exit")
		exit.triggered.connect(self.stopthreads)

		self.setContextMenu(menu)

		threadcoap.start()
		threadmdns.start()
	def stopthreads(self):
		global stop_threads
		stop_threads = True
		server.close()
		threadmdns.join()
		threadcoap.join()
		app.quit()

	def big_mode(self , reason):
		data['bigmode'] = reason

app = QtWidgets.QApplication()
w = QtWidgets.QWidget()
tray_icon = SystemTrayIcon(QtGui.QIcon(resource_path('icon.ico')), w)
tray_icon.show()

sys.exit(app.exec_())