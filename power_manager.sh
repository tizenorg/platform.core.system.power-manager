#!/bin/sh

if [ ! -e /opt/etc/.hib_capturing ]; then
	/usr/bin/pmctrl start
fi
