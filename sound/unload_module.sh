#!/bin/sh

module="snd_cs4624"

sudo pulseaudio --kill
sudo killall pulseaudio
if lsmod|grep snd_cs4624 > /dev/null;then
	echo 'rmmod snd_cs4624'
	sudo rmmod snd_cs4624
fi
