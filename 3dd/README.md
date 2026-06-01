When running the Guacamole container for the first time, initialize the postgres db with the command:

```
docker run --rm guacamole/guacamole:1.6.0 /opt/guacamole/bin/initdb.sh --postgresql | \
    docker exec -i guac_postgres psql -U guacamole_user -d guacamole_db -f -
```
