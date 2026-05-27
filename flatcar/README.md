# Flatcar deployment bundle

Praktische handleiding voor het bouwen en deployen van een Flatcar bare-metal
machine met de eigen docker applicaties. Voor de achterliggende theorie
(A/B partities, butane, ignition, etc.) zie [FLATCAR.md](FLATCAR.md).

## Wat doet dit?

[create-flatcar-files.sh](create-flatcar-files.sh) leest [flatcar.conf](flatcar.conf)
en produceert een complete `build/` directory die je op een USB stick of via scp
op een doelmachine zet. De bundle bevat alles wat nodig is voor:

1. **Bare-metal installatie** — eerste keer Flatcar installeren
2. **App update** — nieuwe docker images uitrollen, geen reboot
3. **OS update** — Flatcar zelf bijwerken via A/B partitie

---

## Eenmalige setup op de build machine

Vereisten: `bash`, `curl`, `sha256sum`, `docker` (voor build + butane container).
Werkt op Linux of WSL. Niet op kale PowerShell — gebruik WSL of een Linux VM.

```bash
# SSH keypair voor de 'core' user op de Flatcar machine
ssh-keygen -t ed25519 -a 100 -f flatcar/id_ed25519
```

De public key `id_ed25519.pub` wordt door het script ingelezen.
**Bewaar `id_ed25519` veilig** — zonder de private key kun je niet op de Flatcar
machine inloggen.

---

## Config aanpassen

Bewerk [flatcar.conf](flatcar.conf):

| Variabele | Wat het doet |
|---|---|
| `HOSTNAME` | hostnaam van de Flatcar machine |
| `SSH_KEY_FILE` | pad naar de public key (default `./id_ed25519.pub`) |
| `INSTALL_DEVICE` | doelschijf op de bare-metal machine (`/dev/sda`, `/dev/nvme0n1`, ...) |
| `FLATCAR_CHANNEL` | `stable` / `beta` / `alpha` |
| `FLATCAR_VERSION` | `current` (auto-resolved naar laatste release) of expliciet bv. `4152.2.0` |
| `NETWORK_INTERFACES` | array, één regel per interface: `"naam\|dhcp"` of `"naam\|static\|adres/cidr\|gateway\|dns1 dns2"` |
| `DOCKER_BUILD_IMAGES` | images om lokaal te bouwen: `naam:tag\|pad-naar-Dockerfile-dir` |
| `DOCKER_PULL_IMAGES` | images om van een registry te pullen: `naam:tag` |
| `SERVICES` | systemd services: `naam\|image:tag\|docker run args` |

Per service in `SERVICES` wordt een `/etc/systemd/system/<naam>.service` unit
gegenereerd die `docker run --name <naam> <args> <image:tag>` draait, met
restart-on-failure.

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
│   ├── flatcar_production_image.bin.bz2   # OS install image
│   ├── flatcar-install                    # installer script
│   ├── config.bu                          # butane bron
│   └── config.ign                         # gegenereerde ignition config
├── docker-images/
│   └── <naam>_<tag>.tar                   # per geconfigureerde image
├── scripts/
│   ├── install-flatcar.sh                 # bare-metal install
│   ├── update-app.sh                      # app update (images + units)
│   ├── update-os.sh                       # OS update (A/B partitie)
│   ├── import-images.sh                   # alle .tar's laden
│   └── health-check.sh                    # services status
├── updates/
│   └── flatcar_production_update.gz       # OS update payload
├── checksums/
│   └── sha256sum.txt
└── README.txt
```

Flatcar artifacts worden gecached in `.cache/` — herhaalde builds slaan de
download over.

---

## Scenario 1: bare-metal installatie

Eerste keer, of na een complete herinstallatie. Wist de hele schijf.

### Op de doelmachine

1. Boot vanaf een **Ubuntu Live USB**.
2. Steek de bundle-USB erin en mount deze:
   ```bash
   sudo mkdir -p /mnt/usb
   sudo mount /dev/sdb1 /mnt/usb     # pas /dev/sdb1 aan
   ```
   Of kopieer `build/` via het netwerk naar `/mnt/usb`.
3. Draai de installer:
   ```bash
   sudo bash /mnt/usb/scripts/install-flatcar.sh
   ```
   Optioneel een ander device meegeven: `... install-flatcar.sh /dev/nvme0n1`
4. Reboot:
   ```bash
   sudo reboot
   ```

### Wat er gebeurt

- `flatcar-install` schrijft het volledige image (USR-A + USR-B + OEM + ROOT)
  naar `INSTALL_DEVICE` en injecteert `config.ign` voor de eerste boot.
- Ignition draait eenmalig bij first-boot en zet hostname, SSH key, directories
  en alle systemd units neer.
- `import-images.service` (oneshot) laadt alle `.tar`'s uit `/opt/images`.
- Elke geconfigureerde service start automatisch.

### Verifiëren

```bash
ssh -i flatcar/id_ed25519 core@<ip-adres>
systemctl status flatcar-test.service   # of jouw service-naam
docker ps
```

`<ip-adres>` is een van de adressen uit je `NETWORK_INTERFACES`. Je build-machine
moet in hetzelfde subnet zitten om de Flatcar host te kunnen bereiken.

### Inloggen op de Flatcar machine

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

Permanente oplossing: zet in `~/.ssh/config`:

```
Host flatcar-node
  HostName 192.168.100.1
  User core
  IdentityFile ~/path/to/flatcar/id_ed25519
  AddKeysToAgent yes
