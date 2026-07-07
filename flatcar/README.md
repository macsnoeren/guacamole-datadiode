# Flatcar multi-node deployment bundle

Praktische handleiding voor het bouwen en deployen van meerdere
samenwerkende Flatcar bare-metal nodes met hun eigen docker applicaties,
uit één gedeelde build. Voor de achterliggende theorie (A/B partities,
butane, ignition, etc.) zie [FLATCAR.md](FLATCAR.md).

## Wat doet dit?

[create-flatcar-files.sh](create-flatcar-files.sh) leest de gedeelde
[flatcar.conf](flatcar.conf) en per node [nodes/<naam>/node.conf](nodes/),
en produceert één `build/` directory die je naar elke node kunt scp-en /
op een USB-stick kunt zetten. De bundle bevat alles wat nodig is voor:

1. **Bare-metal installatie** — eerste keer Flatcar installeren op een node
2. **App update** — nieuwe docker images uitrollen op een node, geen reboot
3. **OS update** — Flatcar zelf bijwerken via A/B partitie

Eén bundle = alle nodes. Per node wordt de juiste config / images / services
gekozen via de node-naam (hostname).

---

## Eenmalige setup op de build machine

Vereisten: `bash`, `curl`, `sha256sum`, `docker` (voor build + butane container).
Werkt op Linux of WSL. Niet op kale PowerShell — gebruik WSL of een Linux VM.

```bash
# SSH keypair voor de 'core' user op alle Flatcar nodes (één keypair, alle nodes)
ssh-keygen -t ed25519 -a 100 -f flatcar/id_ed25519
```

De public key `id_ed25519.pub` wordt door het script ingelezen.
**Bewaar `id_ed25519` veilig** — zonder de private key kun je niet op de
Flatcar nodes inloggen.

---

## Config-structuur

```
flatcar/
├── flatcar.conf              # gedeeld: channel, versie, ssh key, lijst van NODES
├── nodes/
│   ├── agent/node.conf
│   ├── proxy-stpa-tx/node.conf
│   ├── proxy-stpa-rx/node.conf
│   ├── proxy-plant-tx/node.conf
│   └── proxy-plant-rx/node.conf
└── create-flatcar-files.sh
```

### Gedeelde [flatcar.conf](flatcar.conf)

| Variabele | Wat het doet |
|---|---|
| `FLATCAR_CHANNEL` | `stable` / `beta` / `alpha` |
| `FLATCAR_VERSION` | `current` (auto-resolved naar laatste release) of expliciet bv. `4152.2.0` |
| `FLATCAR_ARCH` | `amd64-usr` of `arm64-usr` |
| `SSH_KEY_FILE` | pad naar de public key (default `./id_ed25519.pub`) |
| `NODES` | array met node-namen — moeten matchen met directories onder `nodes/` |
| `NODE_CONFIG_DIR` | directory met per-node configuraties (default `./nodes`) |
| `BUILD_DIR` | output directory (default `./build`) |
| `CACHE_DIR` | cache voor gedownloade Flatcar artifacts (default `./.cache`) |

### Per-node [nodes/&lt;naam&gt;/node.conf](nodes/)

De directorynaam wordt automatisch de hostname. Override met `HOSTNAME=...`
in node.conf als je iets anders wilt.

| Variabele | Wat het doet |
|---|---|
| `INSTALL_DEVICE` | doelschijf op de bare-metal machine (`/dev/sda`, `/dev/nvme0n1`, ...) |
| `NETWORK_INTERFACES` | array, één regel per interface: `"naam\|dhcp"` of `"naam\|static\|adres/cidr\|gateway\|dns1 dns2"` |
| `DOCKER_BUILD_IMAGES` | images om lokaal te bouwen: `naam:tag\|pad-naar-Dockerfile-dir` |
| `DOCKER_PULL_IMAGES` | images om van een registry te pullen: `naam:tag` |
| `SERVICES` | systemd services: `naam\|image:tag\|docker run args` |

Paden in `DOCKER_BUILD_IMAGES` zijn relatief t.o.v. `flatcar.conf` (de
top-level config), níét t.o.v. de node-directory. Per service wordt een
`/etc/systemd/system/<naam>.service` unit gegenereerd die
`docker run --name <naam> <args> <image:tag>` draait, met restart-on-failure.

### Images zonder service draaien niet

