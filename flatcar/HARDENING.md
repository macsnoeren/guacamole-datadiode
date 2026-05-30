# Security hardening — analyse en implementatieplan

Dit document beschrijft welke hardening-maatregelen worden voorzien voor de
Flatcar multi-node bundle (zie [README.md](README.md) en
[create-flatcar-files.sh](create-flatcar-files.sh)), waar elke maatregel in
de codebase landt, wat de moeite is, welk risico eraan vastzit, en in welke
volgorde implementatie zinvol is.

Dit is een planningsdocument. Geen van de onderstaande punten is op moment
van schrijven geïmplementeerd.

---

## Architectuur-uitgangspunt

De twee data-diode routes (`low-side` ↔ `high-side`) zitten binnen hetzelfde
trust-domein. Consolidatie op hostniveau is daarom acceptabel, mits:

- elke proxy-route een eigen container heeft (geen gedeelde proxy-process)
- geen shared mutable state tussen containers
- aparte netwerkinterfaces en firewallregels per route
- defense-in-depth zodat compromise van één container niet automatisch de
  andere route raakt

Te vermijden:

- één gezamenlijke proxy-instance voor beide routes
- shared cache / session storage
- privileged containers
- host networking zonder filtering

---

## 1. Container run-flags (read-only, cap-drop, no-new-privileges, tmpfs, user, seccomp)

Technisch zijn dit allemaal docker run argumenten — het 3e veld van
`SERVICES` in een node.conf. Drie implementatie-routes:

### Route A — defaults injecteren in `emit_service_unit_body`
Altijd-aan flags vóór de `<args>` van elke service prefixen:
```
--security-opt=no-new-privileges --cap-drop=ALL
```
Veilig altijd-aan voor user-space proxies. Niet altijd-aan: `--read-only`,
`--user`, `--tmpfs`, AppArmor/Seccomp profiles — die zijn app-specifiek.

### Route B — per-service hardening profile (uitgebreide SERVICES syntax)
Uitbreiden van `SERVICES` met een 4e veld:
```
"name|image|args|profile"
```
waar `profile` verwijst naar een named bundle (bv. `proxy-strict`, `webserver`,
`host-net`) gedefinieerd in `flatcar.conf` of `node.conf`. De generator
expandeert de profile-naam naar concrete flags.

### Route C — compose (aanbevolen)
Compose heeft eersteklas velden voor alles:
```yaml
services:
  proxy:
    read_only: true
    user: "1000:1000"
    cap_drop: [ALL]
    cap_add: [NET_BIND_SERVICE]   # alleen indien nodig
    security_opt:
      - no-new-privileges:true
      - seccomp:/opt/seccomp/proxy.json
      - apparmor=proxy-profile
    tmpfs:
      - /tmp:size=64m,mode=1777
      - /run:size=16m
    pids_limit: 200
```
Alles op één plek, leesbaar, valideerbaar. Dit is het sterkste argument om
compose-migratie eerst te doen — daarna komt vrijwel alle hardening voor
"gratis" mee als config-velden.

---

## 2. Read-only container + tmpfs voor schrijfbare paden

Werkt alleen als per container precies bekend is welke paden writable
moeten zijn.

| Service | Schrijft naar | Schrijfbaar via | `--read-only` mogelijk? |
|---|---|---|---|
| `flatcar-webserver-test` | logs naar /logs | bestaand `-v /var/log/app:/logs` | ja |
| `node-logging-proxy` | `/logs/nodes.log` | bestaand `-v /var/log/app:/logs` | ja |
| `update-node-proxy` | `/var/lib/update-proxy/inbox` + sidecars | bestaand `-v /var/lib/update-proxy:/var/lib/update-proxy` | ja |

Alle drie zijn kandidaten voor `--read-only`, mits `--tmpfs /tmp --tmpfs /run`
erbij komt voor eventuele runtime files. Per service één keer testen.

---

## 3. Non-root in containers

Twee delen, beide nodig:

### Per Dockerfile
Toevoegen aan elk Dockerfile in [../docker/](../docker/):
```dockerfile
RUN addgroup -S app && adduser -S -G app app
USER app
```

### In docker run
`--user 1000:1000` als override mocht dat anders zijn.

