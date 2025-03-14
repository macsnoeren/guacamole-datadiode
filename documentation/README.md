# Documentation Guacamole Data-Diode
You can find here some usefull documentation to start with the Guacamole Data-Diode.

## Get Guacamole up and running
We use docker to setup a Guacamole installation. See for more information [Guacamole on Docker Hub](https://hub.docker.com/r/guacamole/guacamole). First we spin up a MySQL server.
```
$ docker run --name some-mysql -e MYSQL_ROOT_PASSWORD=mysqlpassword -d mysql:latest
```

While this is an empty database, we need officially create a database named 'guacamole_db' and a user. However, we will here use the root password, which you should never use in production environments! The next step is to populate the database. First get the ```initdb.sql``` file.
```
$ docker run --rm guacamole/guacamole /opt/guacamole/bin/initdb.sh --mysql > initdb.sql
```

Add on top of this file:
```
CREATE DATABASE guacamole_db;
USE guacamole_db;
```

Populate the database using the ```initdb.sql``` file:
```
$ docker exec -i some-mysql sh -c 'exec mysql -uroot -p"mysqlpassword"' < initdb.sql
 ```

Now we are good to go and can start the Guacamole web server and the guacd deamon:
```
$ docker run --name some-guacd -d -p 4822:4822 guacamole/guacd
$ docker run --name some-guacamole --link some-mysql:mysql -e MYSQL_DATABASE=guacamole_db -e MYSQL_USER=root -e MYSQL_PASSWORD=mysqlpassword -e GUACD_HOSTNAME=localhost -e GUACD_PORT=4822 -d -p 8080:8080 guacamole/guacamole
```

You can find your Guacamole on ```localhost:8080/guacamole```. If you somehow need to restart these dockers, please use
```
$ docker restart some-guacd
$ docker restart some-mysql
$ docker restart some-guacamole
```

If you need somehow access to these dockers on command-line, please use
```
$ docker exec -it some-mysql bash
```

## Testing the Guacamole Data-Diode locally
In order to test the data-diode software locally, we need to arrange some ports. First we start the proxies. The gmserver needs to send its data over the sending data-diode proxy, this can be done by:
```
$ gmproxyin &
$ gmproxyout &
```

Next we need the receiving data-diode proxy, so gmserver is able to received the data, this can be done by
```
$ gmproxyout -p -20001 -i 40001 &
$ gmproxyin -p 10001 -o 40001 &
```

The reason that we need different ports, is that the "normal" ports have been taken by the sending data-diode already. The last step is spinning up the gmserver with which the Guacamole web server will connect to and the gmclient that connects to the guacd.

```
$ gmclient -o 10001 -p 4823&
$ gmserver -i 20001 &
```

That are al lot of applications!