Alleen images die in `SERVICES` staan worden ook daadwerkelijk als container
gestart. Een image die alleen in `DOCKER_BUILD_IMAGES` of `DOCKER_PULL_IMAGES`
staat, wordt wel naar de node gekopieerd en met `docker load` geladen, maar
er wordt geen systemd unit voor aangemaakt. Het bouw-script geeft hier per
ongebruikte image een gele waarschuwing voor, bv:

```
[!] node proxy-plant-rx: image 'gmproxyin:latest' wordt geladen maar heeft geen SERVICES entry — er start geen container voor.
```

Zo zie je tijdens de build meteen of je een service vergeten bent.

### Image deduplicatie

Als meerdere nodes dezelfde `image:tag` definiëren wordt die maar één keer
gebouwd / gepulld. Verschillende build-contexten voor dezelfde tag = fout.
Elke node krijgt een eigen `images.list` die aangeeft welke tars op die node
geladen moeten worden — je verspilt dus geen disk op nodes die een image
niet nodig hebben.

---

## Bundle bouwen

```bash
cd flatcar
bash create-flatcar-files.sh
```

Resultaat in [build/](build/):

```
build/
├── flatcar/
│   ├── flatcar_production_image.bin.bz2   # gedeelde OS install image
│   └── flatcar-install                    # installer script
├── docker-images/                         # gedeelde pool van alle image-tars
│   ├── flatcar-webserver-test_1.0.tar
│   └── ...
├── nodes/
│   ├── agent/
│   │   ├── config.bu                      # node-specifieke butane
│   │   ├── config.ign                     # gegenereerde ignition
│   │   ├── images.list                    # welke tars deze node nodig heeft
│   │   ├── update-services.sh             # service-units writer
│   │   └── node.env                       # HOSTNAME + INSTALL_DEVICE
│   ├── proxy-stpa-tx/...
│   └── ...
├── scripts/
│   ├── install-flatcar.sh                 # bare-metal install (vereist node-arg)
│   ├── update-app.sh                      # app update (auto-detect via /etc/hostname)
│   ├── update-os.sh                       # OS update (A/B partitie, node-agnostisch)
│   ├── import-images.sh
│   └── health-check.sh
├── updates/
│   └── flatcar_production_update.gz       # gedeelde OS update payload
├── checksums/
│   └── sha256sum.txt
└── README.txt
```

Flatcar artifacts worden gecached in `.cache/` — herhaalde builds slaan de
download over.

---

## Scenario 1: bare-metal installatie

Eerste keer, of na een complete herinstallatie van een node. Wist de hele schijf.

### Op de doelmachine

1. Boot vanaf een **Ubuntu Live USB**.
2. Steek de bundle-USB erin en mount deze:
   ```bash
   sudo mkdir -p /mnt/usb
   sudo mount /dev/sdb1 /mnt/usb     # pas /dev/sdb1 aan
   ```
   Of kopieer `build/` via het netwerk naar `/mnt/usb`.
3. Draai de installer met de node-naam:
   ```bash
   sudo bash /mnt/usb/scripts/install-flatcar.sh agent
   ```
   Optioneel een ander device meegeven: `... install-flatcar.sh agent /dev/nvme0n1`
4. Reboot:
   ```bash
   sudo reboot
   ```

Beschikbare nodes lijst je op met:
```bash
ls /mnt/usb/nodes
```

### Wat er gebeurt

- `flatcar-install` schrijft het volledige image (USR-A + USR-B + OEM + ROOT)
  naar het `INSTALL_DEVICE` uit de node-config en injecteert de
  node-specifieke `config.ign`.
- Ignition draait eenmalig bij first-boot en zet hostname (= node-naam),
  SSH key, directories en alle systemd units voor deze node.
- `import-images.service` (oneshot) laadt alle `.tar`'s uit `/opt/images`.
- Alleen de voor deze node geconfigureerde tars zijn gekopieerd op basis
  van zijn `images.list`.
- Elke voor deze node geconfigureerde service start automatisch.

### Verifiëren

```bash
ssh -i flatcar/id_ed25519 core@<ip-adres>
hostname                                # zou de node-naam moeten zijn
systemctl status <service-naam>
docker ps
```

`<ip-adres>` is een van de adressen uit de `NETWORK_INTERFACES` van die node.
Je build-machine moet in hetzelfde subnet zitten om de Flatcar host te
kunnen bereiken.

