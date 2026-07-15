# gmlbroker docker

Create the docker image for gmlbroker application that communicate with the Guacomole
server part of the Guacamole project. Note that it is important to set the correct build
context. This is the root of this project. If you want to build the docker image,
please use the syntax below

```docker build -f Dockerfile -t gmlbroker ../../```

If you want to make sure that you build the full images, please
use the ```--no-cache``` option.

```docker build --no-cache -f Dockerfile -t gmlbroker ../../```
