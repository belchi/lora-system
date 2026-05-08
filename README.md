Project with a Lora "hub" on an RPi zero and RFM95W and a battery powered temp sensor on Arduino Pro mini and RFM95W. 
Arduino Pro Mini is run on battery. Led and Voltage regulator is removed. The module is put to sleep for 15 mins between transmits and draws ~5µA during that time.

Run on RPi to autostart:

```
pip install lgpio gpiozero
pip install flask
```

/etc/systemd/system/hub-lora.service:
```
[Unit]
Description=LoRa Hub mottagare
After=network.target

[Service]
User=martinb
WorkingDirectory=/home/martinb
ExecStart=/home/martinb/env/bin/python3 /home/martinb/hub_lorarf.py
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
WorkingDirectory=/home/martinb
ExecStart=/home/martinb/env/bin/python3 /home/martinb/hub_web.py
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