### Inloggen op een Flatcar node

Op de console verschijnt wel een `login:` prompt, maar de `core` user heeft
**geen wachtwoord** — by design. Toegang loopt uitsluitend via SSH met de
private key die hoort bij de public key in [flatcar.conf](flatcar.conf).

```bash
ssh -i flatcar/id_ed25519 core@<ip-adres>
```

Heeft je private key een passphrase (aangeraden), gebruik dan ssh-agent zodat
je niet bij elke `ssh` / `scp` opnieuw hoeft te typen:

```bash
eval "$(ssh-agent -s)"
ssh-add flatcar/id_ed25519          # 1x passphrase invullen
ssh core@<ip-adres>                 # daarna geen prompt meer in deze shell
```

Permanente oplossing: zet in `~/.ssh/config` voor elke node:

```
Host agent
  HostName 192.168.100.1
  User core
  IdentityFile ~/path/to/flatcar/id_ed25519
  AddKeysToAgent yes

Host proxy-stpa-tx
  HostName 192.168.100.2
  User core
  IdentityFile ~/path/to/flatcar/id_ed25519
  AddKeysToAgent yes
```

Daarna volstaat `ssh agent` / `ssh proxy-stpa-tx`.

> **Let op voor automation:** voor CI of cron-gestuurde `update-app.sh` /
> `update-os.sh` heb je een agent nodig op die machine, óf een aparte key
> zonder passphrase met beperkte rechten.

---

## Scenario 2: app update (docker images / service config)

Wanneer alleen de applicatie verandert: nieuwe image tag, gewijzigde port,
nieuw volume, of een extra service. **Geen reboot nodig.**

### Op de build machine

1. Pas in `nodes/<naam>/node.conf` aan:
   - `DOCKER_BUILD_IMAGES` — nieuwe tag, bv. `gmproxyin:1.1`
   - `SERVICES` — verwijs naar de nieuwe tag, of pas args aan
2. Herbouw de bundle (één keer, voor alle nodes):
   ```bash
   bash create-flatcar-files.sh
   ```

### Op de betreffende node

```bash
scp -r build/ core@agent:/tmp/bundle
ssh core@agent
sudo bash /tmp/bundle/scripts/update-app.sh
```

`update-app.sh` leest `/etc/hostname` en pakt automatisch de configuratie
van die node. Override mogelijk:
```bash
sudo bash /tmp/bundle/scripts/update-app.sh proxy-stpa-rx
```

### Wat er gebeurt

- Alleen de in `nodes/<node>/images.list` genoemde `.tar`'s worden naar
  `/opt/images` gekopieerd en in docker geladen.
- Voor elke service in `SERVICES` van die node wordt
  `/etc/systemd/system/<naam>.service` herschreven (zodat tag/args-wijzigingen
  doorkomen).
- `systemctl daemon-reload` + `enable --now` + `restart` per service.
- OS partities (USR-A/USR-B) worden **niet** aangeraakt.

### Meerdere nodes updaten

```bash
for n in agent proxy-stpa-tx proxy-stpa-rx proxy-plant-tx proxy-plant-rx; do
  scp -r build/ core@${n}:/tmp/bundle
  ssh core@${n} "sudo bash /tmp/bundle/scripts/update-app.sh"
done
```

### Let op

`update-app.sh` herschrijft **alle** voor die node geconfigureerde service
units, niet alleen de gewijzigde. Dat is veilig (idempotent) maar betekent
een korte restart van elke service. Een service die uit de node's config is
weggehaald wordt **niet** automatisch gestopt — die moet je handmatig met
`sudo systemctl disable --now <naam>.service` opruimen.

### Rollback

`update-app.sh` heeft geen ingebouwde rollback. Wil je een vorige versie
terug: bewaar de vorige `build/` en draai `update-app.sh` daaruit.

---

## Scenario 3: OS update (Flatcar zelf)

Wanneer je naar een nieuwere Flatcar versie wilt. Gebruikt het A/B mechanisme:
de update gaat naar de **inactieve** USR partitie en wordt pas actief na een
reboot. Bij boot- of health-failure rolt update_engine automatisch terug.
Dezelfde payload geldt voor alle nodes.

### Op de build machine

