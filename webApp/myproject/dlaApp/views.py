from __future__ import unicode_literals
from django.shortcuts import render
from django.shortcuts import redirect
from django.http import HttpResponse
from django.views.static import serve
import os
import subprocess
import serial
import sys

NUM_OF_CHANNELS = 16
DEBUG = True
user_input_string = ''
#################################### WebApp View functions ###################################

def visual(request):
	channels = readData("")
	context = {"channels": channels, "button_num": range(1,NUM_OF_CHANNELS + 1),"display_trigger": True}
	return render(request, "dlaApp/visual.html", context)

def visualAjax(request):

	form = request.POST
	data = form["dataStr"]
	channels = readData(data)
	context = {"button_num": range(1,NUM_OF_CHANNELS + 1), "display_data": True, "protocol": True}
	context["data"] = data
	for i in range(NUM_OF_CHANNELS):
		key = "channel" + str(i+1)
		context[key] = channels[i]
	return render(request, "dlaApp/visual.html", context)


##################################### Backend Data Processing ############################

def readData(s):
	# type: () -> List(List(str))

	chanData = []
	if s == "":
		return []
	timeData = s
	for i in range(NUM_OF_CHANNELS-1, -1, -1):
		lst = []
		j = i
		while j < len(timeData):
			lst.append(timeData[j])
			j += NUM_OF_CHANNELS	
		string = "".join(lst)
		chanData.append(string)

	if DEBUG:
		if (len(timeData) % 16) != 0:
			print("data length is not divisible by 16")
	return chanData

def start(request):

    valid = True
    sendData = dict()
    s = ""
    if request.method == 'POST':
        form = request.POST
        if "voltage" not in form:
        	valid = false
        else:
        	s = form["voltage"] + "\n"
        for i in range(1, NUM_OF_CHANNELS + 1):
                key = "channel" + str(i)
                if key in form and form[key] != "":
                        s += form[key] + ","
                else:
                        s += "2,"
        s = s[:-1]
        s += ";"
        writeUserInput(s)
        global user_input_string
        user_input_string = s
        context ={"loading":True, "read": True}
        return render(request, "dlaApp/visual.html", context)

def readProcessedData(request):

	subprocess.call(['python3', 'read_usb_serial.py',user_input_string])
	with open("test.txt", 'r') as f:
		readStr = f.read()
		f.close()
	return HttpResponse(readStr)

############################### Protocol Analysis ##################################

def startProtocol(request):


	form = request.POST
	clockChan = int(form["sclk"])
	mosiChan = int(form["mosi"])
	misoChan = int(form["miso"])
	csChan = int(form["ss"])
	clockPolarity = int(form["cp"])
	strData = form["data"]



	pathToOutput = "out.txt"
	outputFile = open(pathToOutput, "w")


	outputFile.write("**** SPI ANALYSIS ****\n\n")



	bitsTransmit = 0
	tempMosi = 0
	tempMiso = 0
	time = 0

	if(clockPolarity):
		lastClock = 1
	else:
		lastClock = 0
	lastCS = 1

	count = len(strData) / 16
	for i in range(count):
		start_index = i* 16
		first_byte = int(strData[start_index: start_index+8],2)
		second_byte = int(strData[start_index+8: start_index +16],2)
		reading = (first_byte << 8) | second_byte
		
		thisClock = (reading >> clockChan) & 0b1
		thisCS    = (reading >> csChan) & 0b1
		thisMosi  = (reading >> mosiChan) & 0b1
		thisMiso  = (reading >> misoChan) & 0b1
		if((thisClock == clockPolarity) and (lastClock != clockPolarity)):
			bitsTransmit = bitsTransmit + 1
			tempMosi = (tempMosi << 1) | thisMosi
			tempMiso = (tempMiso << 1) | thisMiso
  

		if(thisCS != lastCS):
			if(thisCS):
				outputFile.write("Trasmission found with " + str(bitsTransmit) + " bits, at time " + str(time) + "\n")
				outputFile.write("Mosi: " + hex(tempMosi) + "\n")
				outputFile.write("Miso: " + hex(tempMiso) + "\n")
			else:
				bitsTransmit = 0
				tempMosi = 0
				tempMiso = 0
		lastClock = thisClock
		lastCS = thisCS
		time = time + 1

	outputFile.close()

	return redirect("/serveFile")


def serveFile(request):
	filepath = "out.txt"
	return serve(request, os.path.basename(filepath), os.path.dirname(filepath))


