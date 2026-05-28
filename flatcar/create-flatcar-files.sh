#!/usr/bin/env bash
#
# create-flatcar-files.sh
#
# Bouwt een complete build/ directory met alle bestanden die nodig zijn om
# meerdere bare-metal Flatcar machines te installeren of te updaten.
#
# Setup:
#   - flatcar.conf           : gedeelde config (kanaal, versie, ssh-key, lijst van nodes)
#   - nodes/<naam>/node.conf : per-node config (install device, netwerk, images, services)
#
# Output:
#   - Eén Flatcar OS image en één update payload (gedeeld)
#   - Eén pool van docker image tars (gedeeld, gededupliceerd)
#   - Per-node butane + ignition + manifest + per-node update-fragment
#   - Scripts die op de USB / target machine de juiste node selecteren
#
# Vereisten op het build-systeem:
#   - bash, curl, sha256sum, awk, sed
#   - docker (voor het bouwen/pullen/opslaan van images en het draaien van butane)

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
# Gedeelde config laden
# ------------------------------------------------------------

CONFIG_FILE="${1:-${SCRIPT_DIR}/flatcar.conf}"

[[ -f "${CONFIG_FILE}" ]] || die "Config bestand niet gevonden: ${CONFIG_FILE}"

log "Gedeelde config laden uit ${CONFIG_FILE}"
# shellcheck source=/dev/null
source "${CONFIG_FILE}"

: "${FLATCAR_CHANNEL:?FLATCAR_CHANNEL ontbreekt in gedeelde config}"
: "${FLATCAR_VERSION:?FLATCAR_VERSION ontbreekt in gedeelde config}"
: "${FLATCAR_ARCH:?FLATCAR_ARCH ontbreekt in gedeelde config}"
: "${SSH_KEY_FILE:?SSH_KEY_FILE ontbreekt in gedeelde config}"
: "${BUILD_DIR:?BUILD_DIR ontbreekt in gedeelde config}"
: "${CACHE_DIR:?CACHE_DIR ontbreekt in gedeelde config}"
: "${NODE_CONFIG_DIR:?NODE_CONFIG_DIR ontbreekt in gedeelde config}"

[[ -n "${NODES+x}" ]] || die "NODES ontbreekt in gedeelde config."
[[ "${#NODES[@]}" -gt 0 ]] || die "NODES is leeg in gedeelde config."

# Maak paden absoluut t.o.v. de gedeelde config-locatie
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
NODE_CONFIG_DIR="$(resolve_path "${NODE_CONFIG_DIR}")"

require_cmd curl
require_cmd sha256sum
require_cmd docker
require_cmd awk
require_cmd sed

[[ -f "${SSH_KEY_FILE}" ]] || die "SSH public key niet gevonden: ${SSH_KEY_FILE}
Genereer er een met: ssh-keygen -t ed25519 -a 100 -f \"${SSH_KEY_FILE%.pub}\""

SSH_KEY_CONTENT="$(tr -d '\n\r' < "${SSH_KEY_FILE}")"

# Valideer dat elke node een node.conf heeft
for node in "${NODES[@]}"; do
  nc="${NODE_CONFIG_DIR}/${node}/node.conf"
  [[ -f "${nc}" ]] || die "Node config ontbreekt: ${nc}"
done

# ------------------------------------------------------------
# Build directory structuur
# ------------------------------------------------------------

log "Build directory voorbereiden: ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"
mkdir -p \
  "${BUILD_DIR}/flatcar" \
  "${BUILD_DIR}/docker-images" \
  "${BUILD_DIR}/nodes" \
  "${BUILD_DIR}/scripts" \
  "${BUILD_DIR}/updates" \
  "${BUILD_DIR}/checksums"

mkdir -p "${CACHE_DIR}"

# ------------------------------------------------------------
# Flatcar artifacts ophalen (gedeeld, gecached)
# ------------------------------------------------------------

FLATCAR_IMAGE="flatcar_production_image.bin.bz2"
FLATCAR_INSTALLER="flatcar-install"
FLATCAR_UPDATE_PAYLOAD="flatcar_production_update.gz"

