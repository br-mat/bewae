import os, re, argparse, time, datetime
from influxdb import InfluxDBClient

# Set required InfluxDB parameters.
host = "localhost" #Could also set local ip address
port = 8086
user = "*******"
password = "********"
dbname = "****"
# How frequently we will write sensor data to the database in s
sampling_period = 20

def getCPUtemperature():
    # Return CPU temperature as a character string
    res = os.popen('vcgencmd measure_temp').readline()
    return (float)(res.replace("temp=","").replace("'C\n",""))

def getRAMinfo():
    # Return RAM information (unit=kb) in a list
    # Index 0: total RAM
    # Index 1: used RAM
    # Index 2: free RAM
    p = os.popen('free')
    i = 0
    while 1:
        i = i + 1
        line = p.readline()
        if i==2:
            return(line.split()[1:4])

def getDiskSpace():
    # Return % of CPU used by user as a float
    #def getCPUuse():
    #    return (float)(os.popen("top -n1 | awk '/Cpu\(s\):/ {print $2}'").readline().strip())
    p = os.popen("df -h /")
    i = 0
    while 1:
        i = i +1
        line = p.readline()
        if i==2:
            return (line.split()[1:5])

def get_args():

    #'''This function parses and returns arguments passed in'''
    # Assign description to the help doc
    parser = argparse.ArgumentParser(description='Program writes measurements data to specified influx db.')
    # Add arguments
    parser.add_argument('-db','--database', type=str, help='Database name', required=True)
    parser.add_argument('-sn','--session', type=str, help='Session', required=True)
    now = datetime.datetime.now()
    parser.add_argument('-rn','--run', type=str, help='Run number', required=False,default=now.strftime("%Y%m%d%H%M"))
    # Array of all arguments passed to script
    args=parser.parse_args()
    # Assign args to variables
    dbname=args.database
    runNo=args.run
    session=args.session
    return dbname, session,runNo

def get_data_points():
    # Get the three measurement values from the SenseHat sensors
    #CPU_usage = getCPUuse() 
    CPU_usage = 0.0
    CPU_temp = getCPUtemperature()
    RAM_used_raw = float(getRAMinfo()[1]) +64 * 1024 #the video card takes 64 mb
    print("ram used raw:" + str(RAM_used_raw))
    RAM_used_percent=int((RAM_used_raw/float(getRAMinfo()[0]))*100)
    RAM_installed = (int(float(getRAMinfo()[0])/1024))
    disk_free_1 = getDiskSpace()[3] #SD CARD
    disk_free = re.sub("[0-9]","",disk_free_1)
    disk_free_nmb = int(re.sub("[0-9]","",disk_free_1).replace('%', '0'))
    disk_total_1 = getDiskSpace()[0]
    disk_total = re.sub("[0-9]","",disk_total_1)
    timestamp=datetime.datetime.utcnow().isoformat()
    # Create Influxdb datapoints (using lineprotocol as of Influxdb >1.1)
    datapoints = [{"measurement": session,"tags": {"runNum": runNo,},"time": timestamp,"fields": {"CPU_usage":CPU_usage,"CPU_temp":CPU_temp,"RAM_used_percent":RAM_used_percent,"RAM_installed":RAM_installed,"disk_free":disk_free,"disk_free_nmb":disk_free_nmb,"disk_total":disk_total}}]
    return datapoints

def main():
    # Match return values from get_arguments()
    # and assign to their respective variables
    dbname, session, runNo =get_args()
    print("Session: ", session)
    print("Run No: ", runNo)
    print("DB name: ", dbname)
    # Initialize the Influxdb client
    client = InfluxDBClient(host, port, user, password, dbname)
    try:
        for x in range(1, 4):
    # Write datapoints to InfluxDB
            datapoints=get_data_points()
            bResult=client.write_points(datapoints)
            print("Write points {0} Bresult:{1}".format(datapoints,bResult))
    # Wait for next sample
            time.sleep(sampling_period)
    # Run until keyboard ctrl-c
    except KeyboardInterrupt:
        print ("Program stopped by keyboard interrupt [CTRL_C] by user. ")


if __name__ == '__main__':
    print('Get Pi status values:')
    main()