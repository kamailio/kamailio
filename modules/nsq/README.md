# NSQ Module for Kamailio

## 1. Overview

NSQ is a realtime distributed messaging platform designed to operate at scale, handling billions of messages per day.
It promotes distributed and decentralized topologies without single points of failure, enabling fault tolerance and high availability coupled with a reliable message delivery guarantee.

From a high-level, the purpose of the module might be for things like:

* Integrate to an application to make real-time routing decisions (instead of using, say, a SQL database)
* Provide a real-time integration into your program, instead of your database, so you can overlay additional logic in your preferred language while also utilizing a message bus
* Utilize messaging to have a distributed messaging layer, such that machines processing requests/responses/events can go up/down or share the workload and your Kamailio node will still be happy

Supported operations are:

* publish json payloads to nsq topics
* publish json payloads to nsq topics and wait for correlated response message
* subscribe to an nsq topic and channel and handle events from that channel

The NSQ module also has support to publish updates to presence module thru the nsq_pua_publish function.

This module is heavily based on the Kazoo module from 2600hz.

## 2. How it works

The module works with a main forked process that does the communication with your nsq system for issuing publishes, waiting for replies, and consuming messages. When it consumes a message it defers the process to a worker thread so it doesn't block the main process (uses libev).

### 2.1. Event Routes

The worker process issues an event-route where we can act on the received payload. The name of the event-route is composed by values extracted from the payload.

NSQ module will try to execute the event route from most significant to less significant. define the event route like event_route[nsq:consumer-event[-payload_key_value[-payload_subkey_value]]]

#### Example
```
...
modparam("nsq", "consumer_event_key", "Event-Type")
modparam("nsq", "consumer_event_subkey", "Event-Name")
...

event_route[nsq:consumer-event-presence-update]
{
# presence is the value extracted from Event-Type field in json payload
# update is the value extracted from Event-Name field in json payload
xlog("L_INFO", "received $(kzE{kz.json,Event-Package}) update for $(kzE{kz.json,From})");
...
}

event_route[nsq:consumer-event-presence]
{
# presence is the value extracted from Event-Type field in json payload
xlog("L_INFO", "received $(kzE{kz.json,Event-Package}) update for $(kzE{kz.json,From})");
...
}

event_route[nsq:consumer-event]
{
# this event route is executed if we can't find the previous
}

```

### 2.2 Acknowledge Messages

Consumed messages have the option of being acknowledged in two ways:

* immediately when received
* after processing by the worker


## 3. Dependencies

### 3.1. Kamailio Modules

The following modules must be loaded before this module:

* none

### 3.2. External Libraries or Applications

* libev
* libjson
* libuuid

## 4. Parameters

### 4.1. NSQ Client

#### 4.1.1. nsqd_address(str)

The http address of the nsqd to post messages to

_Default value is Null. You must set this parameter value for the module to work_

__Example__
```
...
modparam("nsq", "nsqd_address", "127.0.0.1:4151")
...
```

#### 4.1.2. lookupd_address(str)

The http address of the nsq lookupd servers ( _comma seperated_ )

_Default value is Null. You must set this parameter value for the module to work_

__Example__
```
...
modparam("nsq", "lookupd_address", "10.10.10.1:4161,10.10.10.2:4161")
...
```

#### 4.1.3. consumer_topic(str)

The topic to listen on for inbound events

__Example__
```
...
modparam("nsq", "consumer_topic", "kamailio")
...
```

#### 4.1.4. consumer_channel(str)

The channel to listen on for inbound events

__Example__
```
...
modparam("nsq", "consumer_channel", "sip-proxy-01")
...
```

#### 4.1.5. consumer_event_key(str)

The JSON property name to watch for for handling event_routes

_Default value is "Event-Type"_

__Example__
```
...
modparam("nsq", "consumer_event_key", "Type")
...
```


#### 4.1.6. consumer_event_subkey(str)

The JSON property sub key name to watch for for handling event_routes

_Default value is "Event-Name"_

