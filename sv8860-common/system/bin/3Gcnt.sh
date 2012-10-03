#!/bin/sh
#/system/bin/run_3G /system/bin/pppd /dev/ttyUSB0 115200 crtscts connect '/system/bin/chat -v -f /system/etc/chat-isp || /system/bin/chat -f /system/etc/chat-isp-pin' noauth ipcp-accept-local ipcp-accept-remote persist modem defaultroute nodetach maxfail 1

/system/bin/pppd usepeerdns /dev/ttyUSB0 115200 crtscts connect '/system/bin/chat -v -f /system/etc/chat-isp || /system/bin/chat -f /system/etc/chat-isp-pin' noauth ipcp-accept-local ipcp-accept-remote persist modem defaultroute nodetach maxfail 1&

