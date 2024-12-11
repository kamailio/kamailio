sip-router-presence_dfks-module
===============================

sip-router/kamailio module for handling as-feature-event presence messages

This module provide support for 'Device FeatureKey Synchronization', as described in
http://community.polycom.com/polycom/attachments/polycom/VoIP/2233/1/DeviceFeatureKeySynchronizationFD.pdf
This 'protocol' was developed at Broadsoft, and is used in Linksys/Cisco and Polycom phones and probably some other too.

It allows to set up features like dnd (No Not Disturb) and call forwarding using phone interface and have that status
updated on aplication server/ proxy or otherway, set those features on aplication server using some gui, and have this data
provisioned on phone.
For Shared Line Appreance application this let You share status on all phones.

Overview
------

This module is work in progress, it does not provide much functionality yet.
Module reuses as much as possible from existing sip-router/kamailio modules, expecially from presence and pua.
Basicaly module translates body of incoming SUBSCRIBE messages and and sends them as PUBLISH to presence framework,
which would then propagate NOTIFY messages to watchers.
Currently module uses pua module to send PUBLISH, but that dependency is about to be dropped.
In order to have this working currently pua module needs to be patched.
Side effect of this is that it is possible to set phone status using pua_mi interface.


```sh
kamctl fifo pua_publish sip:1000@10.10.99.254 3600 as-feature-event application/x-as-feature-event+xml . . . "<?xml version='1.0' encoding='ISO-8859-1'?><DoNotDisturbEvent><device>1000</device><doNotDisturbOn>true</doNotDisturbOn></DoNotDisturbEvent>"

kamctl fifo pua_publish sip:1000@10.10.99.254 3600 as-feature-event application/x-as-feature-event+xml . . . "<?xml version='1.0' encoding='ISO-8859-1'?><ForwardingEvent><device><notKnown/></device><forwardingType>forwardImmediate</forwardingType><forwardStatus>true</forwardStatus><forwardTo>1234</forwardTo></ForwardingEvent>"
```
This will be dropped too, a set of internal mi functions is planned and a script function that could preload presence server with phone status using some avp's. Registration route would be a good place for this.