__Example__
```
...
modparam("nsq", "consumer_event_subkey", "Name")
...
```


#### 4.1.7. max_in_flight(int)

Number of messages the nsq client will handle concurrently

_Default value is 100_

__Example__
```
...
modparam("nsq", "max_in_flight", 5)
...
```

#### 4.1.7. query_timeout(int)

Number of seconds until timeout for query requests

_Default value is 2_

__Example__
```
...
modparam("nsq", "query_timeout", 5)
...
```

### 4.2. Presence Related

#### 4.2.1. db_url(str)

The database for the presentity table.

If set, the nsq_ppua_publish function will update the presentity status in the database.

_Default value is “NULL”._

__Example__
```
...
modparam("nsq", "db_url", "mysql://kamailio:kamailiorw@localhost/kamailio")
...
```

#### 4.2.2. presentity_table(str)

The name of the presentity table in the database.

_Default value is “presentity”._

__Example__
```
...
modparam("nsq", "presentity_table", "my_presentity_table")
...
```


## 5. Functions

### 5.1. nsq related

#### 5.1.1. nsq_publish(topic, json_payload)

The function publishes a json payload to the nsq topic passed in.

This function can be used from ANY ROUTE.

__Example__
```
...
$var(nsq_payload_request) = "{'Event-Type' : 'directory', 'Event-Name' : 'reg_success', 'Contact' : '" + $var(fs_contact) + "', 'Call-ID' : '" + $ci + "', 'Realm' : '" + $fd +"', 'Username' : '" + $fU + "', 'From-User' : '" + $fU + "', 'From-Host' : '" + $fd + "', 'To-User' : '" + $tU +"', 'To-Host' : '" + $td + "', 'User-Agent' : '" + $ua +"' ," + $var(register_contants)+ " }";
nsq_publish("registrations", $var(nsq_payload_request));
...
```

#### 5.1.2. nsq_query(topic, json_payload [, target_var])

The function publishes a json payload to nsq, waits for a correlated message and puts the result in target_var. target_var is optional as the function also puts the result in pseudo-variable $nqR.

This function can be used from ANY ROUTE.

__Example__
```
...
$var(nsq_payload_request) = "{'Event-Category' : 'call_event' , 'Event-Name' : 'query_user_channels_req', 'Realm' : '" + $fd + "', 'Username' : '" + $fU + "', 'Active-Only' : false }";
nsq_encode("$ci", "$var(callid_encoded)");
if(nsq_query("callevt", $var(nsq_payload_request), "$var(nsq_result)")) {
   nsq_json("$var(nsq_result)", "Channels[0].switch_url", "$du");
   if($du != $null) {
       xlog("L_INFO", "$ci|log|user channels found redirecting call to $du");
       return;
   }
}
...
```

### 5.2. presence related

#### 5.2.1. nsq_pua_publish(json_payload)

The function build presentity state from json_payload and updates presentity table.

This function can be used from ANY ROUTE.

__Example__
```
...
event_route[nsq:consumer-event-presence-update]
{
    xlog("L_INFO", "received $(nqE{nq.json,Event-Package}) update for $(nqE{nq.json,From})");
    nsq_pua_publish($kzE);
    pres_refresh_watchers("$(nqE{nq.json,From})", "$(nqE{nq.json,Event-Package})", 1);
}
...
```

### 5.3. other

### 5.3. presence related

#### 5.3.1. nsq_encode(to_encode, target_var)

The function encodes the 1st parameter to JSON and puts the result in the 2nd parameter.

This function can be used from ANY ROUTE.

__Example__
```
...
event_route[nsq:consumer-event-presence-update]
{
    xlog("L_INFO", "received $(nqE{nq.json,Event-Package}) update for $(nqE{nq.json,From})");
    nsq_pua_publish($nqE);
    pres_refresh_watchers("$(nqE{nq.json,From})", "$(nqE{nq.json,Event-Package})", 1);
}
...
```



## 6. Exported pseudo-variables

## 7. Transformations





