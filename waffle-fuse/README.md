# waffle-fuse

Un driver FUSE userspace per il lettore di floppy **DrawBridge / Waffle** su Arduino.
Fa comportare il drive come un drive Amiga sul desktop Linux:

* **inserisci un disco** → viene montato da solo in `/media/$USER/<nome volume>` (o
  `~/Floppy/<nome volume>`, vedi sotto) e compare
  come icona di volume sul desktop e nel file manager (come l'icona su Workbench);
* **espelli** → dal menu dell'icona nel tray o con "Smonta" sull'icona del desktop:
  le modifiche in sospeso vengono scritte, poi arriva l'avviso "puoi estrarre il disco";
* **estrai il disco** mentre è montato → viene smontato automaticamente.

Formati supportati (rilevati automaticamente, densità compresa):

| Disco | Filesystem | Accesso |
|-------|------------|---------|
| Amiga 880 KB (DD) / 1.76 MB (HD) | OFS, FFS, INTL (`DOS0`–`DOS3`) | lettura + scrittura |
| Amiga con DirCache (`DOS4`/`DOS5`) | | sola lettura |
| PC 720 KB (DD) / 1.44 MB (HD) | FAT12 (anche FAT16), nomi lunghi VFAT | lettura + scrittura |

---

## Dipendenze

```bash
sudo apt install libfuse3-dev fuse3 build-essential pkg-config
sudo apt install python3-gi gir1.2-gtk-3.0 libnotify-bin   # icona nel tray e notifiche
```

Il driver accede all'hardware via seriale (`/dev/ttyUSB0` o simile): serve essere nel
gruppo `dialout` (o `plugdev`, a seconda della distribuzione):
`sudo usermod -aG dialout $USER`, poi rifare il login.

---

## Compilazione e installazione

```bash
cd waffle-fuse
make
./scripts/waffle-installer.py        # oppure doppio clic su waffle-installer.desktop
```

L'installer grafico (GTK, funziona su X11 e Wayland, con qualsiasi desktop, da Debian ad
Arch) controlla le dipendenze e suggerisce i pacchetti mancanti per la tua distribuzione,
compila waffle-fuse se serve, poi:

* **per l'utente**: installa launcher e icone e avvia **Waffle DiskFlashback** (il demone
  con l'icona nel tray) all'apertura della sessione grafica, con l'autostart XDG (GNOME, KDE,
  XFCE, MATE, Cinnamon, LXQt) oppure con un servizio utente systemd legato a
  `graphical-session.target` sui window manager senza autostart (sway, Hyprland, i3...);
* **per il sistema**: chiede la password di amministratore con la finestra standard di
  polkit (`pkexec`; in mancanza `sudo -A` con un askpass grafico) ed esegue
  `scripts/install-system.sh`, per montare i dischi dove li monta udisks e per dare
  accesso alla porta seriale (gruppo `dialout` o `uucp`, solo se serve).

Ha anche un pulsante per disinstallare. Senza interfaccia grafica: `waffle-installer.py --text`.

