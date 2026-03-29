# waffle-fuse

Un driver FUSE userspace per il lettore di floppy **DrawBridge / Waffle** su Arduino.
Una volta montato, i normali strumenti Linux (`ls`, `cp`, `diff`, `tar`, …) lavorano
direttamente sul disco fisico — senza creare file immagine intermedi.

Formati supportati:

| Formato | Accesso | Descrizione |
|---------|---------|-------------|
| FAT12   | lettura + scrittura | Floppy PC 720 KB / 1.2 MB / 1.44 MB |
| FAT16   | lettura + scrittura | Volumi FAT16 (raro su floppy) |
| Amiga OFS | lettura + scrittura | Floppy Amiga DD/HD, Old Filesystem |
| Amiga FFS | lettura + scrittura | Floppy Amiga DD/HD, Fast Filesystem |

---

## Dipendenze

```bash
sudo apt install libfuse3-dev build-essential
```

Il driver accede all'hardware via seriale (`/dev/ttyUSB0` o simile) usando il
firmware DrawBridge — non serve nessun modulo kernel aggiuntivo.

---

## Compilazione

```bash
cd waffle-fuse
make
```

Il binario risultante è `waffle-fuse/waffle-fuse`.

---

## Uso

```
waffle-fuse <porta-seriale> <mountpoint> [opzioni-fuse]
```

### Opzioni (passate con `-o chiave[=valore]`)

| Opzione | Descrizione |
|---------|-------------|
| `format=fat\|affs` | Forza il tipo di filesystem (default: autodetect) |
| `density=dd\|hd`  | Forza la densità del disco (default: autodetect) |
| `ro`              | Monta in sola lettura (sempre attivo per AFFS) |
| `debug_hw`        | Abilita output debug hardware/seriale |
| `-f`              | Esegui in foreground (utile per debug) |
| `-d`              | Foreground con output debug FUSE |

---

## Montare un disco

### Prima di iniziare

```bash
# Creare il punto di mount (una volta sola)
mkdir -p /mnt/floppy

# Assicurarsi di essere nel gruppo plugdev (per accedere a /dev/ttyUSB0)
# Se necessario: sudo usermod -aG plugdev $USER  (poi ri-login)
```

### Disco Amiga 880K DS/DD (OFS o FFS)

```bash
# Autodetect (rileva il formato dal boot block)
./waffle-fuse /dev/ttyUSB0 /mnt/floppy

# Oppure con formato e densità espliciti
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -o format=affs,density=dd
```

### Disco PC FAT12 720K DS/DD

```bash
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -o density=dd
```

### Disco PC FAT12 1.44 MB HD

```bash
./waffle-fuse /dev/ttyUSB0 /mnt/floppy
# oppure esplicitamente:
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -o density=hd
```

### Con debug visibile (foreground)

```bash
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -f -o debug_hw
```

Output atteso per un disco Amiga:
```
waffle-fuse: disk opened  format=Amiga/ADF  HD=no  sectors=1760
waffle-fuse: mounted as Amiga OFS (read-write)
```

---

## Lavorare con i file

```bash
# Elencare la root
ls /mnt/floppy

# Copiare un file dal disco
cp /mnt/floppy/s/startup-sequence .

# Copiare tutta la directory C:
cp -r /mnt/floppy/C/ ~/backup-amiga-C/

# Fare il backup completo del disco
cp -r /mnt/floppy/ ~/backup-amiga/

# Cercare file per nome
find /mnt/floppy -name "*.mod"

# Leggere un file di testo
cat /mnt/floppy/S/Startup-Sequence

# Copiare un file SUL disco (FAT e Amiga)
cp mio_file.txt /mnt/floppy/

# Creare una directory sul disco Amiga
mkdir /mnt/floppy/MYDIR

# Rinominare/spostare un file sul disco Amiga
mv /mnt/floppy/OLDNAME /mnt/floppy/NEWNAME

# Cancellare un file dal disco Amiga
rm /mnt/floppy/MYFILE
```

> **Nota:** Il disco fisico viene scritto direttamente a ogni operazione di scrittura.
> Smontare sempre con `fusermount3 -u` prima di estrarre il floppy.

