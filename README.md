# kwp1281-bt-gauges
Simple way of displaying live engine data that was pulled from your VAG vehicle trough kwp1281 protocol
Features: 
- 6 gauges + 2 labels (car speed and genrator load)
- websocket server running in the back so you can display the data on different devices/apps
- The ardu-dongle bundles the data as json so it made it easily to manage

Inspirate by: http://grauonline.de/wordpress/?p=74
For hardware connection please visit the url above. The major difirence here is that instead of using SERIAL to USB convertor I attached HC-06 bt module on the hardware RX/TX
For the python app:
Follow instruction how to add it as external app in the OpanAuto forum
You may have to install websockets if you don`t already have it on the Rpi: 

    pip install websockets
