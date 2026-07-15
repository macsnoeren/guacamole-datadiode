# test-guacamole docker

This is the normal Apache Guacamole webapp image (`guacamole/guacamole:1.6.0`) with one small extra startup step. The extra step lets the webapp trust the self-signed certificate that the `gmlbroker` makes, so the connection between the Guacamole webapp and the `gmlbroker` can run over TLS.

## Why this image exists

The `gmlbroker` can encrypt the link to the Guacamole server with TLS. When you turn that on (`GMLBROKER_TLS=1`), the `gmlbroker` makes its own self-signed certificate and writes it in the `/tls` directory. But a self-signed certificate is not trusted by the Java machine of Guacamole, so the webapp would refuse the connection.

The normal guacamole image can talk TLS (`GUACD_SSL=true`), but it has no way to trust such a self-signed certificate. You have to import that certificate in a Java truststore and point the JVM to it. This image does exactly that for you at startup, so you do not have to do it by hand.

## How it works

The upstream guacamole image runs every script in `/opt/guacamole/entrypoint.d/` at startup, in alphabetical order. This image adds one script there: `800-gmlbroker-tls.sh`. The `800` makes it run after the normal config steps and before Tomcat starts.

The script does this, but only when `GMLBROKER_TLS_CERT` is set:

1. It waits for the `gmlbroker` certificate to show up (the `gmlbroker` makes it on its own start, which can be a bit later).
2. It builds a Java truststore. It starts from the JDK its own `cacerts` (so the normal public CAs still work) and adds the `gmlbroker` certificate to it.
3. It sets `JAVA_OPTS` so the JVM uses that truststore.

The truststore is built inside the container and it is rebuilt on every start. So when the `gmlbroker` made a new certificate, you just restart this container and it trusts the new one again.

When `GMLBROKER_TLS_CERT` is NOT set, the script does nothing and the image behaves exactly like the normal guacamole image.

## Environment variables (the extra ones)

| Variable | Default | Description |
|----------|---------|-------------|
| `GMLBROKER_TLS_CERT`            | *(not set)* | path to the `gmlbroker` certificate to trust. Set it (for example `/tls/cert.pem`) to turn on the trust step. Leave it out for plaintext. |
| `GMLBROKER_TRUSTSTORE_PASSWORD` | `changeit`  | password for the truststore that is built |

Next to these you also set the normal guacamole variable `GUACD_SSL=true`, so the webapp actually talks TLS to the `gmlbroker`. All the other guacamole variables (`POSTGRESQL_*`, `GUACD_HOSTNAME`, `GUACD_PORT`, ...) work the same as in the normal image.

## How to use it

You normally do not run this image by hand, it is part of the compose files. In the low-side compose files (`compose-1-node.yml`, `compose-2-node-low.yml`, `compose-3-node-low.yml`) the `guacamole` service already uses this image. To turn on TLS you do two things that belong together:

1. On the `gmlbroker` service: set `GMLBROKER_TLS: "1"`.
2. On the `guacamole` service: set `GUACD_SSL: "true"` and `GMLBROKER_TLS_CERT: /tls/cert.pem`, and make sure the `./tls` directory is mounted (it already is in the compose files).

Both machines share the same `./tls` directory, so the certificate that the `gmlbroker` writes is read by this image. See the README in `dockers/docker-compose` for the full TLS story, also for the case where your Guacamole runs on an other machine.

## Build

The build context is this folder. If you build it by hand:

```
docker build -f Dockerfile -t test-guacamole .
```
