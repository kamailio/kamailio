#!/bin/bash

function notify {
	sipp -sf xml/notify.xml $ADDITIONAL_PARAMETERS -m 8 -bg
	if [[ $? != 99 ]]; then
		echo sipp notify went wrong!!
	fi
}
