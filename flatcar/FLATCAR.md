# Flatcar Air‑Gapped Installatie & OTA Update README

## Doel

Deze handleiding beschrijft hoe je een air‑gapped Flatcar Container Linux systeem installeert vanuit Ubuntu Live, inclusief:

* Flatcar installatie
* Butane configuratie
* Ignition generatie
* SSH keys
* A/B updates
* Offline updates
* Aanbevolen partitie‑indeling
* OTA update workflow

---

# Architectuur

```text
Ubuntu Live USB
        ↓
Flatcar image + installer
        ↓
Butane config (.bu)
        ↓
Ignition config (.ign)
        ↓
flatcar-install
        ↓
Eerste boot
        ↓
Ignition configureert systeem
```

---

# Flatcar A/B Update Systeem

Flatcar gebruikt twee OS partities:

```text
USR-A → actieve partitie
USR-B → standby partitie
```

Update flow:

```text
Running system
    ↓
Download nieuwe image/payload
    ↓
Schrijf update naar inactieve partitie
    ↓
Markeer nieuwe boot target
    ↓
Reboot
    ↓
Health check OK?
    ├── JA → update actief
    └── NEE → automatische rollback
```

Voordelen:

* Atomic updates
* Automatische rollback
* Veilige OTA updates
* Immutable OS

---

# Benodigdheden

## Vooraf downloaden

Download op een internetmachine:

```text
Flatcar image
flatcar-install script
Butane binary
SHA256 hashes
Eventuele update payloads
```

Kopieer alles naar een USB stick.

---

# Aanbevolen USB Structuur

```text
/mnt/usb/
├── flatcar_production_image.bin.bz2
├── flatcar-install
├── butane
├── config.bu
├── config.ign
└── sha256sum.txt
```

---

# Ubuntu Live Boot

Boot vanaf Ubuntu Live USB.

Open een terminal.

Mount eventueel de USB stick:

```bash
sudo mkdir -p /mnt/usb
sudo mount /dev/sdb1 /mnt/usb
```

---

# SSH Keys

## Host keys

Flatcar genereert automatisch SSH host keys bij eerste boot:

```text
/etc/ssh/ssh_host_*
```

Hier hoef je niets voor te doen.

---

## User SSH key genereren

Maak een nieuwe SSH key:

```bash
ssh-keygen -t ed25519 -a 100
```

Resultaat:

```text
~/.ssh/id_ed25519
~/.ssh/id_ed25519.pub
```

Gebruik ALLEEN de `.pub` key in Butane.

---

# Butane Configuratie

## Voorbeeld config.bu

```yaml
variant: flatcar
version: 1.0.0

passwd:
  users:
    - name: core
      ssh_authorized_keys:
        - ssh-ed25519 AAAAC3NzaC1....

storage:
  files:
    - path: /etc/hostname
      mode: 0644
      contents:
        inline: flatcar-node

systemd:
  units:
    - name: docker.service
      enabled: true
```

---

# Automatische Updates Uitschakelen

Voor air‑gapped systemen:

```yaml
storage:
  files:
    - path: /etc/flatcar/update.conf
      mode: 0644
      contents:
        inline: |
          GROUP=stable
          SERVER=
```

Of disable `update-engine` via systemd.

---

# Butane Installeren

Indien nodig:

```bash
curl -LO https://github.com/coreos/butane/releases/latest/download/butane-x86_64-unknown-linux-gnu
chmod +x butane-x86_64-unknown-linux-gnu
sudo mv butane-x86_64-unknown-linux-gnu /usr/local/bin/butane
```

Offline alternatief:

```bash
chmod +x ./butane
```

---

# Ignition Config Genereren

```bash
butane config.bu > config.ign
```

Resultaat:

```text
config.ign
```

---

# Flatcar Installatie

## Installeren vanaf lokaal image

```bash
sudo bash flatcar-install \
  -d /dev/sda \
  -f flatcar_production_image.bin.bz2 \
  -i config.ign
```

Voor NVMe:

```bash
sudo bash flatcar-install \
  -d /dev/nvme0n1 \
  -f flatcar_production_image.bin.bz2 \
  -i config.ign
```

---

# Reboot

```bash
sudo reboot
```

Daarna verbinden:

```bash
ssh core@IP-ADRES
```

---

# Offline / OTA Updates

## Lokale update payload gebruiken

Voorbeeld:

```bash
sudo flatcar-update \
  --to-version 4152.2.0 \
  --to-payload flatcar_production_update.gz
```

De update wordt naar de inactieve partitie geschreven.

Na reboot wordt automatisch de nieuwe partitie gebruikt.

---

# Aanbevolen OTA Workflow

