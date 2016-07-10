#!/bin/bash

sudo apt-get install tightvncserver -y
tightvncserver
service lightdm stop
sed -i -e '$i \vncserver :1 -geometry 800x600 -depth 24 -dpi 96\n' /etc/rc.local
sed -i -e '$i \/opt/auto-mated/automated-metric.py &\n' /etc/rc.local
{ crontab -l -u root; echo '* * * * * /opt/auto-mated/bluetooth_search.sh'; } | crontab -u root -
sudo apt-get install git-core -y
sudo apt-get update -y
sudo apt-get upgrade -y
apt-get install --no-install-recommends bluetooth htop python-dev python-pip -y
service bluetooth start
service bluetooth status
hcitool scan
cd /opt
git clone git://git.drogon.net/wiringPi
cd wiringPi
git pull origin
./build
cd /opt/auto-mated/pcd8544/cpu_show/
chmod +x compile.sh
./compile.sh
pip install obd psutil netifaces
