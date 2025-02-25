# PyProxy - Very Simple TCP/UDP Proxy - Used for investigation the Guacamole hardware-based solution

With many thanks to rsc-dev for the Very Simple TCP/UPS Proxy that can be found on [PyProxy](https://github.com/rsc-dev/pyproxy/blob/master/LICENSE). I used this software as bases for the first test to see how to check the Guacamole protocol and to see how Guacamole implements the protocol. It implements the first simple protocol checker that checks the LENGTH.VALUE. This software will explore more how the protocol could be checked more for a final hardware-based solution.

## About
PyProxy is a very simple TCP/UDP pure Python proxy.
It can be easily extended for custom data handling logic.

## Usage
```cmd
>pyproxy.py -h
usage: pyproxy.py [-h] (--tcp | --udp) -s SRC -d DST [-q | -v]

TCP/UPD proxy.

optional arguments:
  -h, --help         show this help message and exit
  --tcp              TCP proxy
  --udp              UDP proxy
  -s SRC, --src SRC  Source IP and port, i.e.: 127.0.0.1:8000
  -d DST, --dst DST  Destination IP and port, i.e.: 127.0.0.1:8888
  -q, --quiet        Be quiet
  -v, --verbose      Be loud
```

### Custom data handlers
Proxy uses two data handlers for incoming and outgoing data.
By default both are set to identity function. 
```python
LOCAL_DATA_HANDLER = lambda x:x
REMOTE_DATA_HANDLER = lambda x:x
```

One can easily implement own rules. It might be very useful while testing network applications.
```python
def custom_data_handler(data):
    # code goes here
    pass
    
LOCAL_DATA_HANDLER = custom_data_handler
REMOTE_DATA_HANDLER = lambda x:'constant_value'
```

## Example for Guacamole proxy
./pyproxy.py --tcp -s 10.0.2.15:4822 -d localhost:4823

In this case the real guacd listens to 4823 and the proxy listens to 4822 the default port of guacd.

## License
Code is released under [MIT license](https://github.com/macsnoeren/guacamole-datadiode/blob/main/pyproxy/LICENSE).