**Caveat voor `update-node-proxy`**: de sidecars worden in
`/var/lib/update-proxy/` op een host-volume geschreven. Die directory moet
eigendom hebben van uid 1000, anders breekt schrijven. Vereist coördinatie
via butane:
```yaml
storage:
  directories:
    - path: /var/lib/update-proxy
      mode: 0755
      user:
        id: 1000
      group:
        id: 1000
```

**Caveat poortbinding**: services die <1024 binden hebben `NET_BIND_SERVICE`
nodig. Voor de huidige services (8080, 8081, 1111, 40000) is dat niet aan
de orde. `flatcar-webserver-test` op poort 80 wel — die heeft de cap nodig
of moet naar een hogere poort verhuizen.

---

## 4. AppArmor

Flatcar levert `apparmor_parser` mee maar laadt geen custom profiles
default.

### Implementatieplan
1. Per node optionele directory `nodes/<n>/apparmor/<profile>.profile`
2. Build kopieert deze naar `build/nodes/<n>/apparmor/`
3. Butane schrijft ze als files naar `/etc/apparmor.d/`
4. Eén systemd oneshot unit `apparmor-load.service` die bij boot doet:
   ```
   ExecStart=/sbin/apparmor_parser --replace --write-cache /etc/apparmor.d/*
   ```
   Met `After=local-fs.target Before=docker.service` zodat profiles geladen
   zijn vóór containers starten.
5. Per-service: nieuwe field of profile-mapping die
   `--security-opt apparmor=<profile>` toevoegt.

### Trade-off
Het écht moeilijke is het **schrijven** van zinvolle profiles. Een te losse
profile geeft geen winst, een te strakke profile breekt de app op subtiele
manieren (denial-met-debugging). Pas oppakken nadat de eenvoudigere lagen
(cap-drop, read-only, default seccomp) draaien. Begin met één profile voor
de eenvoudigste container.

---

## 5. Seccomp

Docker past **default seccomp profile** al toe (blokkeert ~50 syscalls).
Custom profile zou strikter zijn.

### Quick win
Niets doen, default is al actief. Verifiëren met:
```
docker inspect <container> | grep Seccomp
```
Zou `default` moeten zijn.

### Custom profile
Zelfde mechaniek als AppArmor — ship `nodes/<n>/seccomp/<name>.json`,
butane drop in `/etc/seccomp/`, service-arg
`--security-opt seccomp=/etc/seccomp/X.json`.

**Niet aan beginnen** voordat een security tester een runtime trace heeft
(bv. via `strace` of `aa-logprof` analoog) om te weten welke syscalls de
app nodig heeft.

---

## 6. Netwerksegmentatie

Drie lagen, alle drie aan te pakken:

### Laag 1 — Docker networks per route
Nu draaien containers in de default bridge of `--network host`. Beter:
- Per logische groep een eigen docker network
- Containers binnen dezelfde groep zien elkaar, andere niet
- Implementatie: oneshot systemd unit die `docker network create proxy-stpa --internal`
  doet vóór de service-units, met `Wants=`/`Before=`.

Nieuw config-veld: `DOCKER_NETWORKS=(name|driver|options)` per node, of in
compose `networks:` blok.

### Laag 2 — `--network host` minimaliseren
`update-node-proxy` gebruikt nu `--network host`. Inventariseren of die
echt host-networking nodig heeft of dat `-p` toe is. Host-networking
omzeilt alle docker network isolation.

### Laag 3 — Host firewall (nftables)
Flatcar heeft `nftables.service`. Toevoegen aan butane:
- `/etc/nftables.conf` met deny-by-default input chain
- SSH (22) + relevante proxy-poorten allow
- Per interface verschillende rules: bv. SSH alleen op management-interface,
  niet op de diode-zijde
- `nftables.service` enabled

Aanbevolen om deze laag te combineren met static ARP/MAC integratie (zie
apart voorstel) — beide gebeuren op interface-niveau.

**Risico**: SSH lockout. Eerst testen op een node die fysiek toegankelijk is.

Convention voor de bundle:
- `nodes/<n>/nftables.conf` (optioneel) → butane file naar `/etc/nftables.conf`
  + enable van `nftables.service`.

---

## 7. Image digest pinning

