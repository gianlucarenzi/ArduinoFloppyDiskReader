# waffle-nbd — Guida all'installazione su Linux

**waffle-nbd** è un server NBD (*Network Block Device*) che espone il
floppy disk letto da un adattatore DrawBridge / Waffle V2 come un
dispositivo a blocchi Linux (`/dev/nbd0`), montabile con i normali
strumenti del sistema operativo.

---

## Indice

1. [Panoramica dell'architettura](#1-panoramica-dellarchitettura)
2. [Hardware supportato](#2-hardware-supportato)
3. [Dipendenze di sistema](#3-dipendenze-di-sistema)
4. [Moduli kernel richiesti](#4-moduli-kernel-richiesti)
5. [Compilazione](#5-compilazione)
6. [Installazione](#6-installazione)
7. [Uso manuale](#7-uso-manuale)
8. [Auto-mount via udev](#8-auto-mount-via-udev)
9. [Montaggio del filesystem](#9-montaggio-del-filesystem)
10. [Log e diagnostica](#10-log-e-diagnostica)
11. [Risoluzione dei problemi](#11-risoluzione-dei-problemi)

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

### 3.4 Pacchetti per l'auto-mount via udev

```bash
# Obbligatori
sudo apt install \
    nbd-client \
    kmod

# Raccomandati (notifiche desktop e apertura file manager)
sudo apt install \
    libnotify-bin \
    xdg-utils

# Per montare filesystem Amiga (affs) senza errori di modulo:
# (su Debian/Ubuntu il modulo è già incluso nel kernel standard — vedi §4)
```

### 3.5 Permessi utente

L'utente che usa `waffle-nbd` direttamente (senza auto-mount) deve
far parte del gruppo `dialout` per accedere a `/dev/ttyUSBx`:

```bash
sudo usermod -aG dialout $USER
# Effettuare logout e login per rendere effettivo il gruppo.
```

> L'handler udev (`waffle-nbd-handler.sh`) gira come **root** via systemd,
> quindi non richiede questo per l'auto-mount.

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

make

# Verifica
./waffle-nbd --help
./waffle-nbd --probe /dev/ttyUSB0
```

Il Makefile usa `g++` con standard C++17 e collega solo `-lpthread -ldl`
(nessuna dipendenza da librerie esterne).

---

## 6. Installazione

### 6.1 Installazione completa (consigliata)

Lo script installa il binario, gli script helper, la regola udev e la
polkit rule:

```bash
sudo scripts/install-udev.sh
```

Cosa viene installato:

| File | Destinazione |
|------|-------------|
| `waffle-nbd`            | `/usr/local/bin/waffle-nbd` |
| `waffle-udev.sh`        | `/usr/local/lib/waffle-nbd/waffle-udev.sh` |
| `waffle-nbd-handler.sh` | `/usr/local/lib/waffle-nbd/waffle-nbd-handler.sh` |
| `99-waffle-nbd.rules`   | `/etc/udev/rules.d/99-waffle-nbd.rules` |
| `85-waffle-nbd.rules`   | `/etc/polkit-1/rules.d/85-waffle-nbd.rules` |
| icone SVG               | `/usr/share/icons/hicolor/scalable/devices/` |

Al plug-in del dispositivo, udev lancia automaticamente `waffle-udev.sh`
che avvia `waffle-nbd-handler.sh` tramite `systemd-run`. Per seguire i log:

```bash
# Log dell'handler (nome dinamico per porta)
journalctl -u waffle-ttyUSB0 -f

# Log udev helper
cat /tmp/waffle-udev.log
```

### 6.2 Installazione manuale (senza auto-mount)

```bash
sudo install -m 755 waffle-nbd /usr/local/bin/waffle-nbd
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

## 8. Auto-mount via udev

Il sistema di auto-mount è interamente basato su script shell e `systemd-run`,
senza dipendenze Python.

### 8.1 Come funziona al plug-in

1. udev rileva il device ttyUSB (VID:PID `0403:6015` / `0403:6001`)
2. Esegue `waffle-udev.sh add /dev/ttyUSBx` come root
3. `waffle-udev.sh` usa `waffle-nbd --probe` per confermare che il dispositivo
   risponda al protocollo Waffle/DrawBridge (non solo un generico FTDI)
4. Se la probe ha successo, `systemd-run` lancia `waffle-nbd-handler.sh`
   come unit transiente `waffle-ttyUSBx.service`

### 8.2 Cosa fa waffle-nbd-handler.sh

1. Carica `modprobe nbd` e `modprobe affs` se necessario
2. Trova un `/dev/nbdN` libero e una porta TCP libera (a partire da 10809)
3. Lancia `waffle-nbd <porta> 127.0.0.1 <porta-tcp>`
4. Attende il messaggio `nbd: disk detected` sullo stdout del server
5. Connette `nbd-client` al device NBD
6. Al ricevimento di `nbd: client requested GO` monta il filesystem
   (`-t affs` o `-t vfat` in base al formato rilevato) sotto `/media/<utente>/`
7. Apre il file manager con `xdg-open` come utente della sessione
8. Al messaggio `nbd: disk absent` smonta e libera il device NBD

### 8.3 Al rimozione del dispositivo

udev esegue `waffle-udev.sh remove /dev/ttyUSBx`, che ferma l'unit
`waffle-ttyUSBx.service` e scatena lo smontaggio tramite il `trap` in
`waffle-nbd-handler.sh`.

### 8.4 Notifiche desktop

Le notifiche richiedono `notify-send` installato:

```bash
sudo apt install libnotify-bin
```

`waffle-nbd-handler.sh` usa `xdg-open` come utente della sessione grafica
attiva. Se la sessione desktop non è rilevata (server headless), l'apertura
del file manager viene saltata — il montaggio avviene comunque.

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

## 10. Log e diagnostica

### Log dell'handler udev (per porta)

```bash
# Log dell'handler (nome dinamico per porta)
journalctl -u waffle-ttyUSB0 -f

# Ultimi 100 messaggi
journalctl -u waffle-ttyUSB0 -n 100

# Dal boot corrente
journalctl -u waffle-ttyUSB0 -b
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

## 11. Risoluzione dei problemi

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

# Verificare la presenza del disco con waffle-nbd --probe:
waffle-nbd --probe /dev/ttyUSB0
```

### Nessuna apertura file manager

L'handler usa `xdg-open` come utente della sessione. Verificare:

```bash
# L'utente ha xdg-utils?
which xdg-open

# La sessione è registrata in logind?
loginctl list-sessions
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

# Auto-mount (udev + handler)
sudo apt install \
    kmod \
    util-linux \
    libnotify-bin \
    xdg-utils

# Filesystem Amiga
sudo modprobe affs
echo "affs" | sudo tee -a /etc/modules-load.d/waffle.conf

# Modulo NBD permanente
echo "nbd" | sudo tee /etc/modules-load.d/waffle.conf

# Gruppo seriale per uso manuale
sudo usermod -aG dialout $USER
```
