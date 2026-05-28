When running the Guacamole container for the first time, initialize the postgres db with the command:

```
docker run --rm guacamole/guacamole /opt/guacamole/bin/initdb.sh --postgresql | \
    docker exec -i some-postgresql psql -U guacamole_user -d guacamole_db -f -
```