1. Pas in [flatcar.conf](flatcar.conf) aan:
   - `FLATCAR_VERSION` — bv. `4152.2.0`
2. Herbouw de bundle:
   ```bash
   bash create-flatcar-files.sh
   ```

### Op elke node

```bash
scp -r build/ core@<node>:/tmp/bundle
ssh core@<node>
sudo bash /tmp/bundle/scripts/update-os.sh
sudo reboot
```

`update-os.sh` is node-agnostisch — exact dezelfde aanroep op alle 5 nodes.

### Wat er gebeurt

- `flatcar-update` schrijft `flatcar_production_update.gz` naar de inactieve
  USR partitie en markeert die als next boot target.
- Docker images, service units, `/var`, `/opt/appdata` blijven onaangeraakt.
- Na reboot draait Flatcar van de nieuwe partitie. Faalt de boot →
  automatische rollback naar de oude partitie.

### Verifiëren na reboot

```bash
ssh core@<node>
cat /etc/os-release            # nieuwe versie?
update_engine_client --status  # update status
docker ps                      # services draaien weer?
```

---

## Combinatie: app + OS in één deploy

Beide scripts zijn idempotent en raken disjuncte resources aan. Volgorde maakt
in principe niet uit, maar veiligste pad per node:

```bash
sudo bash /tmp/bundle/scripts/update-os.sh     # naar inactieve partitie
sudo reboot                                    # nieuwe OS actief
sudo bash /tmp/bundle/scripts/update-app.sh    # services op nieuwe OS
```

Zo zie je per stap of er iets misgaat.

---

## Een nieuwe node toevoegen

1. Maak een directory:
   ```bash
   mkdir -p flatcar/nodes/<nieuwe-naam>
   ```
2. Kopieer een bestaande `node.conf` als template en pas hem aan:
   ```bash
   cp flatcar/nodes/agent/node.conf flatcar/nodes/<nieuwe-naam>/node.conf
   ```
3. Voeg de naam toe aan `NODES=( ... )` in [flatcar.conf](flatcar.conf).
4. Herbouw:
   ```bash
   bash flatcar/create-flatcar-files.sh
   ```

---

## Troubleshooting

### SSH lukt niet na install

- Check dat de private key bij je is: `ssh -i flatcar/id_ed25519 core@<ip>`
- Check op de console: `journalctl -u sshd`
- Klopt het ip? Default krijgt Flatcar via DHCP, tenzij je static hebt gezet.
- Klopt de hostname? `hostname` op de node moet matchen met de node-naam.

### Container start niet

```bash
ssh core@<host>
systemctl status <naam>.service
journalctl -u <naam>.service -f
docker logs <naam>
```

### Image niet geladen

```bash
ls /opt/images/                    # staat de .tar er?
systemctl status import-images.service
docker images                      # is de image geladen?
```

Als de image wel in de gedeelde pool zit maar niet op deze node: check
`build/nodes/<node>/images.list` of die image-tar genoemd staat.

### update-app.sh kiest verkeerde node

`update-app.sh` baseert zich op `/etc/hostname`. Forceer expliciet:
```bash
sudo bash /tmp/bundle/scripts/update-app.sh <node-naam>
```

### OS update terug naar oude versie

Dat is de rollback in actie — boot is gefaald of duurde te lang. Check:

```bash
journalctl -b -1                   # logs van de gefaalde boot
update_engine_client --status
```

### Builds blijven oude Flatcar image gebruiken

Cache wissen:
```bash
rm -rf flatcar/.cache
```

### Build faalt op "Image X wordt door meerdere nodes gebouwd vanuit verschillende contexten"

Twee nodes hebben dezelfde `image:tag` met een ander build-context-pad
opgegeven. Kies één pad, of gebruik verschillende tags.

---

## Welke files staan waar op een Flatcar node

| Pad | Inhoud | Overleeft OS update? |
|---|---|---|
| `/usr/` | Flatcar OS (read-only, A/B) | nee — wordt vervangen |
| `/etc/systemd/system/` | Service units (door ignition gezet, door update-app herschreven) | ja |
| `/opt/images/` | Docker `.tar` archieven voor deze node | ja |
| `/var/lib/docker/` | Docker daemon state, geladen images, containers | ja |
| `/var/log/app/` | App logs (volume mount) | ja |
| `/opt/appdata/` | App data (volume mount) | ja |