if [[ "${FLATCAR_VERSION}" == "current" ]]; then
  log "Laatste ${FLATCAR_CHANNEL} versie opzoeken voor ${FLATCAR_ARCH}"
  VERSION_URL="https://${FLATCAR_CHANNEL}.release.flatcar-linux.net/${FLATCAR_ARCH}/current/version.txt"
  RESOLVED_VERSION="$(curl -fsSL "${VERSION_URL}" | awk -F= '/^FLATCAR_VERSION=/{print $2}' | tr -d '\r')"
  [[ -n "${RESOLVED_VERSION}" ]] || die "Kon FLATCAR_VERSION niet uit ${VERSION_URL} parsen."
  log "  current => ${RESOLVED_VERSION}"
  FLATCAR_VERSION="${RESOLVED_VERSION}"
fi

BASE_URL="https://${FLATCAR_CHANNEL}.release.flatcar-linux.net/${FLATCAR_ARCH}/${FLATCAR_VERSION}"

VERSION_CACHE_DIR="${CACHE_DIR}/${FLATCAR_ARCH}/${FLATCAR_VERSION}"
mkdir -p "${VERSION_CACHE_DIR}"

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
download_cached "${BASE_URL}/${FLATCAR_IMAGE}"          "${VERSION_CACHE_DIR}/${FLATCAR_IMAGE}"
download_cached "https://raw.githubusercontent.com/flatcar/init/flatcar-master/bin/flatcar-install" \
                "${CACHE_DIR}/${FLATCAR_INSTALLER}"

UPDATE_URLS=(
  "${BASE_URL}/${FLATCAR_UPDATE_PAYLOAD}"
  "https://update.release.flatcar-linux.net/${FLATCAR_ARCH}/${FLATCAR_VERSION}/${FLATCAR_UPDATE_PAYLOAD}"
)
PAYLOAD_OK=0
for url in "${UPDATE_URLS[@]}"; do
  if download_cached "${url}" "${VERSION_CACHE_DIR}/${FLATCAR_UPDATE_PAYLOAD}"; then
    PAYLOAD_OK=1
    break
  fi
done

if [[ "${PAYLOAD_OK}" -ne 1 ]]; then
  err "Kon de Flatcar update payload niet downloaden. Geprobeerde URLs:"
  for url in "${UPDATE_URLS[@]}"; do err "  ${url}"; done
  die "Stop: update payload is vereist voor update-os.sh."
fi

cp "${VERSION_CACHE_DIR}/${FLATCAR_IMAGE}"          "${BUILD_DIR}/flatcar/${FLATCAR_IMAGE}"
cp "${CACHE_DIR}/${FLATCAR_INSTALLER}"              "${BUILD_DIR}/flatcar/${FLATCAR_INSTALLER}"
cp "${VERSION_CACHE_DIR}/${FLATCAR_UPDATE_PAYLOAD}" "${BUILD_DIR}/updates/${FLATCAR_UPDATE_PAYLOAD}"
chmod +x "${BUILD_DIR}/flatcar/${FLATCAR_INSTALLER}"

# ------------------------------------------------------------
# Helpers voor butane / units / netwerk
# ------------------------------------------------------------

tar_name_for() {
  # "naam:tag" -> "naam_tag.tar"
  printf '%s.tar' "${1//[:\/]/_}"
}

# Waarschuwt voor images die wel geladen worden op een node maar geen
# corresponderende SERVICES entry hebben — die starten dus niet automatisch.
# Vereist dat SERVICES, DOCKER_BUILD_IMAGES, DOCKER_PULL_IMAGES gezet zijn
# (door load_node_config).
warn_unused_images() {
  local node="$1"
  local -A used=()
  local svc img entry
  if [[ "${#SERVICES[@]}" -gt 0 ]]; then
    for svc in "${SERVICES[@]}"; do
      img="$(printf '%s' "${svc}" | awk -F'|' '{print $2}')"
      used["${img}"]=1
    done
  fi
  if [[ "${#DOCKER_BUILD_IMAGES[@]}" -gt 0 ]]; then
    for entry in "${DOCKER_BUILD_IMAGES[@]}"; do
      img="${entry%%|*}"
      if [[ -z "${used[${img}]+x}" ]]; then
        warn "node ${node}: image '${img}' wordt geladen maar heeft geen SERVICES entry — er start geen container voor."
      fi
    done
  fi
  if [[ "${#DOCKER_PULL_IMAGES[@]}" -gt 0 ]]; then
    for entry in "${DOCKER_PULL_IMAGES[@]}"; do
      img="${entry}"
      if [[ -z "${used[${img}]+x}" ]]; then
        warn "node ${node}: image '${img}' wordt geladen maar heeft geen SERVICES entry — er start geen container voor."
      fi
    done
  fi
}

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