```text
Running system
    ↓
Download update payload
    ↓
Controleer SHA256/signature
    ↓
Schrijf naar inactieve partitie
    ↓
Markeer boot target
    ↓
Reboot
    ↓
Health check
    ├── OK → update actief
    └── FAIL → rollback
```

---

# Persistent Data Partities

Bewaar applicatiedata buiten de OS partities.

Aanbevolen:

```text
/var/lib/docker
/var/mnt/data
/opt/appdata
```

Zo blijven data behouden tijdens updates.

---

# Aanbevolen Productie Setup

## Veiligheid

Aanbevolen:

* UEFI boot
* Secure Boot
* TPM2
* Read‑only OS
* Signed update payloads
* Watchdog rollback checks

---

# Controlecommando's

## Actieve partitie bekijken

```bash
sudo cgpt show /dev/sda
```

---

## Flatcar versie

```bash
cat /etc/os-release
```

---

## Update status

```bash
update_engine_client --status
```

---

# Troubleshooting

## Ignition logs

```bash
journalctl -u ignition
```

---

## Boot logs

```bash
journalctl -b
```

---

## SSH problemen

Controleer:

```bash
~/.ssh/authorized_keys
```

En netwerkconfiguratie.

---

# Nuttige Links

* [https://www.flatcar.org/docs/latest/installing/bare-metal/installing-to-disk/](https://www.flatcar.org/docs/latest/installing/bare-metal/installing-to-disk/)
* [https://www.flatcar.org/docs/latest/setup/releases/update-strategies/](https://www.flatcar.org/docs/latest/setup/releases/update-strategies/)
* [https://coreos.github.io/butane/](https://coreos.github.io/butane/)

---

# Docker op Flatcar Bare‑Metal

## Doelarchitectuur

```text
USB Stick
├── Flatcar image
├── Butane config
├── Ignition config
├── Docker images (.tar)
├── Install scripts
├── Update scripts
└── Applicatie configuratie
        ↓
Ubuntu Live Boot
        ↓
Flatcar installatie
        ↓
Docker images laden
        ↓
Containers starten via systemd
```

---

# Docker Strategie

Er zijn twee soorten containers:

## 1. Eigen Docker images

Voorbeeld:

```text
my-api
my-worker
my-dashboard
```

Gebouwd vanuit een Dockerfile.

---

## 2. Bestaande Docker images

Voorbeeld:

```text
nginx
postgres
redis
grafana
```

Deze kun je vooraf pullen en exporteren.

---

# Docker Images Offline Beschikbaar Maken

## Eigen image bouwen

Voorbeeld Dockerfile:

```dockerfile
FROM alpine:3.20

RUN apk add --no-cache bash curl

COPY app.sh /app.sh

CMD ["/app.sh"]
```

Build:

```bash
docker build -t my-api:1.0 .
```

---

## Image exporteren naar USB

```bash
docker save my-api:1.0 -o my-api_1.0.tar
```

Voor bestaande image:

```bash
docker pull nginx:latest

docker save nginx:latest -o nginx_latest.tar
```

---

# Docker Images Importeren op Flatcar

Op Flatcar:

```bash
docker load -i /opt/images/my-api_1.0.tar
```

---

# Aanbevolen USB Structuur (Uitgebreid)

```text
/mnt/usb/
├── flatcar/
│   ├── flatcar_production_image.bin.bz2
│   ├── flatcar-install
│   ├── config.bu
│   ├── config.ign
│   └── butane
│
├── docker-images/
│   ├── my-api_1.0.tar
│   ├── nginx_latest.tar
│   └── postgres_16.tar
│
├── scripts/
│   ├── install-flatcar.sh
│   ├── import-images.sh
│   ├── update-flatcar.sh
│   ├── health-check.sh
│   └── rollback-check.sh
│
├── updates/
│   └── flatcar_production_update.gz
│
└── checksums/
    └── sha256sum.txt
```

---

# Persistente Data

## Waarom?

Flatcar updates vervangen alleen de OS partities.

Data moet daarom buiten de A/B partities opgeslagen worden.

---

# Aanbevolen Persistent Storage Layout

```text
/var/lib/docker
/var/log/app
/opt/appdata
/var/mnt/data
```

---

# Persistent Docker Volumes

Voorbeeld:

```bash
docker run -d \
  -v /opt/appdata/postgres:/var/lib/postgresql/data \
  postgres:16
```

---

# Logging Strategie

## Lokale logfiles

Aanbevolen:

```text
/var/log/app/
```

---

## Docker logging driver

Voorbeeld:

```bash
docker run \
  --log-driver=json-file \
  --log-opt max-size=10m \
  --log-opt max-file=5 \
  my-api:1.0
```

---

# Containers via systemd

Flatcar gebruikt systemd als hoofdbeheer.

Aanbevolen:

* Containers automatisch starten via systemd
* Niet handmatig via shell

---

# Voorbeeld systemd Unit

## Bestand

```text
/etc/systemd/system/my-api.service
```

## Inhoud

```ini
[Unit]
Description=My API Container
After=docker.service
Requires=docker.service

[Service]
Restart=always
ExecStartPre=-/usr/bin/docker rm -f my-api
ExecStart=/usr/bin/docker run \
  --name my-api \
  -p 8080:8080 \
  -v /var/log/app:/logs \
  my-api:1.0

ExecStop=/usr/bin/docker stop my-api

[Install]
WantedBy=multi-user.target
```

---

# Butane Config voor Docker Services

## Voorbeeld

```yaml
variant: flatcar
version: 1.0.0

passwd:
  users:
    - name: core
      ssh_authorized_keys:
        - ssh-ed25519 AAAA...

storage:
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

    - name: my-api.service
      enabled: true
      contents: |
        [Unit]
        Description=My API Container
        After=docker.service
        Requires=docker.service

        [Service]
        Restart=always
        ExecStartPre=-/usr/bin/docker rm -f my-api
        ExecStart=/usr/bin/docker run \
          --name my-api \
          -v /var/log/app:/logs \
          my-api:1.0

        ExecStop=/usr/bin/docker stop my-api

        [Install]
        WantedBy=multi-user.target
```

---

# Butane → Ignition

Genereren:

```bash
butane config.bu > config.ign
```

---

# Docker Images Automatisch Importeren

## Script: import-images.sh

```bash
#!/bin/bash
set -e

IMAGE_DIR="/mnt/usb/docker-images"

for image in ${IMAGE_DIR}/*.tar; do
  echo "Loading ${image}"
  docker load -i "${image}"
done
```

---

# Flatcar Install Script

## install-flatcar.sh

```bash
#!/bin/bash
set -e

USB=/mnt/usb

sudo bash ${USB}/flatcar/flatcar-install \
  -d /dev/sda \
  -f ${USB}/flatcar/flatcar_production_image.bin.bz2 \
  -i ${USB}/flatcar/config.ign
```

---

# OTA Update Strategie

## Doel

Nieuwe Flatcar image installeren op:

```text
INACTIEVE PARTITIE
```

Daarna reboot.

Bij problemen:

```text
AUTOMATISCHE ROLLBACK
```

---

# Offline Update Payload

Voorbeeld payload:

```text
flatcar_production_update.gz
```

---

# Update Script

## update-flatcar.sh

```bash
#!/bin/bash
set -e

PAYLOAD="/opt/updates/flatcar_production_update.gz"
VERSION="4152.2.0"

sudo flatcar-update \
  --to-version ${VERSION} \
  --to-payload ${PAYLOAD}

sudo reboot
```

---

# Health Check Script

## health-check.sh

```bash
#!/bin/bash

curl -f http://localhost:8080/health || exit 1

systemctl is-active my-api.service || exit 1

exit 0
```

---

# Rollback Concept

Flatcar rollback gebeurt automatisch als:

* systeem niet boot
* watchdog timeout
* boot marked failed

Extra controle kan via eigen health-check scripts.

---

# Aanbevolen Update Workflow

```text
Nieuwe update payload maken
        ↓
Checksum genereren
        ↓
USB of lokale repository updaten
        ↓
Payload installeren op inactieve partitie
        ↓
Reboot
        ↓
Container health checks
        ↓
Succes → actief
Fail → rollback
```

---

# Toekomstige GitLab Workflow

## Mogelijke pipeline

```text
GitLab CI
    ↓
Docker build
    ↓
Docker save
    ↓
Generate Butane
    ↓
Generate Ignition
    ↓
Build USB bundle
    ↓
Generate update payloads
    ↓
Deploy artifacts
```

---

# Aanbevolen Volgende Stap

Maak eerst een minimale proof-of-concept:

1. Flatcar installeren
2. Eén Docker container offline laden
3. Eén systemd service starten
4. Persistente logs testen
5. OTA update testen
6. Rollback testen

Daarna uitbreiden naar:

* meerdere containers
* Docker Compose alternatief
* Kubernetes/k3s
* GitLab CI pipelines
* signed updates
* automatische deployments

---

# Samenvatting

Flatcar is geschikt voor:

* Air‑gapped systemen
* Embedded Linux
* Immutable infrastructuur
* Kubernetes nodes
* Veilige OTA updates
* A/B rollback systemen

De combinatie van:

* Butane
* Ignition
* Flatcar A/B updates
* Offline payloads

maakt het zeer geschikt voor robuuste productie‑omgevingen.
