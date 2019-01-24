import sys
import socket
import numpy as np
import struct
import time
import threading

args = sys.argv

dirname="data"
filename="1"
if len(args)>1:
    filename=args[1]
    print("ファイル名：" + args[1])
filepath="./"+dirname+"/"+filename+""
csv_path=filepath+".csv"

HOST = '0.0.0.0' 
PORT = 44444 
BUFFER_SIZE = 6144
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind((HOST, PORT))
past_len=0

f=open(filepath, "ab")


loop_flag = True
def wait_input():
    global loop_flag
    input()
    print("ENTER")
    loop_flag = False
    # # sys.exit()
    # s.close()
    # f.close()
    # print('CLOSE1')


th = threading.Thread(target=wait_input)
th.start()


while loop_flag:
    received_data = s.recvfrom(BUFFER_SIZE)
    if received_data:
    # print('Client to Server: ' , data[0].upper(), data[1])
        data=received_data[0]
        sender=received_data[1]
        print(data[0], len(data),sender)
        for x in data:         
            f.write(struct.pack("B", x))
        if past_len+len(data)==2048:
            s.sendto(b'ok', sender)
        past_len=len(data)

print('CLOSE')
s.close()
f.close()
th.join()