emit_network_file_body() {
  local iface_entry="$1"
  local name mode address gateway dns_list
  name="$(printf '%s' "${iface_entry}" | awk -F'|' '{print $1}' | tr -d ' ')"
  mode="$(printf '%s' "${iface_entry}" | awk -F'|' '{print $2}' | tr -d ' ')"
  printf '[Match]\nName=%s\n\n[Network]\n' "${name}"
  case "${mode}" in
    dhcp)
      printf 'DHCP=yes\n'
      ;;
    static)
      address="$(printf '%s' "${iface_entry}" | awk -F'|' '{print $3}')"
      gateway="$(printf '%s' "${iface_entry}" | awk -F'|' '{print $4}')"
      dns_list="$(printf '%s' "${iface_entry}" | awk -F'|' '{print $5}')"
      [[ -n "${address}" ]] || die "static interface '${iface_entry}' mist een adres"
      printf 'Address=%s\n' "${address}"
      [[ -n "${gateway}" ]] && printf 'Gateway=%s\n' "${gateway}"
      for d in ${dns_list}; do
        [[ -n "${d}" ]] && printf 'DNS=%s\n' "${d}"
      done
      ;;
    *)
      die "Onbekende network mode '${mode}' in interface '${iface_entry}'. Gebruik 'dhcp' of 'static'."
      ;;
  esac
}

# Laadt een node.conf in de huidige shell met defaults en validatie.
# Reset alle per-node arrays vooraf zodat er niets lekt tussen nodes.
load_node_config() {
  local node="$1"
  HOSTNAME="${node}"
  INSTALL_DEVICE=""
  NETWORK_INTERFACES=()
  DOCKER_BUILD_IMAGES=()
  DOCKER_PULL_IMAGES=()
  SERVICES=()
  # shellcheck source=/dev/null
  source "${NODE_CONFIG_DIR}/${node}/node.conf"
  : "${INSTALL_DEVICE:?INSTALL_DEVICE ontbreekt in nodes/${node}/node.conf}"
  [[ "${#NETWORK_INTERFACES[@]}" -gt 0 ]] \
    || die "NETWORK_INTERFACES leeg voor node ${node}."
  for iface_entry in "${NETWORK_INTERFACES[@]}"; do
    mode="$(printf '%s' "${iface_entry}" | awk -F'|' '{print $2}' | tr -d ' ')"
    case "${mode}" in
      dhcp|static) ;;
      *) die "Onbekende network mode '${mode}' in node '${node}' (interface '${iface_entry}')." ;;
    esac
  done
}

# ------------------------------------------------------------
# Fase 1: per node de image-referenties verzamelen
# ------------------------------------------------------------

mkdir -p "${CACHE_DIR}/manifests"

log "Image-referenties verzamelen per node"
for node in "${NODES[@]}"; do
  (
    load_node_config "${node}"
    if [[ "${#DOCKER_BUILD_IMAGES[@]}" -gt 0 ]]; then
      for entry in "${DOCKER_BUILD_IMAGES[@]}"; do
        printf 'BUILD\t%s\n' "${entry}"
      done
    fi
    if [[ "${#DOCKER_PULL_IMAGES[@]}" -gt 0 ]]; then
      for entry in "${DOCKER_PULL_IMAGES[@]}"; do
        printf 'PULL\t%s\n' "${entry}"
      done
    fi
  ) > "${CACHE_DIR}/manifests/${node}.images"
done

# Dedupliceer over alle nodes heen
declare -A SEEN_BUILD_CTX
GLOBAL_BUILD_ENTRIES=()
declare -A SEEN_PULL
GLOBAL_PULL_ENTRIES=()