L'icona nel tray usa AppIndicator/StatusNotifierItem (pacchetto
`gir1.2-ayatanaappindicator3-0.1` su Debian, `libayatana-appindicator` su Arch), necessaria
su Wayland, GNOME (con l'estensione AppIndicator) e KDE; su X11 c'è un ripiego senza.
Senza tray il demone funziona lo stesso, con le sole notifiche.

### Dove vengono montati i dischi

La cartella di mount prende il nome del volume Amiga o dell'etichetta FAT; se il disco
non ne ha uno si chiama `FloppyDDAFFS`, `FloppyHDAFFS`, `FloppyDDDOS` o `FloppyHDDOS`.
Viene creata dove udisks monta le chiavette USB, `/media/$USER` (Debian, Ubuntu) o
`/run/media/$USER` (Arch, Fedora...), se la parte di sistema è installata; altrimenti in
`~/Floppy`.

udisks2 crea quella cartella da solo, di root con un'ACL in sola lettura per l'utente, e
solo quando monta un dispositivo a blocchi; waffle-fuse invece gira come utente e lì non
può creare cartelle. `scripts/install-system.sh` (lanciato dall'installer, oppure a mano
con `sudo`, `--uninstall` per toglierlo) installa `/etc/tmpfiles.d/waffle-fuse.conf`, con
cui systemd crea la cartella a ogni avvio con un'ACL che dà all'utente anche la scrittura,
e la applica subito. udisks2 imposta i suoi permessi solo quando crea la cartella, quindi
continua a funzionare come prima.

Opzioni del demone: `--port /dev/ttyUSB0` (default: autodetect), `--mount-dir DIR`
(default come sopra; deve stare sotto `/media`, `/run/media` o nella home perché compaiano
le icone sul desktop), `--open` (apre il file manager all'inserimento), `--ro` (monta
sempre in sola lettura).

Per usare la porta con WaffleCopyPRO bisogna prima chiudere il demone (menu → Quit):
la porta seriale è aperta in modo esclusivo.

---

## Come si comporta (come trackdisk.device)

* **Cache per traccia**: si legge sempre una traccia intera; le letture successive sulla
  stessa traccia non toccano il disco.
* **Scrittura differita**: le scritture vanno in cache e la traccia viene scritta circa
  1 secondo dopo l'ultima modifica (opzione `flushdelay`), poi **riletta e verificata**
  (fino a 3 tentativi). Copiare un file non riscrive più la stessa traccia decine di volte.
* **Nessun dato inventato**: un settore illeggibile non viene mai "indovinato". Il file
  che lo contiene dà errore di I/O, e una traccia con settori illeggibili non viene mai
  riscritta, così i settori rovinati non vengono sovrascritti con zeri.
* **Motore**: si spegne dopo 3 secondi di inattività e si riaccende quando serve.
* **Estrazione**: a disco fermo il drive viene interrogato ogni 3 secondi (come
  trackdisk.device; regolabile con `diskpoll`). Se il disco
  sparisce, il volume viene smontato. Se c'erano ancora modifiche in sospeso arriva un
  avviso di errore, perché quelle modifiche sono perse: aspetta l'avviso "puoi estrarre"
  prima di togliere il disco, come su Amiga.
* **Desktop**: nella radice del disco compare un `.xdg-volume-info` *virtuale* (in sola
  lettura, mai scritto sul disco) con il nome del volume e l'icona Amiga o DOS, che i file
  manager usano per l'icona sul desktop. Se il disco ne contiene già uno, viene nascosto.
* **Niente cestino sul floppy**: le cartelle `.Trash`/`.Trash-<uid>` vengono rifiutate, così
  il file manager propone di eliminare definitivamente invece di riempire il disco con i
  file cancellati (opzione `trash` per riabilitarlo).
* **Smontaggio**: dopo `fusermount3 -u` (o "Smonta"/"Espelli") il processo scrive ciò
  che resta, libera il drive e rimuove la directory di mount.

---

## Uso manuale

```
waffle-fuse <porta-seriale> <mountpoint> [opzioni]
waffle-fuse <porta-seriale> --automount <dir> [opzioni]     # monta in <dir>/<nome volume>
waffle-fuse --image <file.adf|file.img> <mountpoint> [opzioni]
waffle-fuse --probe <porta>          # 0 = disco presente, 1 = nessun disco, 2 = errore
waffle-fuse --wait-disk <porta>      # attende l'inserimento di un disco
waffle-fuse --wait-removal <porta>   # attende l'estrazione del disco
```

Strumenti per il disco (senza montarlo):

```
waffle-fuse --format <porta> <tipo> <dd|hd> [nome] [--quick] [--yes] [--force]
waffle-fuse --format-image <file> <tipo> <dd|hd> [nome]   # crea un'immagine vuota
waffle-fuse --scan <porta>                # mappa della superficie: settori buoni per traccia
waffle-fuse --dump <porta> <file.adf>     # legge tutto il disco in un'immagine
waffle-fuse --write-image <porta> <file.adf|img> [--full] [--yes]
                                          # scrive un'immagine sul disco: di default rilegge
                                          # ogni traccia e riscrive solo quelle diverse
```

`<tipo>`: `ffs` (o `amiga`), `ofs`, `amiga-intl`, `ofs-intl`, `dos`. La formattazione
cancella ogni traccia, la riscrive e la rilegge per verificarla (circa 2 minuti), poi
crea il filesystem: AmigaDOS come Format di AmigaOS (DD 880K, HD 1.76M), FAT12 come
MS-DOS (720K, 1.44M). `--quick` riscrive solo le tracce con le strutture del
filesystem e richiede un disco già formattato. Senza `--yes` chiede di scrivere YES.
Un disco DD non viene formattato come HD (o viceversa) senza `--force`.

```bash
./waffle-fuse --format /dev/ttyUSB0 ffs dd "Work"
./waffle-fuse --format /dev/ttyUSB0 dos hd "DATI"
```

Il disco viene letto e riconosciuto **prima** del mount: con un disco sbagliato o
illeggibile il comando esce con un errore e non lascia mountpoint appesi.

### Opzioni (`-o chiave[=valore],...`)

| Opzione | Descrizione |
|---------|-------------|
| `format=fat\|affs` | Forza il tipo di filesystem (default: autodetect) |
| `density=dd\|hd`  | Forza la densità (default: autodetect, provando entrambe) |
| `ro`              | Monta in sola lettura |
| `flushdelay=MS`   | Ritardo della scrittura differita (default 1000 ms) |
| `noverify`        | Non rilegge le tracce scritte (più veloce, meno sicuro) |
| `diskpoll=MS`     | Intervallo del controllo di estrazione (default 3000 ms, come trackdisk.device) |
| `trash`           | Permette ai file manager di creare il cestino `.Trash-<uid>` sul disco |
| `novolumeinfo`    | Niente `.xdg-volume-info` virtuale (nome e icona sul desktop) |
| `debug_hw`        | Output di debug hardware/seriale |
| `-f`              | Resta in foreground |

### Esempi

```bash
# Monta il disco inserito in ~/Floppy/<nome volume>, in background
./waffle-fuse /dev/ttyUSB0 --automount ~/Floppy

# In un mountpoint fisso, in foreground con debug
mkdir -p /tmp/floppy
./waffle-fuse /dev/ttyUSB0 /tmp/floppy -f -o debug_hw

# Smontare (scrive le modifiche in sospeso)
fusermount3 -u ~/Floppy/Workbench3.1

# Lavorare su un'immagine ADF/IMG invece che sul drive
./waffle-fuse --image workbench.adf /tmp/wb
```

C'è anche `scripts/waffle-mount.sh [auto|amiga|dos] [auto|dd|hd] [porta]`, usato dai
launcher del menu: se il disco è montato lo smonta, altrimenti lo monta.

### Codici di uscita

| Codice | Significato |
|--------|-------------|
| 0 | smontato normalmente, tutte le modifiche scritte |
| 1 | errore negli argomenti o nel mount |
| 2 | porta seriale non apribile (scollegata o in uso) |
| 3 | nessun disco nel drive |
| 4 | disco non DOS: non formattato, NDOS (giochi) o protetto |
| 5 | errore del drive leggendo la traccia 0 |
| 6 | filesystem non valido |
| 7 | disco estratto: smontato automaticamente |
| 8 | disco estratto con modifiche ancora in sospeso (perse) |
| 9 | alcune modifiche non sono state scritte |

Su stdout viene stampata una riga per evento (`WAFFLE event=detected ...`,
`WAFFLE event=mounted mountpoint=...`, `WAFFLE event=removed`, ...), letta dal demone.

---

## Test

I filesystem si provano su immagini disco, senza hardware:

```bash
make test
```

Il test copia un albero di file su immagini appena formattate (OFS, FFS, FFS-INTL,
FFS HD, FAT 720K e 1.44M), poi sposta, rinomina, tronca, accoda, cancella e riempie il
disco. Infine confronta il risultato tre volte: dal mount, dopo un nuovo mount e con
strumenti indipendenti (`xdfscan`/`xdftool` di [amitools](https://github.com/cnvogelg/amitools)
per AmigaDOS, `fsck.fat` e [pyfatfs](https://github.com/nathanhi/pyfatfs) per FAT). Se
gli strumenti stanno in un virtualenv:

```bash
python3 -m venv ~/waffle-venv && ~/waffle-venv/bin/pip install amitools pyfatfs "setuptools<70"
XDFTOOL=~/waffle-venv/bin/xdftool XDFSCAN=~/waffle-venv/bin/xdfscan \
TEST_PYTHON=~/waffle-venv/bin/python make test
```

---

## Risoluzione problemi

**"Transport endpoint is not connected" su una directory in `~/Floppy`**: è rimasta da un
crash. Si pulisce con `fusermount3 -uz <directory>`; il demone lo fa da solo all'avvio.

**"cannot open device"**: verifica `ls -l /dev/ttyUSB0`, di essere nel gruppo `dialout`
e che nessun altro programma (WaffleCopyPRO, un altro `waffle-fuse`) stia usando la porta.

**"Disk in use" all'espulsione**: una finestra del file manager o un terminale è dentro il
disco. Chiudili e riprova.

**Disco riconosciuto male**: forza formato e densità, ad esempio
`-o format=affs,density=dd`.

---

## Architettura

```
waffle_fuse.cpp          – main(): rileva il disco, sceglie il filesystem, monta, eventi
├── disk_cache.h/cpp     – IDiskImage; WaffleDisk (cache per traccia, scrittura differita
│   │                      e verificata, motore, rilevamento estrazione); ImageDisk (file)
│   └── ADFWriter        – encode/decode MFM Amiga e IBM, I/O seriale
├── fat_fs.h/cpp         – FAT12/FAT16 con nomi lunghi VFAT
├── format.h/cpp         – immagini vuote AmigaDOS OFS/FFS e FAT12 (DD e HD)
└── affs_fs.h/cpp        – Amiga OFS/FFS (nomi ISO-8859-1 ↔ UTF-8, date in ora locale)
scripts/
├── waffle-diskflashback.py – demone tray: automount, espulsione, notifiche
├── waffle-mount.sh         – monta/smonta con un clic (launcher)
└── install-desktop.sh      – installa launcher, icone e avvio automatico
tests/run-tests.sh          – test dei filesystem su immagini
```

---

## Limitazioni note

* Su AmigaDOS i nomi possono avere al massimo 30 caratteri (ISO-8859-1); i link (hard
  e soft) si leggono ma non si possono modificare.
* I volumi Amiga con bitmap non validata (disco da "validare") vengono montati in sola
  lettura: validali su un Amiga o in un emulatore.
* FAT32, settori diversi da 512 byte e dischi Amiga partizionati (RDB) non sono supportati.
* Un cambio di disco più veloce dell'intervallo di controllo (3 s) può non essere notato (il drive segnala il
  cambio solo con la linea DSKCHG): smonta sempre prima di cambiare disco.
* I file `.ipf` non sono supportati.