---

## Smontare il disco

**Smontare sempre il disco prima di estrarlo o cambiarlo.**

```bash
fusermount3 -u /mnt/floppy
```

Se il driver è in foreground (`-f`), smontare dal secondo terminale:
```bash
# Terminale 2
fusermount3 -u /mnt/floppy
# Il processo waffle-fuse termina automaticamente
```

---

## Cambiare disco (cambio fisico del floppy)

Il driver non supporta il cambio a caldo del disco: ogni mount legge la geometria
e il formato una volta sola all'avvio. Per passare a un disco diverso:

```bash
# 1. Smontare il disco corrente
fusermount3 -u /mnt/floppy

# 2. Estrarre il floppy dall'unità e inserire il nuovo

# 3. Rimontare (il driver rileva automaticamente il nuovo formato)
./waffle-fuse /dev/ttyUSB0 /mnt/floppy
```

Se il secondo disco ha un formato diverso (es. da Amiga a FAT), l'autodetect
lo riconosce senza opzioni aggiuntive. Per forzare il formato:

```bash
# Passare da un disco Amiga a uno FAT
fusermount3 -u /mnt/floppy
# ... cambia il disco ...
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -o format=fat,density=dd

# Passare da un disco FAT a uno Amiga
fusermount3 -u /mnt/floppy
# ... cambia il disco ...
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -o format=affs,density=dd
```

---

## Risoluzione problemi

### "Permission denied" sul mountpoint

Il mountpoint è rimasto in stato inconsistente dopo un arresto forzato:

```bash
fusermount3 -u /mnt/floppy   # prova a smontare
rmdir /mnt/floppy             # rimuovi se vuoto
mkdir /mnt/floppy             # ricrea
```

### "cannot open device"

```bash
ls -l /dev/ttyUSB0            # verifica che esista
groups                        # verifica di essere in plugdev
sudo chmod a+rw /dev/ttyUSB0  # soluzione temporanea
```

### Il disco viene montato come formato sbagliato

Usa `-o format=affs` o `-o format=fat` per forzarlo:

```bash
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -f -o format=affs,density=dd,debug_hw
```

### File/directory mancanti dopo il mount

Controlla che il disco non sia HD montato come DD o viceversa:

```bash
# Se il disco è HD (1.76 MB Amiga / 1.44 MB PC)
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -o density=hd

# Se il disco è DD (880 KB Amiga / 720 KB PC)
./waffle-fuse /dev/ttyUSB0 /mnt/floppy -o density=dd
```

---

## Architettura

```
waffle_fuse.cpp          – main(), parsing opzioni, hook init/destroy
├── disk_cache.h/cpp     – interfaccia IDiskImage + implementazione WaffleDisk
│   └── ADFWriter        – encode/decode MFM, I/O seriale hardware
├── fat_fs.h/cpp         – callback FUSE FAT12/FAT16 (lettura+scrittura)
└── affs_fs.h/cpp        – callback FUSE Amiga OFS/FFS (sola lettura)
```

**WaffleDisk** espone `readSector(lba)` / `writeSector(lba)` con cache per traccia,
riducendo al minimo i lenti round-trip seriali. La geometria viene rilevata
automaticamente al primo `open()`.

**Ordine di rilevamento del formato:**
1. Legge traccia 0 con decoder IBM/MFM; controlla firma `0x55 0xAA` → FAT
2. Se fallisce, legge traccia 0 con decoder Amiga/MFM; controlla `DOS` nel boot block → AFFS

---

## Limitazioni note

* AFFS è **sola lettura** — la scrittura richiederebbe il ricalcolo dei checksum su più tipi di blocco.
* FAT32 non è supportato (praticamente mai usato su floppy).
* Le immagini Amiga partizionate (RDB) non sono supportate; solo ADF standard a singola partizione.
* I nomi LFN vengono decodificati in lettura; i nuovi file vengono creati con nomi 8.3.
* La libreria CAPS/IPF è esclusa intenzionalmente; i file `.ipf` non possono essere letti.