| Type | Pinning mogelijk? | Aanpak |
|---|---|---|
| `DOCKER_BUILD_IMAGES` (lokaal gebouwd) | Niet zinvol — geen registry digest. Wel mogelijk: image ID na `docker build` opnemen in `build/manifests/<node>.images` voor audit. | Optioneel |
| `DOCKER_PULL_IMAGES` | Ja — syntax al compatibel: `nginx@sha256:abc...` werkt in `docker pull`/`save` | Triviaal — alleen documenteren + validatie regex |

Implementatie: regex-validatie toevoegen in `load_node_config` of in
image-collection fase. Warning als een pull-entry géén `@sha256:` bevat.

**Risico**: bij elke upgrade moet je elke pin handmatig updaten, anders geen
security patches. Compromis: pin op major versie + verifieer digest in CI.

---

## 8. Flatcar host hardening

### a) Auto security updates
Huidige `flatcar.conf` zet `SERVER=` (uit). Voor air-gapped: laten uit,
gebruik `update-os.sh` voor handmatige bundle-updates. **Aanbeveling**:
cron op de build machine die `FLATCAR_VERSION="current"` controleert en een
nieuwe bundle bouwt + signaleert.

### b) Audit logging
- **Optie 1**: kernel audit events naar journald (default in moderne
  systemd) — gratis maar beperkt.
- **Optie 2**: `auditd` als container draaien — meer werk, betere coverage.
- **Optie 3**: forward journal naar centrale syslog/loki — past goed bij
  `node-logging-proxy` flow.

### c) SSH hardening
Butane uitbreiden met `/etc/ssh/sshd_config.d/10-hardening.conf`:
```
PermitRootLogin no
PasswordAuthentication no
AllowUsers core
MaxAuthTries 3
ClientAliveInterval 300
ClientAliveCountMax 2
```
Plus eventueel `Match Address` om SSH te beperken tot een specifiek subnet.

Codeloc: extra inline file in `generate_butane_for_node`. Beter gedeeld via
de top-level `flatcar.conf` (bv. `SSH_HARDENING=true`), niet per node.

### d) sysctl hardening
Butane file `/etc/sysctl.d/99-hardening.conf`:
```
net.ipv4.tcp_syncookies = 1
net.ipv4.conf.all.rp_filter = 1
net.ipv4.conf.all.accept_redirects = 0
net.ipv4.conf.all.send_redirects = 0
net.ipv4.conf.all.log_martians = 1
kernel.kptr_restrict = 2
kernel.dmesg_restrict = 1
fs.protected_hardlinks = 1
fs.protected_symlinks = 1
```
**Let op**: `net.ipv4.ip_forward` MOET aan blijven voor docker bridge
networking — niet uitschakelen.

### e) Disable unused services
Inventariseren wat default draait (`systemctl list-units`). Kandidaten:
- `update-engine.service` (al deels effectief via `SERVER=`)
- `locksmithd.service` (reboot orchestrator — niet nodig in single-machine
  setup)

Via butane:
```yaml
- name: update-engine.service
  mask: true
- name: locksmithd.service
  mask: true
```

`update-os.sh` blijft werken — dat gebruikt `flatcar-update` direct, niet
update-engine's automatische check.

---

## 9. Shared state vermijden

Huidige setup heeft per service een eigen `/var/log/app` bind-mount —
meerdere containers schrijven naar dezelfde directory. Bijvoorbeeld op
`high-side-rx-proxy`:
- `flatcar-webserver-test` → `/var/log/app:/logs`
- `node-logging-proxy` → `/var/log/app:/logs` (schrijft `nodes.log`)

Dit is **shared mutable state tussen containers** — precies wat de
hardening-richtlijn afraadt.

Beter: per service een eigen log-directory:
- `/var/log/flatcar-webserver-test/`
- `/var/log/node-logging-proxy/`
- `/var/log/update-node-proxy/`

Codeloc: butane `storage.directories` per service-naam (generator kan ze
automatisch uit `SERVICES` afleiden), `-v` aanpassen.

Voor `/opt/appdata` idem.

---

## 10. Overzicht — voorstel → codeloc → effort

