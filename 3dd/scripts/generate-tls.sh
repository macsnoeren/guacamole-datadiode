#!/usr/bin/env bash
#
# Generate the TLS material for the gmlbroker <-> Guacamole web server leg into
# the given output directory:
#
#   key.pem / cert.pem  -> mounted into gmlbroker  (GMLBROKER_TLS server cert)
#   truststore.jks      -> mounted into the web app (so its JVM trusts that cert)
#
# The truststore is a copy of the JDK's own cacerts with gmlbroker's cert added,
# so the web app trusts gmlbroker while still validating public CAs. keytool runs
# inside the guacamole image, so the host needs no JDK.
#
# Usage: scripts/generate-tls.sh <output-dir>
# Used by the per-topology docker/<topology>/generate-tls.sh wrappers.
set -euo pipefail

OUT="${1:?usage: generate-tls.sh <output-dir>}"
GUAC_IMAGE="guacamole/guacamole:1.6.0"
STOREPASS="changeit"

mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)" # absolutize for the docker -v mount

# Self-signed server certificate for gmlbroker. Guacamole's SSLGuacamoleSocket
# uses SSLSocketFactory.getDefault() with no endpoint-identification algorithm,
# so it validates the chain but NOT the hostname — CN/SAN are cosmetic, set to
# gmlbroker for clarity.
echo ">> generating gmlbroker key + self-signed cert in $OUT"
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$OUT/key.pem" -out "$OUT/cert.pem" \
  -days 3650 -subj "/CN=gmlbroker" \
  -addext "subjectAltName=DNS:gmlbroker"
chmod 600 "$OUT/key.pem"

# Truststore for the web app: a copy of the JDK cacerts (so public CAs still
# validate) with gmlbroker's cert added. --user makes the output files owned by
# the invoking user rather than root.
echo ">> building truststore.jks (cacerts + gmlbroker cert) via $GUAC_IMAGE"
docker run --rm --user "$(id -u):$(id -g)" -v "$OUT:/out" \
  --entrypoint bash "$GUAC_IMAGE" -c "
    set -e
    cp /opt/java/openjdk/lib/security/cacerts /out/truststore.jks
    keytool -importcert -noprompt -alias gmlbroker \
      -file /out/cert.pem -keystore /out/truststore.jks -storepass '$STOREPASS'
  "

echo ">> TLS material written to $OUT"