for node in "${NODES[@]}"; do
  while IFS=$'\t' read -r kind entry; do
    [[ -n "${kind:-}" ]] || continue
    case "${kind}" in
      BUILD)
        image="${entry%%|*}"
        ctx_rel="${entry#*|}"
        ctx_abs="$(resolve_path "${ctx_rel}")"
        if [[ -n "${SEEN_BUILD_CTX[${image}]+x}" ]]; then
          if [[ "${SEEN_BUILD_CTX[${image}]}" != "${ctx_abs}" ]]; then
            die "Image ${image} wordt door meerdere nodes gebouwd vanuit verschillende contexten:
  ${SEEN_BUILD_CTX[${image}]}
  ${ctx_abs}"
          fi
        else
          SEEN_BUILD_CTX[${image}]="${ctx_abs}"
          GLOBAL_BUILD_ENTRIES+=( "${image}|${ctx_abs}" )
        fi
        ;;
      PULL)
        image="${entry}"
        if [[ -z "${SEEN_PULL[${image}]+x}" ]]; then
          SEEN_PULL[${image}]=1
          GLOBAL_PULL_ENTRIES+=( "${image}" )
        fi
        ;;
    esac
  done < "${CACHE_DIR}/manifests/${node}.images"
done

# ------------------------------------------------------------
# Fase 2: unieke docker images bouwen en pullen (gedeelde pool)
# ------------------------------------------------------------

if [[ "${#GLOBAL_BUILD_ENTRIES[@]}" -gt 0 ]]; then
  log "Docker images bouwen (gedeelde pool, ${#GLOBAL_BUILD_ENTRIES[@]} unieke)"
  for entry in "${GLOBAL_BUILD_ENTRIES[@]}"; do
    image="${entry%%|*}"
    ctx="${entry#*|}"
    [[ -d "${ctx}" ]] || die "Build context bestaat niet: ${ctx}"
    log "  build ${image} <= ${ctx}"
    docker build -t "${image}" "${ctx}"
    out="${BUILD_DIR}/docker-images/$(tar_name_for "${image}")"
    log "  save  ${image} => ${out}"
    docker save "${image}" -o "${out}"
  done
else
  log "Geen images om te bouwen."
fi

if [[ "${#GLOBAL_PULL_ENTRIES[@]}" -gt 0 ]]; then
  log "Docker images pullen (gedeelde pool, ${#GLOBAL_PULL_ENTRIES[@]} unieke)"
  for image in "${GLOBAL_PULL_ENTRIES[@]}"; do
    log "  pull  ${image}"
    docker pull "${image}"
    out="${BUILD_DIR}/docker-images/$(tar_name_for "${image}")"
    log "  save  ${image} => ${out}"
    docker save "${image}" -o "${out}"
  done
else
  log "Geen images om te pullen."
fi

# ------------------------------------------------------------
# Fase 3: per node butane + ignition + manifest + update-fragment
# ------------------------------------------------------------

generate_butane_for_node() {
  local node="$1"
  local out_bu="$2"
  load_node_config "${node}"
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
      overwrite: true
      contents:
        inline: |
          GROUP=${FLATCAR_CHANNEL}
          SERVER=
EOF

    local idx=10
    for iface_entry in "${NETWORK_INTERFACES[@]}"; do
      iface_name="$(printf '%s' "${iface_entry}" | awk -F'|' '{print $1}' | tr -d ' ')"
      printf '\n    - path: /etc/systemd/network/%d-%s.network\n      mode: 0644\n      contents:\n        inline: |\n' \
        "${idx}" "${iface_name}"
      emit_network_file_body "${iface_entry}" | sed 's/^/          /'
      idx=$((idx + 1))
    done

    cat <<EOF

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
        After=docker.service docker.socket
        Requires=docker.service
        ConditionDirectoryNotEmpty=/opt/images

        [Service]
        Type=oneshot
        RemainAfterExit=yes
        ExecStartPre=/bin/sh -c 'until /usr/bin/docker info >/dev/null 2>&1; do echo "wachten op docker daemon..."; sleep 1; done'
        ExecStart=/bin/sh -c 'for f in /opt/images/*.tar; do /usr/bin/docker load -i "\$f"; done'

        [Install]
        WantedBy=multi-user.target
EOF

    if [[ "${#SERVICES[@]}" -gt 0 ]]; then
      for svc in "${SERVICES[@]}"; do
        name="$(printf '%s' "${svc}" | awk -F'|' '{print $1}')"
        image="$(printf '%s' "${svc}" | awk -F'|' '{print $2}')"
        args="$(printf '%s' "${svc}" | awk -F'|' '{print $3}')"
        printf '\n    - name: %s.service\n      enabled: true\n      contents: |\n' "${name}"
        emit_service_unit_body "${name}" "${image}" "${args}" | sed 's/^/        /'
      done
    fi
  } > "${out_bu}"
}

