Project with a Lora "hub" on an RPi zero and RFM95W and a battery powered temp sensor on Arduino Pro mini and RFM95W. 
Arduino Pro Mini is run on battery. Led and Voltage regulator is removed. The module is put to sleep for 15 mins between transmits and draws ~5µA during that time.

Pinout



```
| RFM95W      | Pro Mini |
| ----------- | -------- |
| VCC         | VCC      |
| GND         | GND      |
| SCK         | D13      |
| MISO        | D12      |
| MOSI        | D11      |
| NSS         | D10      |
| RESET       | D9       |
| DIO0        | D2       |
| ANT         | antenn   |
|             |          |
| DS18B20     | Pro Mini |
| VDD (röd)   | D7       |
| GND (svart) | GND      |
| DATA (gul)  | D4       |
	
4.7 kΩ pullup between DATA and D7	
	
| RPi GPIO     | RFM95W |
| ------------ | ------ |
| GPIO 7 (CE1) | NSS/CS |
| GPIO 25      | RESET  |
| GPIO 22      | DIO0   |
| GPIO 11      | SCK    |
| GPIO 10      | MOSI   |
| GPIO 9       | MISO   |
| 3.3V         | VCC    |
| GND          | GND    |
```

```
pip install lgpio gpiozero
pip install flask
```

Run on RPi to autostart:

/etc/systemd/system/hub-lora.service:
```
[Unit]
Description=LoRa Hub mottagare
After=network.target

[Service]
User=<USER>
WorkingDirectory=/home/<USER>
ExecStart=/home/<USER>/env/bin/python3 /home/<USER>/hub_lorarf.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

/etc/systemd/system/hub-web.service:
```
[Unit]
Description=LoRa Hub webbserver
After=network.target

[Service]
User=martinb
WorkingDirectory=/home/<USER>
ExecStart=/home/<USER>/env/bin/python3 /home/<USER>/hub_web.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

```
sudo systemctl daemon-reload
sudo systemctl enable hub-lora hub-web
sudo systemctl start hub-lora hub-web
``

Check
```
sudo systemctl status hub-lora
sudo systemctl status hub-web
```

Website reachable at http://<pi-ip>:5000
