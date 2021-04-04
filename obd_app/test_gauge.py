#!/usr/bin/env python3
from tkinter import *
from serial import *
import tkinter as tk
from tkinter.font import Font
import asyncio
import websockets
import socket
from threading import Thread
import gaugelib
import json
import time
import os
import sys

win = tk.Tk()
#a5 = PhotoImage(file="g1.png")
#win.tk.call('wm', 'iconphoto', win._w, a5)
win.title("OBD VAG COM")
win.geometry("800x400+0+0")
win.resizable(width=True, height=True)
win.configure(bg='black')
win.grid_columnconfigure(0, weight=1)
win.grid_columnconfigure(1, weight=1)
win.attributes("-fullscreen", True)

USERS = set()
serBuffer=''
received = ''

cnt=0
num_retries= 5
exception = True
while (exception and cnt != num_retries):
    exception = False
    try:
        ser = Serial("/dev/rfcomm1" , baudrate=9600, timeout=0, writeTimeout=0) #ensure non-blocking
    except:
        exception = True
    time.sleep(3)
    cnt = cnt + 1
if cnt == 5 and exception:
    os.system("zenity --warning --title='OBD Dashboard' --text='Cannot connect to OBD bluetooth' --timeout=5 --width=200 2> /dev/null")
    sys.exit()
# Get host and port from config file
server_host = socket.gethostname()
server_port = 8081  # random.randint(10000, 60000)

print('server_host: ' + server_host)

def start_loop(loop, server):
    loop.run_until_complete(server)
    loop.run_forever()

async def notify_users():
    if USERS:  # asyncio.wait doesn't accept an empty list
        await asyncio.wait([user.send(received) for user in USERS])

async def register(websocket):
    USERS.add(websocket)
    await notify_users()

async def hello(websocket, path):
    await register(websocket)
    while True:
        #name = await websocket.recv()
        #print(f"< {name}")
        #greeting = f"Hello {name}!"
        await notify_users()
        #await websocket.send(serBuffer)
        time.sleep(0.2)

new_loop = asyncio.new_event_loop()
start_server = websockets.serve(hello, '192.168.0.102', server_port, loop=new_loop)
t = Thread(target=start_loop, args=(new_loop, start_server), daemon=True)
t.start()

p1 = gaugelib.DrawGauge2(
    win,
    max_value=120,
    min_value=-30,
    size=200,
    bg_col='black',
    unit = "Coolant",bg_sel = 2, units = '℃')
p1.grid(row=0,column=0)
p2 = gaugelib.DrawGauge2(
    win,
    max_value=100,
    min_value= 0,
    size=200,
    bg_col='black',
    unit = "Throttle",bg_sel = 2, units = '%')
p2.grid(row=0,column=1)

p3 = gaugelib.DrawGauge2(
    win,
    max_value=16.0,
    min_value= 0.0,
    size=200,
    bg_col='black',
    unit = "Battery",bg_sel = 2, units = 'V')
p3.grid(row=0,column=2)
p6 = gaugelib.DrawGauge2(
    win,
    max_value=5000,
    min_value= 0,
    size=230,
    bg_col='black',
    unit = "RPM",bg_sel = 2)
p6.grid(row=1,column=0, padx=10)
p4 = gaugelib.DrawGauge2(
    win,
    max_value=3000.0,
    min_value=0.0,
    size=230,
    bg_col='black',
    unit = "MAF",bg_sel = 2, units = 'mg/h')
p4.grid(row=1,column=1)
p5 = gaugelib.DrawGauge2(
    win,
    max_value=2000.0,
    min_value= 0.0,
    size=230,
    bg_col='black',
    unit = "Boost",bg_sel = 2, units = 'mbar')
p5.grid(row=1,column=2)

myFont = Font(size=20)
B = tk.Button(win,text ="X",bg='black', fg='white', highlightthickness=0,borderwidth=0, command=quit)
B['font'] = myFont
B.grid(row=2,column=0)
B.config(width=3, height=2)

var = StringVar()
label = tk.Label(win, textvariable=var, relief=RAISED, bg='black', bd=0, anchor=CENTER, fg='#fff', font=("Arial", 25))
label.grid(row=2,column=1)

var2 = StringVar()
label2 = tk.Label(win, textvariable=var2, relief=RAISED, bg='black', bd=0, anchor=CENTER, fg='#fff', font=("Arial", 25))
label2.grid(row=2,column=2)

def quit():
    t.stop()
    win.destroy()

def readSerial():
    while True:
        c = ser.read() # attempt to read a character from Serial
        #was anything read?
        if len(c) == 0:
            break
        try:
            c=unicode(c, 'utf-8')
        except: break
        # get the buffer from outside of this function
        global serBuffer
        global received
        # check if character is a delimeter
        if c == '\r':
            c = '' # don't want returns. chuck it
            
        if c == '\n':
            serBuffer += "\n" # add the newline to the buffer      
            #add the line to the TOP of the log
            if serBuffer[0] == "{":
                try:
                    received = serBuffer
                    parsed = json.loads(serBuffer)
                    p1.set_value(int(parsed["coolant"]))
                    p2.set_value(int(parsed["pedal_pos"]), 16)
                    p3.set_value(float(parsed["battery"]))
                    p4.set_value(float(parsed["maf"]),14)
                    p5.set_value(float(parsed["boost"]),14)
                    p6.set_value(float(parsed["rpm"]),15)
                    var.set("%s km/h" % parsed['v_speed']) 
                    var2.set("%s %%" % parsed['g_load'])
                except: serBuffer = ""
            serBuffer = ""
        else:
            serBuffer += c
    
    win.after(1, readSerial) # check serial again soon
# after initializing serial, an arduino may need a bit of time to reset
win.after(100, readSerial)
mainloop()
