#!/usr/bin/env bash
#
# create-flatcar-files.sh
#
# Bouwt een complete build/ directory met alle bestanden die nodig zijn om
# een bare-metal machine te installeren of te updaten met Flatcar +
# de geconfigureerde Docker applicaties.
#
# Lees flatcar.conf voor de instellingen.
#
# Vereisten op het build-systeem:
#   - bash, curl, sha256sum, awk, sed
#   - docker (voor het bouwen/pullen/opslaan van images en het draaien van butane)
#
# Output: zie ${BUILD_DIR} na succesvolle run.

set -euo pipefail

# ------------------------------------------------------------
# Helpers
# ------------------------------------------------------------

SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

log()  { printf '\033[1;34m[+] %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m[!] %s\033[0m\n' "$*" >&2; }
err()  { printf '\033[1;31m[x] %s\033[0m\n' "$*" >&2; }
die()  { err "$*"; exit 1; }

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "Commando '$1' niet gevonden in PATH."
}

# ------------------------------------------------------------
# Config laden
# ------------------------------------------------------------

CONFIG_FILE="${1:-${SCRIPT_DIR}/flatcar.conf}"

[[ -f "${CONFIG_FILE}" ]] || die "Config bestand niet gevonden: ${CONFIG_FILE}"

log "Config laden uit ${CONFIG_FILE}"
# shellcheck source=/dev/null
source "${CONFIG_FILE}"

: "${HOSTNAME:?HOSTNAME ontbreekt in config}"
: "${SSH_KEY_FILE:?SSH_KEY_FILE ontbreekt in config}"
: "${INSTALL_DEVICE:?INSTALL_DEVICE ontbreekt in config}"
: "${FLATCAR_CHANNEL:?FLATCAR_CHANNEL ontbreekt in config}"
: "${FLATCAR_VERSION:?FLATCAR_VERSION ontbreekt in config}"
: "${FLATCAR_ARCH:?FLATCAR_ARCH ontbreekt in config}"
: "${BUILD_DIR:?BUILD_DIR ontbreekt in config}"
: "${CACHE_DIR:?CACHE_DIR ontbreekt in config}"

