

def printReading(reading):
    print (str((reading >> 15) & 0b1) +
        str((reading >> 14) & 0b1) +
        str((reading >> 13) & 0b1) +
        str((reading >> 12) & 0b1) +
        str((reading >> 11) & 0b1) +
        str((reading >> 10) & 0b1) +
        str((reading >> 9) & 0b1) +
        str((reading >> 8) & 0b1) +
        str((reading >> 7) & 0b1) +
        str((reading >> 6) & 0b1) +
        str((reading >> 5) & 0b1) +
        str((reading >> 4) & 0b1) +
        str((reading >> 3) & 0b1) +
        str((reading >> 2) & 0b1) +
        str((reading >> 1) & 0b1) +
        str((reading >> 0) & 0b1))


def spiAnalysis(pathToInput, pathToOutput, clockPolarity, clockChan, csChan, mosiChan, misoChan):

    inputFile = open(pathToInput, "rb")
    outputFile = open(pathToOutput, "w")

    outputFile.write("**** SPI ANALYSIS ****\n\n")

    data = inputFile.read(2)

    bitsTransmit = 0
    tempMosi = 0
    tempMiso = 0
    time = 0

    if(clockPolarity):
        lastClock = 1
    else:
        lastClock = 0
    lastCS = 1

    while(len(data) >= 2):
        reading = (data[0] << 8) | data[1]
        #printReading(reading)
        thisClock = (reading >> clockChan) & 0b1
        thisCS    = (reading >> csChan) & 0b1
        thisMosi  = (reading >> mosiChan) & 0b1
        thisMiso  = (reading >> misoChan) & 0b1

        if((thisClock == clockPolarity) and (lastClock != clockPolarity)):
            bitsTransmit = bitsTransmit + 1
            tempMosi = (tempMosi << 1) | thisMosi
            tempMiso = (tempMiso << 1) | thisMiso
            #print("Clock Found")

        if(thisCS != lastCS):
            if(thisCS):
                #print("Trasmission found with " + str(bitsTransmit) + " bits, at time " + str(time))
                #print("Mosi: " + hex(tempMosi))
                #print("Miso: " + hex(tempMiso))
                outputFile.write("Trasmission found with " + str(bitsTransmit) + " bits, at time " + str(time) + "\n")
                outputFile.write("Mosi: " + hex(tempMosi) + "\n")
                outputFile.write("Miso: " + hex(tempMiso) + "\n")
            else:
                bitsTransmit = 0
                tempMosi = 0
                tempMiso = 0

        lastClock = thisClock
        lastCS = thisCS
        data = inputFile.read(2)
        time = time + 1

    outputFile.close()
    inputFile.close()




spiAnalysis("input.raw", "output.txt", True, 0, 1, 4, 5)