generate_images_list_for_node() {
  local node="$1"
  local out="$2"
  (
    load_node_config "${node}"
    if [[ "${#DOCKER_BUILD_IMAGES[@]}" -gt 0 ]]; then
      for entry in "${DOCKER_BUILD_IMAGES[@]}"; do
        tar_name_for "${entry%%|*}"
        echo
      done
    fi
    if [[ "${#DOCKER_PULL_IMAGES[@]}" -gt 0 ]]; then
      for image in "${DOCKER_PULL_IMAGES[@]}"; do
        tar_name_for "${image}"
        echo
      done
    fi
  ) > "${out}"
}

generate_node_update_fragment() {
  local node="$1"
  local out="$2"
  (
    load_node_config "${node}"
    cat <<NHEADER
#!/usr/bin/env bash
# Per-node app-update fragment voor: ${HOSTNAME}
# Wordt aangeroepen door scripts/update-app.sh.
set -euo pipefail

NHEADER

    if [[ "${#SERVICES[@]}" -eq 0 ]]; then
      printf 'echo "[+] Node %s heeft geen services geconfigureerd."\n' "${HOSTNAME}"
    else
      for svc in "${SERVICES[@]}"; do
        name="$(printf '%s' "${svc}" | awk -F'|' '{print $1}')"
        image="$(printf '%s' "${svc}" | awk -F'|' '{print $2}')"
        args="$(printf '%s' "${svc}" | awk -F'|' '{print $3}')"
        term="UNIT_$(printf '%s' "${name}" | tr -c 'A-Za-z0-9_' '_')"
        printf 'echo "[+] schrijf /etc/systemd/system/%s.service"\n' "${name}"
        printf 'sudo tee /etc/systemd/system/%s.service > /dev/null <<'"'"'%s'"'"'\n' "${name}" "${term}"
        emit_service_unit_body "${name}" "${image}" "${args}"
        printf '%s\n\n' "${term}"
      done
      printf 'echo "[+] daemon-reload"\nsudo systemctl daemon-reload\n\n'
      for svc in "${SERVICES[@]}"; do
        name="$(printf '%s' "${svc}" | awk -F'|' '{print $1}')"
        printf 'echo "[+] restart %s.service"\nsudo systemctl enable --now %s.service\nsudo systemctl restart %s.service\n' \
          "${name}" "${name}" "${name}"
      done
    fi
  ) > "${out}"
  chmod +x "${out}"
}

log "Per-node butane + ignition + manifests genereren"
for node in "${NODES[@]}"; do
  node_build_dir="${BUILD_DIR}/nodes/${node}"
  mkdir -p "${node_build_dir}"

  butane_file="${node_build_dir}/config.bu"
  ignition_file="${node_build_dir}/config.ign"
  images_list="${node_build_dir}/images.list"
  node_update_sh="${node_build_dir}/update-services.sh"
  node_env="${node_build_dir}/node.env"

  log "  ${node}"
  generate_butane_for_node "${node}" "${butane_file}"
  warn_unused_images "${node}"
  generate_images_list_for_node "${node}" "${images_list}"
  generate_node_update_fragment "${node}" "${node_update_sh}"

  # node.env voor install-flatcar.sh
  (
    load_node_config "${node}"
    cat > "${node_env}" <<NENV
HOSTNAME="${HOSTNAME}"
INSTALL_DEVICE="${INSTALL_DEVICE}"
NENV
  )

  log "    ignition genereren"
  docker run --rm -i \
    -v "${node_build_dir}:/work" \
    -w /work \
    quay.io/coreos/butane:release \
    --pretty --strict config.bu --output config.ign

  [[ -s "${ignition_file}" ]] || die "Ignition genereren is mislukt voor ${node}: ${ignition_file} is leeg."
