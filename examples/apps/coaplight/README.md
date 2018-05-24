# OpenThread COAP LIGHT Example

This example application demonstrates a minimal OpenThread application 
that that implements a simple Dimmer Light CoAP interface.
The Dimmer light has 256 levels (0-255) and exposes the following commands:
 - GET light, which replies with a json containing the values of the  
   dimmer light properties
 - PUT light/down, decrements by 'step' the current level of the light
 - PUT light/up, increments by 'step' the current level of the light
 - PUT light/toggle, that moves the level from 'toggleLevel' to 0 and 
   viceversa
 - PUT light/set, this command is use to set new values for the light 
   properties 'step' and 'toogleLevel', the CoAP request must include 
   payload with a simple json similar to this:
      '{"step":50,"toggleLevel":120}'
The example application also includes functionality to simulate a switch
bevavior by periodically toogleing a light. This bevahior can be 
activated/deactivited by a custom CLI command called 'switchsim'.

This example application is based on the CLI application and adds more 
functionality, this means that the user can use all the available CLI 
commands to create, join, configure the network.

In addition, two more CoAP endpoints are included for demostration purposes:
 - GET ping, simply replies with pong, the reply comes in a form of a json 
   payload '{"res":"pong"}'
 - GET ident, replies with a json payload containing the eui64 and the 
   mesh local ip addr of the node, the json payload has the following form:
      '{"eui":"0615aae900124b00","ipaddr":"fdde:ad00:beef:0:558:f56b:d688:799"}'


NOTE: The application uses a opensource (MIT license) json parser library, the
link of the library is https://github.com/zserge/jsmn, specifically from commit
732d283ee9a2e5c34c52af0e044850576888ab09. The library consists of the files
jsmn.h and jsmn.c, and has minimal dependencies. Minor modifications were made 
the source code in order for it to pass the Openthread TravisCI tests.

## 1. Build

```bash
$ cd <path-to-openthread>
$ ./bootstrap
$ make -f examples/Makefile-posix JOINER=1 COAP=1 DEBUG=1 DEBUG_UART=1 DEBUG_UART_LOG=1 FULL_LOGS=1
```

## 2. Start node 1 (will play role of a light)

Spawn the process:

```bash
$ cd <path-to-openthread>/output/<platform>/bin
$ ./ot-coaplight-ftd 1
```

Set the PAN ID:

```bash
> panid 0x1234
```

Bring up the IPv6 interface:

```bash
> ifconfig up
Done
```

Start Thread protocol operation:

```bash
> thread start
Done
```

Wait a few seconds and verify that the device has become a Thread Leader:

```bash
> state
leader
Done
```

View IPv6 addresses assigned to Node 1's Thread interface:

```bash
> ipaddr
fdde:ad00:beef:0:0:ff:fe00:0
fdde:ad00:beef:0:558:f56b:d688:799
fe80:0:0:0:f3d9:2a82:c8d8:fe43
Done
```

## 2. Start node 2 (will play role of a switch)

Spawn the process:

```bash
$ cd <path-to-openthread>/output/<platform>/bin
$ ./ot-coaplight-ftd 2
```

Set the PAN ID:

```bash
> panid 0x1234
```

Bring up the IPv6 interface:

```bash
> ifconfig up
Done
```

Start Thread protocol operation:

```bash
> thread start
Done
```

Wait a few seconds and verify that the device has become a Thread Router:

```bash
> state
router
Done
```

## 3. OPTIONAL Ping Node 2 from Node 1

```bash
> ping fdde:ad00:beef:0:558:f56b:d688:799
16 bytes from fdde:ad00:beef:0:558:f56b:d688:799: icmp_seq=1 hlim=64
```

## 4. Node 2 queries Node 1 initial values by sending coap message GET light
```bash
> coap get fdde:ad00:beef:0:558:f56b:d688:799 light con
Sending coap request: Done
Received coap response with payload: 7b226c6576656c223a20302c2022746f67676c654c6576656c223a203235352c202273746570223a203235357d
```

## 5. Node 2 configures light by sending coap message PUT light/set to Node 1
## Dimmer light (0-255) is configured to move up/down using steps of 50, and when toggled light will go from 0-125 and viceversa
```bash
> coap put fdde:ad00:beef:0:558:f56b:d688:799 light/set con {"step":50,"toggleLevel":125} json
Sending coap request: Done
Received coap response with payload: 7b22737461747573223a224f6b22202c202273746570223a3530202c2022746f67676c654c6576656c223a3132357d
```

## 6. Node 2 sends a coap message PUT light/up to Node 1 to increment the level of the dimmer light
```bash
> coap put fdde:ad00:beef:0:558:f56b:d688:799 light/up con
Sending coap request: Done
Received coap response with payload: 7b22737461747573223a224f6b222c20226c6576656c223a35307d
```

## 7. Node 2 sends a coap message PUT light/down to Node 1 to decrement the level of the dimmer light
```bash
> coap put fdde:ad00:beef:0:558:f56b:d688:799 light/down con
Sending coap request: Done
Received coap response with payload: 7b22737461747573223a224f6b222c20226c6576656c223a307d
```

## 8. Node 2 sends a coap message PUT light/toggle to Node 1 in order to toggle light from on/off states or viceversa
```bash
> coap put fdde:ad00:beef:0:558:f56b:d688:799 light/toggle con
Sending coap request: Done
Received coap response with payload: 7b22737461747573223a224f6b222c20226c6576656c223a3132357d
```

## 9. Starts a simple simulated switch functionality on Node 2, specifiyng the Node 1 ipaddr
```bash
> switchsim start fdde:ad00:beef:0:558:f56b:d688:799
SUCCESS, starting switchsim app on ip = fdde:ad00:beef:0:558:f56b:d688:799
```

## 10. Stop the simple simulated switch fucntionality on Node 2
```bash
> switchsim stop
SUCCESS, stopping switchsim app
```

## 11. Node 2 sends a coap message GET ping to Node 1
```bash
> coap get fdde:ad00:beef:0:558:f56b:d688:799 ping con
Sending coap request: Done
Received coap response with payload: 7b22726573223a22706f6e67227d
```

## 12. Node 2 sends a coap message GET ident to Node 1
```bash
> coap get fdde:ad00:beef:0:558:f56b:d688:799 ident con
Sending coap request: Done
Received coap response with payload: 7b22657569223a2231383a62343a33303a303a303a303a303a31222c22697061646472223a22666464653a616430303a626565663a303a346365353a666133303a313636373a63313164227d
```

## 13. Need more CLI commands?

You may note that the example above did not include any network parameter configuration, such as the IEEE 802.15.4 PAN ID or the Thread Master Key. OpenThread currently implements default values for network parameters, however, you may use the CLI to change network parameters, other configurations, and perform other operations.

See the [OpenThread CLI Reference README.md](../../../src/cli/README.md) to explore more.