| Voorstel | Wijzig in | Effort | Risico |
|---|---|---|---|
| `--read-only` per container | docker run args of compose `read_only` | M | hoog (breekt apps) |
| `--tmpfs /tmp /run` | docker run args of compose `tmpfs` | L | laag |
| `--cap-drop ALL` (always) | `emit_service_unit_body` of compose `cap_drop` | L | middel |
| `--security-opt=no-new-privileges` (always) | idem | L | laag |
| Non-root user | per Dockerfile + run args + host-dir ownership | M | middel |
| AppArmor | butane files + load-unit + per-service flag + profile authoring | H | middel-hoog |
| Seccomp default | niets — al actief | — | — |
| Seccomp custom | profile shipping mechaniek + flag | M | hoog (apps breken) |
| Eigen docker network per route | systemd oneshot pre-service, of compose `networks` | M | laag |
| Geen `--network host` | refactor `update-node-proxy` service-def | L | check binding-IP |
| Host firewall (nftables) | butane + `nftables.conf` per node | M | hoog (lockout) |
| Image digest pinning | regex-validatie + docs | L | laag |
| SSH hardening | gedeelde butane snippet | L | laag |
| sysctl hardening | gedeelde butane snippet | L | laag |
| Disable update-engine/locksmithd | butane `units.mask: true` | L | laag |
| Audit logging | systemd-journald default of `auditd` container | M | laag |
| Geen gedeelde log/data dirs | butane `storage.directories` + service args per service | L | laag |

L = low, M = medium, H = high effort.

---

## 11. Voorgestelde implementatie-volgorde

Gegeven dat compose al op de roadmap staat én het natuurlijke huis is voor
de meeste hardening:

| Fase | Wat | Waarom hier |
|---|---|---|
| 1 | **Compose migratie** | Maakt fase 2–6 triviaal in plaats van complex string-gefrutsel |
| 2 | **Always-on basics**: `cap_drop: [ALL]`, `security_opt: [no-new-privileges:true]`, default seccomp | Quick wins, geringe kans op brokken |
| 3 | **Geen shared log/data dirs** + **geen `--network host` waar niet nodig** | Architectuur-hygiëne, voorwaarde voor isolatie |
| 4 | **Read-only + tmpfs** per service | Per service testen, accept-and-iterate |
| 5 | **SSH/sysctl host-hardening** + **disable unused units** + **digest pinning** | Gedeelde butane snippets, breken niets |
| 6 | **Non-root in Dockerfiles** | Dockerfile edits + host-dir ownership via butane |
| 7 | **Network segmentation**: compose networks + nftables (eerst op één test-node!) | Lockout-risico, fysiek toegang houden |
| 8 | **AppArmor** profile per service | Auteurswerk, doe per app na functioneel testen |
| 9 | **Audit logging** (centraal via log-proxy, sluit aan op bestaande `node-logging-proxy` keten) | Past goed bij bestaande architectuur, lage prioriteit |

---

## 12. Open vragen vóór concrete uitwerking

1. **Compose migratie eerst, ja/nee?** Bevestigt of compose (Route C) wordt
   gebruikt voor fase 2–7, of dat de huidige `SERVICES`-syntax blijft
   (Route A+B).
2. **Welke nodes mogen experimenteel?** Network segmentation + nftables
   eerst valideren op een node die makkelijk te resetten is — niet op een
   productie diode-zijde direct.
3. **Trust-grenzen per route**: krijgt elke proxy-route een eigen docker
   network, of mogen tx en rx van dezelfde route met elkaar praten op
   layer 2? (Op een echte diode niet, op een gemeenschappelijke host wel.)
4. **Audit-log bestemming**: lokaal in journald, of via
   `node-logging-proxy` naar een centrale collector?

---

## Aanbevolen eindarchitectuur

### Acceptabel
- Eén Flatcar host per fysieke locatie
- Twee hardened containers (één per proxy-route)
- Zelfde trust domein

### Aanbevolen controls
- Read-only containers
- AppArmor profile per service
- Seccomp (default of custom)
- Non-root user in container
- Capability dropping (`--cap-drop ALL`)
- Tmpfs runtime dirs
- Immutable / digest-pinned pulled images
- Aparte docker networks per route
- Host firewall (nftables) met deny-by-default
- SSH key-only, gehardende sshd config
- Geen shared log/data directories tussen containers

### Niet aanbevolen
- Eén gedeelde proxy service/process voor beide routes
- Shared mutable storage tussen containers
- Privileged containers (`--privileged`)
- Host networking (`--network host`) zonder noodzaak
- Volledige afhankelijkheid van enkel container isolation
