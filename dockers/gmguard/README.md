# gmguard docker

Create the docker image for gmguard application that communicate with the gmlbroker
and gcdbroker to perform protocol filtering. Note that it is important to set the
correct build context. This is the root of this project. If you want to build the 
docker image, please use the syntax below

```docker build -f Dockerfile -t gmguard ../../```

If you want to make sure that you build the full images, please
use the ```--no-cache``` option.

```docker build --no-cache -f Dockerfile -t gmguard ../../```