done

# ------------------------------------------------------------
# Gedeelde scripts (zelfde voor alle nodes)
# ------------------------------------------------------------

log "Gedeelde scripts genereren in ${BUILD_DIR}/scripts"

cat > "${BUILD_DIR}/scripts/install-flatcar.sh" <<EOF
#!/usr/bin/env bash
# Bare-metal installatie van Flatcar voor een specifieke node.
#
# Gebruik vanaf de Ubuntu Live USB:
#   sudo bash install-flatcar.sh <node-naam> [device]
#
# Voorbeeld:
#   sudo bash install-flatcar.sh agent
#   sudo bash install-flatcar.sh proxy-stpa-tx /dev/sda
set -euo pipefail

BUNDLE_DIR="\$( cd -- "\$( dirname -- "\${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

NODE="\${1:-}"
if [[ -z "\${NODE}" ]]; then
  echo "Gebruik: sudo bash install-flatcar.sh <node-naam> [device]" >&2
  echo "Beschikbare nodes:" >&2
  ls -1 "\${BUNDLE_DIR}/nodes" | sed 's/^/  /' >&2
  exit 1
fi

NODE_DIR="\${BUNDLE_DIR}/nodes/\${NODE}"
if [[ ! -d "\${NODE_DIR}" ]]; then
  echo "Onbekende node: \${NODE}" >&2
  echo "Beschikbare nodes:" >&2
  ls -1 "\${BUNDLE_DIR}/nodes" | sed 's/^/  /' >&2
  exit 1
fi

# shellcheck source=/dev/null
source "\${NODE_DIR}/node.env"

DEVICE="\${2:-\${INSTALL_DEVICE}}"

echo "[+] Flatcar installeren op \${DEVICE} voor node \${HOSTNAME}"
sudo bash "\${BUNDLE_DIR}/flatcar/${FLATCAR_INSTALLER}" \\
  -d "\${DEVICE}" \\
  -f "\${BUNDLE_DIR}/flatcar/${FLATCAR_IMAGE}" \\
  -i "\${NODE_DIR}/config.ign"

echo "[+] Docker images voor \${HOSTNAME} kopieren naar /opt/images"
sudo mkdir -p /mnt/newroot
sudo mount "\${DEVICE}9" /mnt/newroot 2>/dev/null || sudo mount "\${DEVICE}p9" /mnt/newroot
sudo mkdir -p /mnt/newroot/opt/images
if [[ -s "\${NODE_DIR}/images.list" ]]; then
  while IFS= read -r tarname; do
    [[ -n "\${tarname}" ]] || continue
    src="\${BUNDLE_DIR}/docker-images/\${tarname}"
    if [[ ! -f "\${src}" ]]; then
      echo "  WARN: ontbrekende image-tar \${tarname}" >&2
      continue
    fi
    echo "  copy \${tarname}"
    sudo cp "\${src}" /mnt/newroot/opt/images/
  done < "\${NODE_DIR}/images.list"
fi
sudo umount /mnt/newroot

echo "[+] Klaar. Reboot de machine."
EOF

cat > "${BUILD_DIR}/scripts/update-app.sh" <<EOF
#!/usr/bin/env bash
# App-update op een draaiende Flatcar node.
#
# Bepaalt de node-naam automatisch via /etc/hostname.
# Override met:
#   sudo bash update-app.sh <node-naam>
#
# Raakt OS partities (USR-A/USR-B) NIET aan. Geen reboot nodig.
set -euo pipefail

BUNDLE_DIR="\$( cd -- "\$( dirname -- "\${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

NODE="\${1:-}"
if [[ -z "\${NODE}" ]]; then
  NODE="\$(cat /etc/hostname | tr -d ' \\r\\n')"
fi

NODE_DIR="\${BUNDLE_DIR}/nodes/\${NODE}"
if [[ ! -d "\${NODE_DIR}" ]]; then
  echo "Onbekende node: \${NODE}" >&2
  echo "Beschikbare nodes:" >&2
  ls -1 "\${BUNDLE_DIR}/nodes" | sed 's/^/  /' >&2
  exit 1
fi

echo "[+] App-update voor node \${NODE}"

sudo mkdir -p /opt/images
if [[ -s "\${NODE_DIR}/images.list" ]]; then
  while IFS= read -r tarname; do
    [[ -n "\${tarname}" ]] || continue
    src="\${BUNDLE_DIR}/docker-images/\${tarname}"
    if [[ ! -f "\${src}" ]]; then
      echo "  ontbrekende image-tar: \${src}" >&2
      exit 1
    fi
    echo "  copy \${tarname}"
    sudo cp "\${src}" /opt/images/
    echo "  load \${tarname}"
    sudo docker load -i "/opt/images/\${tarname}"
  done < "\${NODE_DIR}/images.list"
else
  echo "  geen images geconfigureerd voor node \${NODE}"
fi

bash "\${NODE_DIR}/update-services.sh"

echo "[+] App-update klaar voor node \${NODE}."
EOF

cat > "${BUILD_DIR}/scripts/update-os.sh" <<EOF
#!/usr/bin/env bash
# Flatcar OS update naar de inactieve USR partitie.
# Identiek voor alle nodes (gedeelde OS image).
#
# Na succes: handmatig 'sudo reboot' om de nieuwe partitie te activeren.
# Bij boot- of health-check failure rolt update_engine automatisch terug.
set -euo pipefail

BUNDLE_DIR="\$( cd -- "\$( dirname -- "\${BASH_SOURCE[0]}" )/.." &> /dev/null && pwd )"

PAYLOAD="\${BUNDLE_DIR}/updates/${FLATCAR_UPDATE_PAYLOAD}"
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

cat > "${BUILD_DIR}/scripts/import-images.sh" <<'EOF'
#!/usr/bin/env bash
# Laadt alle .tar docker images vanuit /opt/images in de lokale docker daemon.
set -euo pipefail
for f in /opt/images/*.tar; do
  [[ -f "${f}" ]] || continue
  echo "[+] docker load ${f}"
  docker load -i "${f}"
done
EOF

cat > "${BUILD_DIR}/scripts/health-check.sh" <<'EOF'
#!/usr/bin/env bash
# Eenvoudige health-check voor de huidige node.
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
  find flatcar docker-images updates nodes -type f -print0 2>/dev/null \
    | xargs -0 sha256sum > checksums/sha256sum.txt
)

# ------------------------------------------------------------
# README in de build dir
# ------------------------------------------------------------

cat > "${BUILD_DIR}/README.txt" <<EOF
Flatcar deployment bundle (multi-node)
======================================

Gegenereerd door create-flatcar-files.sh op $(date -u +'%Y-%m-%dT%H:%M:%SZ')
Flatcar channel: ${FLATCAR_CHANNEL}
Flatcar versie:  ${FLATCAR_VERSION}
Architectuur:    ${FLATCAR_ARCH}

Nodes in deze bundle:
$(for n in "${NODES[@]}"; do printf '  - %s\n' "${n}"; done)

Structuur:
  flatcar/        Gedeeld: Flatcar image + installer
  docker-images/  Gedeelde pool van alle gebouwde/gepullde image-tars
  nodes/<naam>/   Per-node: config.bu, config.ign, images.list,
                  update-services.sh, node.env
  updates/        Gedeelde OS update payload
  scripts/        install-flatcar.sh, update-app.sh, update-os.sh,
                  import-images.sh, health-check.sh
  checksums/      SHA256 over alle artifacts

Eerste installatie (Ubuntu Live):
  sudo bash /mnt/usb/scripts/install-flatcar.sh <node-naam> [device]

App-update (op draaiende node, auto-detect via /etc/hostname):
  scp -r build/ core@<host>:/tmp/bundle
  ssh core@<host>
  sudo bash /tmp/bundle/scripts/update-app.sh

OS-update (zelfde bundle voor alle nodes):
  scp -r build/ core@<host>:/tmp/bundle
  ssh core@<host>
  sudo bash /tmp/bundle/scripts/update-os.sh
  sudo reboot
EOF

log "Klaar."
log "Build output: ${BUILD_DIR}"
log "Nodes: ${NODES[*]}"