```

Daarna volstaat `ssh flatcar-node`.

> **Let op voor automation:** voor CI of cron-gestuurde `update-app.sh` /
> `update-os.sh` heb je een agent nodig op die machine, óf een aparte key
> zonder passphrase met beperkte rechten.

---

## Scenario 2: app update (docker images / service config)

Wanneer alleen de applicatie verandert: nieuwe image tag, gewijzigde port,
nieuw volume, of een extra service. **Geen reboot nodig.**

### Op de build machine

1. Pas in [flatcar.conf](flatcar.conf) aan:
   - `DOCKER_BUILD_IMAGES` — nieuwe tag, bv. `flatcar-test-web:1.1`
   - `SERVICES` — verwijs naar de nieuwe tag, of pas args aan
2. Herbouw de bundle:
   ```bash
   bash create-flatcar-files.sh
   ```

### Op de Flatcar machine

```bash
scp -r build/ core@<host>:/tmp/bundle
ssh core@<host>
sudo bash /tmp/bundle/scripts/update-app.sh
```

### Wat er gebeurt

- Alle `.tar`'s worden naar `/opt/images` gekopieerd en in docker geladen.
- Voor elke service in `SERVICES` wordt `/etc/systemd/system/<naam>.service`
  herschreven (zodat tag/args-wijzigingen doorkomen).
- `systemctl daemon-reload` + `enable --now` + `restart` per service.
- OS partities (USR-A/USR-B) worden **niet** aangeraakt.

### Let op

`update-app.sh` herschrijft **alle** geconfigureerde service units, niet alleen
de gewijzigde. Dat is veilig (idempotent) maar betekent een korte restart van
elke service. Een service die uit `flatcar.conf` is weggehaald wordt **niet**
automatisch gestopt — die moet je handmatig met `sudo systemctl disable --now
<naam>.service` opruimen.

### Rollback

`update-app.sh` heeft geen ingebouwde rollback. Wil je een vorige versie
terug: bewaar de vorige `build/` en draai `update-app.sh` daaruit.

---

## Scenario 3: OS update (Flatcar zelf)

Wanneer je naar een nieuwere Flatcar versie wilt. Gebruikt het A/B mechanisme:
de update gaat naar de **inactieve** USR partitie en wordt pas actief na een
reboot. Bij boot- of health-failure rolt update_engine automatisch terug.

### Op de build machine

1. Pas in [flatcar.conf](flatcar.conf) aan:
   - `FLATCAR_VERSION` — bv. `4152.2.0`
2. Herbouw de bundle:
   ```bash
   bash create-flatcar-files.sh
   ```

### Op de Flatcar machine

```bash
scp -r build/ core@<host>:/tmp/bundle
ssh core@<host>
sudo bash /tmp/bundle/scripts/update-os.sh
sudo reboot
```

### Wat er gebeurt

- `flatcar-update` schrijft `flatcar_production_update.gz` naar de inactieve
  USR partitie en markeert die als next boot target.
- Docker images, service units, `/var`, `/opt/appdata` blijven onaangeraakt.
- Na reboot draait Flatcar van de nieuwe partitie. Faalt de boot → automatische
  rollback naar de oude partitie.

### Verifiëren na reboot

```bash
ssh core@<host>
cat /etc/os-release            # nieuwe versie?
update_engine_client --status  # update status
docker ps                      # services draaien weer?
```

---

## Combinatie: app + OS in één deploy

Beide scripts zijn idempotent en raken disjuncte resources aan. Volgorde maakt
in principe niet uit, maar veiligste pad:

```bash
sudo bash /tmp/bundle/scripts/update-os.sh    # naar inactieve partitie
sudo reboot                                   # nieuwe OS actief
sudo bash /tmp/bundle/scripts/update-app.sh   # services op nieuwe OS
```

Zo zie je per stap of er iets misgaat.

---

## Secure Boot

Secure Boot zorgt dat alleen door de leverancier ondertekende bootloader en
kernel mogen draaien. Doet niets aan userspace of docker containers — die
worden niet gevalideerd door UEFI.

### Pad A — Microsoft-signed shim (geïmplementeerd, aanbevolen)

Flatcar's standaard image bevat een Microsoft-signed `shim` die op zijn beurt
de Flatcar-signed `grub` + kernel verifieert. Er hoeft niets aan deze bundle
te veranderen; alleen het BIOS/UEFI van de doelmachine.

**Voorwaarden:**
- Flatcar versie 3815+ (alle recente `stable` releases)
- Moederbord ondersteunt UEFI Secure Boot (vrijwel alles van na 2012)

**Stappen op de doelmachine, voor de eerste install:**

1. Boot het BIOS/UEFI setup menu (meestal F2 / Del / F10 bij power-on).
2. **CSM / Legacy boot uit** zetten — Secure Boot vereist pure UEFI.
3. **Secure Boot aan** zetten. Als je menu een keuze geeft:
   - "Standard / Microsoft keys" — laadt de default MS CA, dit is wat je wil.
   - "Custom / Setup mode" — alleen kiezen als je naar Pad B gaat.
4. Bewaar en reboot terug naar de Ubuntu Live USB (die moet ook in UEFI mode
   booten — kijk in het boot-menu of er twee entries staan, kies de "UEFI:"
   variant).
5. Draai `install-flatcar.sh` zoals normaal. De installer maakt automatisch
   een GPT layout met EFI System Partition.

**Verifiëren na boot:**

```bash
ssh core@<ip>
sudo bash /tmp/bundle/scripts/health-check.sh   # toont "Secure Boot: enabled"
# of direct:
od -An -t u1 /sys/firmware/efi/efivars/SecureBoot-*  | awk '{print $NF}'
# moet 1 zijn
```

**OS updates blijven werken:** `flatcar-update` schrijft een nieuwe `usr`
partitie met dezelfde signed binaries; shim valideert die bij de volgende boot.

### Pad B — Eigen Secure Boot keys (NIET geïmplementeerd)

Voor airgapped / hoogvertrouwelijke omgevingen kun je de Microsoft CA expliciet
*niet* vertrouwen en eigen Platform Key, KEK en signatuur-database genereren.
Dit is **niet in dit project geautomatiseerd**; hieronder staat wat het zou
kosten als je het later wil oppakken.

**Wat erbij komt kijken:**

1. **Key generatie** op de build-machine:
   ```bash
   openssl req -new -x509 -newkey rsa:4096 -subj "/CN=PK/"  -keyout PK.key  -out PK.crt
   openssl req -new -x509 -newkey rsa:4096 -subj "/CN=KEK/" -keyout KEK.key -out KEK.crt
   openssl req -new -x509 -newkey rsa:4096 -subj "/CN=db/"  -keyout db.key  -out db.crt
   # omzetten naar EFI Signature List (.esl) en authenticated (.auth) formaten
   # met cert-to-efi-sig-list, sign-efi-sig-list (efitools package)
   ```
2. **Re-signing van Flatcar binaries** met `sbsign`:
   - shim, grub.efi en vmlinuz uit `flatcar_production_image.bin.bz2`
   - dit moet bij elke Flatcar release-upgrade opnieuw
3. **BIOS in Setup Mode** zetten en jouw `PK.auth` enrollen — kan alleen
   handmatig in de meeste firmwares (UEFI shell of menu).
4. **Update flow aanpassen**: `flatcar_production_update.gz` bevat ook
   binaries die ge-resigned moeten worden voordat `update-os.sh` ze
   accepteert, anders weigert shim de nieuwe USR partitie.

Geschatte effort: 1-2 dagen werk + onderhoudslast bij elke versie-bump.
Beginnen zou met een script `flatcar/generate-sb-keys.sh` en aanpassingen
in `install-flatcar.sh` en `update-os.sh`.

**Wanneer overwegen:**
- Compliance vereist het expliciet
- Dreigingsmodel sluit Microsoft als root-of-trust uit
- Je wil afdwingen dat alléén intern gebouwde firmware-images mogen booten

Voor de meeste deployments is Pad A voldoende.

---

## Troubleshooting

### SSH lukt niet na install

- Check dat de private key bij je is: `ssh -i flatcar/id_ed25519 core@<ip>`
- Check op de console: `journalctl -u sshd`
- Klopt het ip? Default krijgt Flatcar via DHCP.

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

---

## Welke files staan waar op de Flatcar machine

| Pad | Inhoud | Overleeft OS update? |
|---|---|---|
| `/usr/` | Flatcar OS (read-only, A/B) | nee — wordt vervangen |
| `/etc/systemd/system/` | Service units (door ignition gezet, door update-app herschreven) | ja |
| `/opt/images/` | Docker `.tar` archieven | ja |
| `/var/lib/docker/` | Docker daemon state, geladen images, containers | ja |
| `/var/log/app/` | App logs (volume mount) | ja |
| `/opt/appdata/` | App data (volume mount) | ja |