# Maak paden absoluut t.o.v. de config-locatie
CONFIG_DIR="$( cd -- "$( dirname -- "${CONFIG_FILE}" )" &> /dev/null && pwd )"
resolve_path() {
  local p="$1"
  if [[ "${p}" = /* ]]; then
    printf '%s' "${p}"
  else
    printf '%s/%s' "${CONFIG_DIR}" "${p}"
  fi
}

SSH_KEY_FILE="$(resolve_path "${SSH_KEY_FILE}")"
BUILD_DIR="$(resolve_path "${BUILD_DIR}")"
CACHE_DIR="$(resolve_path "${CACHE_DIR}")"

require_cmd curl
require_cmd sha256sum
require_cmd docker
require_cmd awk
require_cmd sed

[[ -f "${SSH_KEY_FILE}" ]] || die "SSH public key niet gevonden: ${SSH_KEY_FILE}
Genereer er een met: ssh-keygen -t ed25519 -a 100 -f \"${SSH_KEY_FILE%.pub}\""

SSH_KEY_CONTENT="$(tr -d '\n\r' < "${SSH_KEY_FILE}")"

# ------------------------------------------------------------
# Build directory structuur
# ------------------------------------------------------------

log "Build directory voorbereiden: ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"
mkdir -p \
  "${BUILD_DIR}/flatcar" \
  "${BUILD_DIR}/docker-images" \
  "${BUILD_DIR}/scripts" \
  "${BUILD_DIR}/updates" \
  "${BUILD_DIR}/checksums"

mkdir -p "${CACHE_DIR}"

# ------------------------------------------------------------
# Flatcar artifacts downloaden (gecached)
# ------------------------------------------------------------

BASE_URL="https://${FLATCAR_CHANNEL}.release.flatcar-linux.net/${FLATCAR_ARCH}/${FLATCAR_VERSION}"

FLATCAR_IMAGE="flatcar_production_image.bin.bz2"
FLATCAR_INSTALLER="flatcar-install"
FLATCAR_UPDATE_PAYLOAD="flatcar_production_update.gz"

download_cached() {
  local url="$1"
  local out="$2"
  if [[ -f "${out}" ]]; then
    log "  cache hit: $(basename "${out}")"
    return 0
  fi
  log "  downloaden: ${url}"
  if curl -fL --retry 3 -o "${out}.part" "${url}"; then
    mv "${out}.part" "${out}"
    return 0
  fi
  rm -f "${out}.part"
  return 1
}

log "Flatcar artifacts ophalen (channel=${FLATCAR_CHANNEL}, versie=${FLATCAR_VERSION}, arch=${FLATCAR_ARCH})"
download_cached "${BASE_URL}/${FLATCAR_IMAGE}"          "${CACHE_DIR}/${FLATCAR_IMAGE}"
download_cached "https://raw.githubusercontent.com/flatcar/init/flatcar-master/bin/flatcar-install" \
                "${CACHE_DIR}/${FLATCAR_INSTALLER}"

HAVE_UPDATE_PAYLOAD=0
if download_cached "${BASE_URL}/${FLATCAR_UPDATE_PAYLOAD}" "${CACHE_DIR}/${FLATCAR_UPDATE_PAYLOAD}"; then
  HAVE_UPDATE_PAYLOAD=1
else
  warn "Update payload niet beschikbaar voor deze versie."
  warn "update-os.sh wordt overgeslagen — bare-metal install en update-app.sh blijven werken."
fi

cp "${CACHE_DIR}/${FLATCAR_IMAGE}"     "${BUILD_DIR}/flatcar/${FLATCAR_IMAGE}"
cp "${CACHE_DIR}/${FLATCAR_INSTALLER}" "${BUILD_DIR}/flatcar/${FLATCAR_INSTALLER}"
chmod +x "${BUILD_DIR}/flatcar/${FLATCAR_INSTALLER}"

if [[ "${HAVE_UPDATE_PAYLOAD}" -eq 1 ]]; then
  cp "${CACHE_DIR}/${FLATCAR_UPDATE_PAYLOAD}" "${BUILD_DIR}/updates/${FLATCAR_UPDATE_PAYLOAD}"
else
  rmdir "${BUILD_DIR}/updates" 2>/dev/null || true
fi

# ------------------------------------------------------------
# Docker images bouwen en pullen
# ------------------------------------------------------------

tar_name_for() {
  # "naam:tag" -> "naam_tag.tar"
  printf '%s.tar' "${1//[:\/]/_}"
}

if [[ "${#DOCKER_BUILD_IMAGES[@]}" -gt 0 ]]; then
  log "Docker images bouwen"
  for entry in "${DOCKER_BUILD_IMAGES[@]}"; do
    image="${entry%%|*}"
    ctx_rel="${entry#*|}"
    ctx="$(resolve_path "${ctx_rel}")"
    [[ -d "${ctx}" ]] || die "Build context bestaat niet: ${ctx}"
    log "  build ${image} <= ${ctx}"
    docker build -t "${image}" "${ctx}"
    out="${BUILD_DIR}/docker-images/$(tar_name_for "${image}")"
    log "  save  ${image} => ${out}"
    docker save "${image}" -o "${out}"
  done
fi

if [[ "${#DOCKER_PULL_IMAGES[@]}" -gt 0 ]]; then
  log "Docker images pullen"
  for image in "${DOCKER_PULL_IMAGES[@]}"; do
    log "  pull  ${image}"
    docker pull "${image}"
    out="${BUILD_DIR}/docker-images/$(tar_name_for "${image}")"
    log "  save  ${image} => ${out}"
    docker save "${image}" -o "${out}"
  done
fi

# ------------------------------------------------------------
# Butane config genereren
# ------------------------------------------------------------

BUTANE_FILE="${BUILD_DIR}/flatcar/config.bu"
IGNITION_FILE="${BUILD_DIR}/flatcar/config.ign"

log "Butane config genereren: ${BUTANE_FILE}"

# Eén bron van waarheid voor de inhoud van een service unit, zodat butane en
# update-app.sh niet uit elkaar lopen.
emit_service_unit_body() {
  local name="$1" image="$2" args="$3"
  cat <<EOF
[Unit]
Description=${name} container
After=docker.service import-images.service
Requires=docker.service import-images.service

[Service]
Restart=always
RestartSec=5
ExecStartPre=-/usr/bin/docker rm -f ${name}
ExecStart=/usr/bin/docker run --name ${name} ${args} ${image}
ExecStop=/usr/bin/docker stop ${name}

[Install]
WantedBy=multi-user.target
EOF
}

{
  cat <<EOF
variant: flatcar
version: 1.0.0

passwd:
  users:
    - name: core
      ssh_authorized_keys:
        - ${SSH_KEY_CONTENT}

storage:
  files:
    - path: /etc/hostname
      mode: 0644
      contents:
        inline: ${HOSTNAME}

    - path: /etc/flatcar/update.conf
      mode: 0644
      contents:
        inline: |
          GROUP=${FLATCAR_CHANNEL}
          SERVER=

  directories:
    - path: /opt/images
      mode: 0755
    - path: /var/log/app
      mode: 0755
    - path: /opt/appdata
      mode: 0755

systemd:
  units:
    - name: docker.service
      enabled: true

    - name: import-images.service
      enabled: true
      contents: |
        [Unit]
        Description=Import Offline Docker Images
        After=docker.service
        Requires=docker.service
        ConditionDirectoryNotEmpty=/opt/images

        [Service]
        Type=oneshot
        RemainAfterExit=yes
        ExecStart=/bin/sh -c 'for f in /opt/images/*.tar; do /usr/bin/docker load -i "\$f"; done'

        [Install]
        WantedBy=multi-user.target
EOF

  for svc in "${SERVICES[@]}"; do
    name="$(printf '%s' "${svc}" | awk -F'|' '{print $1}')"
    image="$(printf '%s' "${svc}" | awk -F'|' '{print $2}')"
    args="$(printf '%s' "${svc}" | awk -F'|' '{print $3}')"
    printf '\n    - name: %s.service\n      enabled: true\n      contents: |\n' "${name}"
    emit_service_unit_body "${name}" "${image}" "${args}" | sed 's/^/        /'
  done
} > "${BUTANE_FILE}"

# ------------------------------------------------------------
# Ignition genereren via butane (in container, geen lokale install nodig)
# ------------------------------------------------------------

log "Ignition genereren: ${IGNITION_FILE}"
docker run --rm -i \
  -v "${BUILD_DIR}/flatcar:/work" \
  -w /work \
  quay.io/coreos/butane:release \
  --pretty --strict config.bu --output config.ign

[[ -s "${IGNITION_FILE}" ]] || die "Ignition genereren is mislukt: ${IGNITION_FILE} is leeg."

# ------------------------------------------------------------
# Scripts kopiëren / genereren
# ------------------------------------------------------------

log "Scripts genereren in ${BUILD_DIR}/scripts"

cat > "${BUILD_DIR}/scripts/install-flatcar.sh" <<EOF
#!/usr/bin/env bash
# Installeert Flatcar op de bare-metal machine.
# Draai dit script vanaf de Ubuntu Live USB.
set -euo pipefail

USB_DIR="\$( cd -- "\$( dirname -- "\${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

DEVICE="\${1:-${INSTALL_DEVICE}}"

echo "[+] Flatcar installeren op \${DEVICE}"
sudo bash "\${USB_DIR}/flatcar/${FLATCAR_INSTALLER}" \\
  -d "\${DEVICE}" \\
  -f "\${USB_DIR}/flatcar/${FLATCAR_IMAGE}" \\
  -i "\${USB_DIR}/flatcar/config.ign"

echo "[+] Docker images kopieren naar /opt/images"
sudo mkdir -p /mnt/newroot
sudo mount "\${DEVICE}9" /mnt/newroot 2>/dev/null || sudo mount "\${DEVICE}p9" /mnt/newroot
sudo mkdir -p /mnt/newroot/opt/images
sudo cp "\${USB_DIR}/docker-images/"*.tar /mnt/newroot/opt/images/
sudo umount /mnt/newroot

echo "[+] Klaar. Reboot de machine."
EOF

# --- update-app.sh: alleen docker images + service units, geen OS update ---
{
  cat <<'HEADER'
#!/usr/bin/env bash
# Update alleen de applicatie laag op een draaiende Flatcar machine:
#   - laadt nieuwe docker images uit ./docker-images/*.tar
#   - overschrijft de bijbehorende /etc/systemd/system/<svc>.service units
#   - daemon-reload + restart per service
# Raakt de OS partities (USR-A/USR-B) NIET aan. Geen reboot nodig.
set -euo pipefail

USB_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

echo "[+] Nieuwe images kopieren naar /opt/images en laden"
sudo mkdir -p /opt/images
sudo cp "${USB_DIR}/docker-images/"*.tar /opt/images/
for f in "${USB_DIR}/docker-images/"*.tar; do
  echo "  load $(basename "${f}")"
  sudo docker load -i "${f}"
done

write_unit() {
  local name="$1"
  local path="/etc/systemd/system/${name}.service"
  echo "[+] schrijf ${path}"
  sudo tee "${path}" > /dev/null
}

HEADER

  for svc in "${SERVICES[@]}"; do
    name="$(printf '%s' "${svc}" | awk -F'|' '{print $1}')"
    image="$(printf '%s' "${svc}" | awk -F'|' '{print $2}')"
    args="$(printf '%s' "${svc}" | awk -F'|' '{print $3}')"
    term="UNIT_$(printf '%s' "${name}" | tr -c 'A-Za-z0-9_' '_')"
    printf 'write_unit %q <<'"'"'%s'"'"'\n' "${name}" "${term}"
    emit_service_unit_body "${name}" "${image}" "${args}"
    printf '%s\n\n' "${term}"
  done

  cat <<'FOOTER'
echo "[+] daemon-reload"
sudo systemctl daemon-reload

FOOTER

  for svc in "${SERVICES[@]}"; do
    name="$(printf '%s' "${svc}" | awk -F'|' '{print $1}')"
    printf 'echo "[+] restart %s.service"\nsudo systemctl enable --now %s.service\nsudo systemctl restart %s.service\n' \
      "${name}" "${name}" "${name}"
  done

  printf '\necho "[+] App update klaar."\n'
} > "${BUILD_DIR}/scripts/update-app.sh"

# --- update-os.sh: alleen Flatcar OS update naar inactieve partitie ---
# Wordt alleen gegenereerd als er een update payload beschikbaar is.
if [[ "${HAVE_UPDATE_PAYLOAD}" -eq 1 ]]; then
  cat > "${BUILD_DIR}/scripts/update-os.sh" <<EOF
#!/usr/bin/env bash
# Schrijft de meegeleverde Flatcar update payload naar de inactieve USR
# partitie en markeert die als next boot target. Raakt docker images en
# service units NIET aan.
#
# Na succes: handmatig 'sudo reboot' om de nieuwe partitie te activeren.
# Bij boot- of health-check failure rolt update_engine automatisch terug.
set -euo pipefail

USB_DIR="\$( cd -- "\$( dirname -- "\${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

PAYLOAD="\${USB_DIR}/updates/${FLATCAR_UPDATE_PAYLOAD}"
VERSION="${FLATCAR_VERSION}"

if [[ ! -f "\${PAYLOAD}" ]]; then
  echo "Geen update payload gevonden: \${PAYLOAD}" >&2
  exit 1
fi

echo "[+] Flatcar OS update naar versie \${VERSION}"
sudo flatcar-update --to-version "\${VERSION}" --to-payload "\${PAYLOAD}"

echo "[+] Geschreven naar inactieve partitie."
echo "    Reboot wanneer je klaar bent:  sudo reboot"
EOF
fi

cat > "${BUILD_DIR}/scripts/import-images.sh" <<'EOF'
#!/usr/bin/env bash
# Laadt alle .tar docker images vanuit /opt/images in de lokale docker daemon.
set -euo pipefail
for f in /opt/images/*.tar; do
  echo "[+] docker load ${f}"
  docker load -i "${f}"
done
EOF

cat > "${BUILD_DIR}/scripts/health-check.sh" <<'EOF'
#!/usr/bin/env bash
# Eenvoudige health check. Pas aan naar de geleverde services.
set -e
EXIT=0
for svc in $(systemctl list-units --type=service --state=running \
              --no-legend --plain | awk '{print $1}' | grep -v '^$'); do
  systemctl is-active --quiet "${svc}" || { echo "FAIL ${svc}"; EXIT=1; }
done
exit "${EXIT}"
EOF

chmod +x "${BUILD_DIR}/scripts/"*.sh

# ------------------------------------------------------------
# Checksums
# ------------------------------------------------------------

log "SHA256 checksums genereren"
(
  cd "${BUILD_DIR}"
  find flatcar docker-images updates -type f -print0 2>/dev/null \
    | xargs -0 sha256sum > checksums/sha256sum.txt
)

# ------------------------------------------------------------
# README in de build dir
# ------------------------------------------------------------

if [[ "${HAVE_UPDATE_PAYLOAD}" -eq 1 ]]; then
  OS_UPDATE_STATUS="beschikbaar (update-os.sh aanwezig)"
else
  OS_UPDATE_STATUS="NIET beschikbaar — update-os.sh overgeslagen"
fi

cat > "${BUILD_DIR}/README.txt" <<EOF
Flatcar deployment bundle
=========================

Gegenereerd door create-flatcar-files.sh op $(date -u +'%Y-%m-%dT%H:%M:%SZ')
Hostname:        ${HOSTNAME}
Install device:  ${INSTALL_DEVICE}
Flatcar channel: ${FLATCAR_CHANNEL}
Flatcar versie:  ${FLATCAR_VERSION}
Architectuur:    ${FLATCAR_ARCH}
OS update:       ${OS_UPDATE_STATUS}

Structuur:
  flatcar/        Flatcar image, installer, butane + ignition config
  docker-images/  Vooraf gebouwde/gepullde docker images als .tar
  scripts/        install-flatcar.sh, update-app.sh, import-images.sh,
                  health-check.sh (en update-os.sh indien payload beschikbaar)
  updates/        Flatcar OTA update payload (alleen indien beschikbaar)
  checksums/      SHA256 hashes van alle artifacts

Eerste installatie:
  1. Kopieer deze hele directory naar een USB stick
  2. Boot de bare-metal machine vanaf een Ubuntu Live USB
  3. Mount de stick: sudo mount /dev/sdX1 /mnt/usb
  4. Draai: sudo bash /mnt/usb/scripts/install-flatcar.sh
  5. Reboot

App update (alleen docker images / services, geen OS reboot):
  1. scp -r build/ core@host:/tmp/bundle
  2. ssh core@host
  3. sudo bash /tmp/bundle/scripts/update-app.sh

OS update (Flatcar zelf naar nieuwe versie, A/B partitie):
  1. scp -r build/ core@host:/tmp/bundle
  2. ssh core@host
  3. sudo bash /tmp/bundle/scripts/update-os.sh
  4. sudo reboot   (rollt automatisch terug bij boot/health failure)
EOF

log "Klaar."
log "Build output: ${BUILD_DIR}"
