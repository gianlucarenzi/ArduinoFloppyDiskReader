# waffle-nbd — Guida all'installazione su Linux

**waffle-nbd** è un server NBD (*Network Block Device*) che espone il
floppy disk letto da un adattatore DrawBridge / Waffle V2 come un
dispositivo a blocchi Linux (`/dev/nbd0`), montabile con i normali
strumenti del sistema operativo.

> [!NOTE]
> Questa guida riguarda esclusivamente **waffle-nbd** e il suo sistema
> di auto-mount (`waffle-monitor.py`).  
> Per il driver FUSE (`waffle-fuse`) consultare il README principale del
> progetto.

---

## Indice

1. [Panoramica dell'architettura](#1-panoramica-dellarchitettura)
2. [Hardware supportato](#2-hardware-supportato)
3. [Dipendenze di sistema](#3-dipendenze-di-sistema)
4. [Moduli kernel richiesti](#4-moduli-kernel-richiesti)
5. [Compilazione](#5-compilazione)
6. [Installazione](#6-installazione)
7. [Uso manuale](#7-uso-manuale)
8. [Auto-mount con waffle-monitor](#8-auto-mount-con-waffle-monitor)
9. [Montaggio del filesystem](#9-montaggio-del-filesystem)
10. [Configurazione della modalità (FUSE vs NBD)](#10-configurazione-della-modalità-fuse-vs-nbd)
11. [Log e diagnostica](#11-log-e-diagnostica)
12. [Risoluzione dei problemi](#12-risoluzione-dei-problemi)

---

## 1. Panoramica dell'architettura

```
┌─────────────────────────────────────────────────────┐
│  DrawBridge / Waffle V2 ── /dev/ttyUSB0             │
│                                                     │
│  waffle-nbd <ttyUSB0>          ← server NBD         │
│       │  porta TCP 10809                            │
│  nbd-client 127.0.0.1 10809 /dev/nbd0  ← client    │
│       │                                             │
│  /dev/nbd0  ────────────────────────────────────── │
│       │                                             │
│  mount -t affs /dev/nbd0 /mnt   (Amiga OFS/FFS)    │
│  mount -t vfat /dev/nbd0 /mnt   (DOS FAT)          │
└─────────────────────────────────────────────────────┘
```

**Ciclo di vita del disco** (gestito interamente da `waffle-nbd`):

| Fase | Stato |
|------|-------|
| 1 | Polling ogni 2,5 s per verificare la presenza del disco |
| 2 | Disco rilevato → porta seriale aperta, motore abilitato |
| 3 | Server in ascolto su TCP → accetta il primo client NBD |
| 4 | Serve le richieste di lettura/scrittura |
| 5 | Disco rimosso → client disconnesso; attesa assenza fisica |
| → 1 | Disco uscito → ritorno al polling |

---

## 2. Hardware supportato

| Dispositivo | VID:PID | Chip FTDI |
|-------------|---------|-----------|
| Waffle V2   | `0403:6015` | FT230X |
| DrawBridge (classico) | `0403:6001` | FT232RL |

---

## 3. Dipendenze di sistema

### 3.1 Pacchetti per la compilazione

```bash
sudo apt install \
    build-essential \
    g++ \
    make \
    git \
    pkg-config
```

> `waffle-nbd` **non richiede libfuse**.  
> `libfuse3-dev` o `libfuse-dev` servono solo per `waffle-fuse`.

### 3.2 Pacchetti runtime — waffle-nbd

`waffle-nbd` usa la porta seriale standard (`/dev/ttyUSB0`) tramite le
API POSIX (`termios`). Non ha dipendenze di librerie esterne obbligatorie.

```bash
# Nessun pacchetto aggiuntivo obbligatorio per il binario waffle-nbd.
```

**Opzionale: libreria FTDI D2XX proprietaria**

Il codice prova a caricare `libftd2xx.so` a runtime via `dlopen` per
operazioni di accesso diretto USB. Se non presente, cade silenziosamente
sulla porta seriale standard — nessuna azione necessaria per l'uso normale.

Per installarla (solo se necessario per funzionalità avanzate):

```bash
# Scaricare la versione Linux ARM/x86_64 da:
# https://ftdichip.com/drivers/d2xx-drivers/
# Poi installare manualmente:
tar xzf libftd2xx-linux-*.tgz
sudo cp release/build/libftd2xx.so.* /usr/local/lib/
sudo ldconfig
```

### 3.3 Pacchetti runtime — client NBD

```bash
sudo apt install nbd-client
```

### 3.4 Pacchetti per waffle-monitor.py (auto-mount)

```bash
# Obbligatori
sudo apt install \
    python3 \
    python3-pyudev \
    nbd-client \
    kmod

# Raccomandati (notifiche desktop e apertura file manager)
sudo apt install \
    libnotify-bin \
    xdg-utils \
    python3-gi \
    gir1.2-notify-0.7 \
    gir1.2-glib-2.0

# Per montare filesystem Amiga (affs) senza errori di modulo:
# (su Debian/Ubuntu il modulo è già incluso nel kernel standard — vedi §4)
```

### 3.5 Permessi utente

L'utente che usa `waffle-nbd` direttamente (senza `waffle-monitor`) deve
far parte del gruppo `dialout` per accedere a `/dev/ttyUSBx`:

```bash
sudo usermod -aG dialout $USER
# Effettuare logout e login per rendere effettivo il gruppo.
```

> `waffle-monitor.py` gira come **root** via systemd, quindi non richiede
> questo per l'auto-mount.

---

## 4. Moduli kernel richiesti

### 4.1 Verifica presenza moduli

```bash
# NBD (Network Block Device)
modinfo nbd

# AFFS (Amiga Fast File System)
modinfo affs

# FTDI serial (di solito già caricato)
modinfo ftdi_sio
```

### 4.2 Caricamento manuale

```bash
sudo modprobe nbd        # crea /dev/nbd0 … /dev/nbd15
sudo modprobe affs       # per montare dischi Amiga OFS/FFS
# ftdi_sio viene caricato automaticamente da udev al plug-in
```

### 4.3 Caricamento automatico all'avvio

```bash
# /etc/modules-load.d/waffle.conf
echo "nbd" | sudo tee /etc/modules-load.d/waffle.conf
echo "affs" | sudo tee -a /etc/modules-load.d/waffle.conf
```

> **Ubuntu 22.04+**: se `modinfo affs` non trova il modulo, installare
> il pacchetto con i moduli extra:
> ```bash
> sudo apt install linux-modules-extra-$(uname -r)
> ```

### 4.4 Numero di device NBD disponibili

Il modulo `nbd` crea di default 16 dispositivi (`nbd0`–`nbd15`).
Per aumentarli:

```bash
sudo modprobe nbd nbds_max=16   # temporaneo

# Permanente:
echo "options nbd nbds_max=16" | sudo tee /etc/modprobe.d/nbd.conf
```

---

## 5. Compilazione

```bash
git clone https://github.com/gianlucarenzi/ArduinoFloppyDiskReader.git
cd ArduinoFloppyDiskReader/waffle-fuse

# Solo waffle-nbd (non richiede libfuse)
make waffle-nbd

# Entrambi i binari
make

# Verifica
./waffle-nbd --help
```

Il Makefile usa `g++` con standard C++17 e collega solo `-lpthread -ldl`
per `waffle-nbd` (nessuna dipendenza da FUSE).

---

## 6. Installazione

### 6.1 Installazione completa (consigliata)

Lo script installa i binari, il monitor Python, la regola udev e il
servizio systemd:

```bash
sudo scripts/install-udev.sh
```

Cosa viene installato:

| File | Destinazione |
|------|-------------|
| `waffle-fuse`         | `/usr/local/bin/waffle-fuse` |
| `waffle-nbd`          | `/usr/local/bin/waffle-nbd` |
| `waffle-udev.sh`      | `/usr/local/lib/waffle-fuse/waffle-udev.sh` |
| `waffle-monitor.py`   | `/usr/local/lib/waffle-fuse/waffle-monitor.py` |
| `waffle-monitor.service` | `/etc/systemd/system/waffle-monitor.service` |
| `99-waffle-fuse.rules` | `/etc/udev/rules.d/99-waffle-fuse.rules` |
| icone SVG             | `/usr/share/icons/hicolor/scalable/devices/` |

Il servizio viene abilitato e avviato automaticamente:

```bash
# Stato del servizio
systemctl status waffle-monitor.service

# Log in tempo reale
journalctl -u waffle-monitor -f
```

### 6.2 Installazione manuale (senza auto-mount)

```bash
sudo install -m 755 waffle-nbd /usr/local/bin/waffle-nbd
sudo install -m 755 waffle-fuse /usr/local/bin/waffle-fuse
```

### 6.3 Disinstallazione

```bash
sudo scripts/install-udev.sh --uninstall
```

---

## 7. Uso manuale

### 7.1 Avviare il server

```bash
# Sintassi:
waffle-nbd [--verbose] <porta-seriale> [indirizzo-nbd] [porta-nbd]

# Esempio:
waffle-nbd /dev/ttyUSB0
waffle-nbd /dev/ttyUSB0 127.0.0.1 10809   # esplicito
waffle-nbd --verbose /dev/ttyUSB0          # log verboso
```

Il server entra nel ciclo di polling (Fase 1): finché non si inserisce un
disco il client NBD non riesce a connettersi — è il comportamento atteso.

### 7.2 Connettere il client NBD

In un secondo terminale (richiede root):

```bash
# Caricare il modulo se non è già presente
sudo modprobe nbd

# Connettere nbd-client al server locale
sudo nbd-client 127.0.0.1 10809 /dev/nbd0

# Verifica connessione
lsblk /dev/nbd0
```

### 7.3 Disconnettere

```bash
sudo umount /mnt/waffle        # se montato
sudo nbd-client -d /dev/nbd0   # disconnetti
# Ctrl-C sul terminale di waffle-nbd (oppure: sudo kill <PID>)
```

---

## 8. Auto-mount con waffle-monitor

`waffle-monitor.py` è un demone Python che ascolta gli eventi udev e
gestisce automaticamente l'intero ciclo: plug-in → server → mount → file
manager → unplug.

### 8.1 Prerequisiti Python

```bash
# Verifica versione Python (richiede 3.9+)
python3 --version

# Installa dipendenze
sudo apt install python3-pyudev

# Verifica
python3 -c "import pyudev; print('pyudev OK')"
```

### 8.2 Avvio manuale del monitor (test)

```bash
# Come root:
sudo python3 /usr/local/lib/waffle-fuse/waffle-monitor.py --verbose
# oppure dal sorgente:
sudo python3 waffle-fuse/scripts/waffle-monitor.py --verbose
```

### 8.3 Servizio systemd

```bash
# Stato
systemctl status waffle-monitor.service

# Avvio / stop
sudo systemctl start  waffle-monitor.service
sudo systemctl stop   waffle-monitor.service

# Abilita all'avvio
sudo systemctl enable waffle-monitor.service

# Log
journalctl -u waffle-monitor -f
journalctl -u waffle-monitor --since "10 min ago"
```

### 8.4 Cosa fa il monitor al plug-in

1. Rileva il device ttyUSB tramite udev (VID:PID `0403:6015` / `0403:6001`)
2. Esegue `modprobe nbd` se `/dev/nbd0` non esiste
3. Lancia `waffle-nbd /dev/ttyUSBx 127.0.0.1 <porta>`
4. Riprova `nbd-client` ogni 2 s (il server è pronto solo dopo che si
   inserisce un disco — questo può richiedere decine di secondi)
5. Mount point: `/run/media/<utente>/waffle-ttyUSBx/`
6. Prova `mount -t affs` → poi `mount -t vfat`
7. Apre il file manager con `xdg-open` come utente della sessione
8. In caso di fallimento manda una notifica desktop

### 8.5 Notifiche desktop

Le notifiche richiedono `notify-send` nell'ambiente dell'utente:

```bash
sudo apt install libnotify-bin
```

`waffle-monitor` usa `runuser` per eseguire `notify-send` come utente
della sessione grafica attiva. Se la sessione desktop non è rilevata
(server headless), le notifiche vengono semplicemente saltate — il
montaggio avviene comunque.

---

## 9. Montaggio del filesystem

### 9.1 Amiga OFS / FFS (affs)

```bash
sudo modprobe affs

# Mount di sola lettura (consigliato per i floppy)
sudo mount -t affs -o ro,uid=$(id -u) /dev/nbd0 /mnt/amiga

# Con dimensione blocco esplicita (se mount fallisce):
sudo mount -t affs -o ro,uid=$(id -u),bs=512 /dev/nbd0 /mnt/amiga
```

### 9.2 DOS / FAT (vfat)

```bash
sudo mount -t vfat \
    -o uid=$(id -u),gid=$(id -g),umask=022 \
    /dev/nbd0 /mnt/dos
```

### 9.3 Rilevamento automatico del formato

`waffle-nbd` rileva automaticamente il formato al primo collegamento NBD:

- **Amiga DD** (880 KB, 80 cilindri × 2 tracce × 11 settori)
- **Amiga HD** (1,76 MB, 80 × 2 × 22)
- **IBM/DOS DD** (720 KB, FAT12, 9 spt) — rilevato da magic 0x55AA
- **IBM/DOS HD** (1,44 MB, FAT12/16, 18 spt) — rilevato da magic 0x55AA

Il formato determina la geometria esposta via NBD; il tipo di filesystem
va comunque specificato a `mount`.

### 9.4 Smontare

```bash
sudo umount /mnt/amiga          # o /mnt/dos
sudo nbd-client -d /dev/nbd0
```

---

## 10. Configurazione della modalità (FUSE vs NBD)

La modalità di default è **NBD**. Per cambiare:

```bash
mkdir -p ~/.config

# Modalità NBD (default, usato se il file non esiste):
echo "NBD" > ~/.config/waffle.mode

# Modalità FUSE:
echo "FUSE" > ~/.config/waffle.mode
```

Il file viene letto da `waffle-udev.sh` all'evento udev di plug-in.

| Modalità | Backend | Mount |
|----------|---------|-------|
| `NBD`    | `waffle-nbd` + `nbd-client` | Kernel affs/vfat |
| `FUSE`   | `waffle-fuse` | FUSE (userspace) |

---

## 11. Log e diagnostica

### Log del servizio systemd

```bash
# Segui in tempo reale
journalctl -u waffle-monitor -f

# Ultimi 100 messaggi
journalctl -u waffle-monitor -n 100

# Dal boot corrente
journalctl -u waffle-monitor -b
```

### Log udev helper

```bash
cat /tmp/waffle-udev.log
```

### Debug del server NBD

```bash
waffle-nbd --verbose /dev/ttyUSB0
```

### Stato dispositivi NBD

```bash
lsblk -o NAME,SIZE,TYPE,MOUNTPOINT | grep nbd
cat /sys/block/nbd0/pid       # PID del nbd-client connesso (0 = libero)
```

### Verifica moduli

```bash
lsmod | grep -E 'nbd|affs|ftdi'
```

### Verifica device seriale

```bash
# Elenco dispositivi USB-seriale connessi
ls -la /dev/ttyUSB*

# Info udev sul device
udevadm info /dev/ttyUSB0

# Monitor eventi udev in tempo reale
udevadm monitor --subsystem-match=tty --property
```

---

## 12. Risoluzione dei problemi

### `modprobe nbd` fallisce

Il modulo potrebbe non essere presente nel kernel in uso.

```bash
# Su Ubuntu/Debian con kernel generico controllare:
ls /lib/modules/$(uname -r)/kernel/drivers/block/nbd.ko*

# Ubuntu: installare moduli extra
sudo apt install linux-modules-extra-$(uname -r)

# Dopo l'installazione:
sudo modprobe nbd
```

### `nbd-client` si rifiuta di connettersi

```
Warning: the oldstyle protocol is no longer supported.
Negotiation: ..E: received invalid negotiation magic ...
```

Il server non è ancora nella fase di accettazione client (probabilmente
nessun disco inserito nel drive). Inserire un disco e riprovare.

```bash
# Verificare che il server sia in ascolto:
ss -tlnp | grep 10809
# o:
nmap -p 10809 127.0.0.1
```

### `/dev/nbd0` non esiste dopo `modprobe nbd`

```bash
# Controllare quanti device sono stati creati:
ls /dev/nbd*

# Se mancano, ricaricare con parametro esplicito:
sudo modprobe -r nbd
sudo modprobe nbd nbds_max=16
```

### Permesso negato su `/dev/ttyUSBx`

```bash
# Aggiungere utente al gruppo dialout:
sudo usermod -aG dialout $USER
newgrp dialout   # oppure fare logout/login
```

### Il filesystem Amiga non si monta (`wrong fs type`)

```bash
# Verificare che il modulo affs sia disponibile:
sudo modprobe affs
lsmod | grep affs

# Verificare il formato del disco (waffle-fuse --probe ritorna il tipo):
waffle-fuse --probe /dev/ttyUSB0
```

### Nessuna notifica desktop

Il monitor gira come root e usa `runuser` per raggiungere la sessione
utente. Verificare:

```bash
# L'utente ha notify-send?
which notify-send

# La sessione è registrata in logind?
loginctl list-sessions

# Test manuale come utente:
notify-send "Test" "Funziona?"
```

### `waffle-monitor.py` non parte (`pyudev not found`)

```bash
sudo apt install python3-pyudev
# oppure:
sudo pip3 install pyudev
```

### `runuser` non disponibile

`runuser` fa parte di `util-linux`, preinstallato su Debian/Ubuntu.
In caso contrario:

```bash
sudo apt install util-linux
```

---

## Riepilogo pacchetti

### Debian 12 / Ubuntu 22.04+

```bash
# Compilazione
sudo apt install build-essential g++ make git pkg-config

# Runtime (NBD + mount)
sudo apt install nbd-client

# Auto-mount (waffle-monitor.py)
sudo apt install \
    python3 \
    python3-pyudev \
    kmod \
    util-linux \
    libnotify-bin \
    xdg-utils \
    python3-gi \
    gir1.2-notify-0.7

# Filesystem Amiga
sudo modprobe affs
echo "affs" | sudo tee -a /etc/modules-load.d/waffle.conf

# Modulo NBD permanente
echo "nbd" | sudo tee /etc/modules-load.d/waffle.conf

# Gruppo seriale per uso manuale (non serve per waffle-monitor)
sudo usermod -aG dialout $USER
```
