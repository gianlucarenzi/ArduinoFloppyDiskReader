# waffle-fuse

Un driver FUSE userspace per il lettore di floppy **DrawBridge / Waffle** su Arduino.
Fa comportare il drive come un drive Amiga sul desktop Linux:

* **inserisci un disco** → viene montato da solo in `/media/$USER/<nome volume>` (o
  `~/Floppy/<nome volume>`, vedi sotto) e compare
  come icona di volume sul desktop e nel file manager (come l'icona su Workbench);
* **espelli** → dal menu dell'icona nel tray o con "Smonta" sull'icona del desktop:
  le modifiche in sospeso vengono scritte, poi arriva l'avviso "puoi estrarre il disco";
* **estrai il disco** mentre è montato → viene smontato automaticamente;
* **doppio clic su un'immagine `.adf` o `.img`** → si apre come disco virtuale, in cui
  aggiungere e togliere file;
* **dal tray** si copia il disco in un'immagine (`.adf`, `.img` o `.scp`) e si scrive
  un'immagine su un disco vero.

Formati supportati (rilevati automaticamente, densità compresa):

| Disco | Filesystem | Accesso |
|-------|------------|---------|
| Amiga 880 KB (DD) / 1.76 MB (HD) | OFS, FFS, INTL (`DOS0`–`DOS3`) | lettura + scrittura |
| Amiga con DirCache (`DOS4`/`DOS5`) | | sola lettura |
| PC 720 KB (DD) / 1.44 MB (HD) | FAT12 (anche FAT16), nomi lunghi VFAT | lettura + scrittura |

---

## Dipendenze

L'installer grafico le controlla e mostra il comando giusto per la tua distribuzione.
A mano:

| Distribuzione | Compilazione | Demone, tray, notifiche, installer |
|---|---|---|
| Debian / Ubuntu | `sudo apt install build-essential pkg-config libfuse3-dev fuse3` | `sudo apt install python3-gi gir1.2-gtk-3.0 gir1.2-ayatanaappindicator3-0.1 libnotify-bin acl` |
| Arch / Manjaro | `sudo pacman -S --needed base-devel pkgconf fuse3` | `sudo pacman -S --needed python-gobject gtk3 libayatana-appindicator libnotify acl` |
| Fedora | `sudo dnf install gcc-c++ make pkgconf fuse3-devel` | `sudo dnf install python3-gobject gtk3 libayatana-appindicator-gtk3 libnotify acl` |
| openSUSE | `sudo zypper install gcc-c++ make pkg-config fuse3-devel` | `sudo zypper install python3-gobject typelib-1_0-Gtk-3_0 typelib-1_0-AyatanaAppIndicator3-0_1 libnotify-tools acl` |

Il pacchetto AppIndicator serve per l'icona nel tray su Wayland, GNOME e KDE; su X11 il
demone ha un ripiego che ne fa a meno.

Il driver accede all'hardware via seriale (`/dev/ttyUSB0` o simile). Se il tuo utente non
può aprire la porta, l'installer lo aggiunge al gruppo che la possiede (`dialout` su Debian
e Ubuntu, `uucp` su Arch); poi bisogna rifare il login.

---

## Compatibilità

**Provato** (con un drive Waffle reale, FTDI FT230X):

* Debian 13 (trixie), MATE su X11, libfuse 3.17, gvfs 1.57, udisks2 2.10: lettura,
  scrittura e formattazione di Amiga DD/HD e DOS 720K/1.44M, demone nel tray, montaggio
  automatico in `/media/$USER`, icone sul desktop (Caja), estrazione a caldo, installer
  grafico con autenticazione polkit, apertura di immagini ADF/IMG con doppio clic, copia
  del disco in un'immagine e scrittura di un'immagine sul disco dal tray (ogni traccia
  scritta viene riletta e verificata). Per gli ADF di giochi NDOS è stato verificato che
  venga scelto Amiberry come programma a cui passarli, senza lanciarlo.

**Previsto, non ancora provato** — il codice è scritto per funzionare con:

* **Distribuzioni**: Debian 11 e 12, Ubuntu 20.04 e successive, Arch e derivate, Fedora,
  openSUSE. Requisiti: libfuse ≥ 3.0 (si usa solo l'API della 3.0), compilatore C++17
  (gcc ≥ 7), Python ≥ 3.7 con PyGObject e GTK 3, systemd (tmpfiles e servizio utente),
  polkit. Su **Debian 11** ci sono tutti: libfuse 3.10, gcc 10, Python 3.9, systemd 247,
  udisks2 2.9 (monta in `/media/$USER`), `gir1.2-ayatanaappindicator3-0.1`. L'unico punto
  da verificare è l'icona personalizzata sul desktop, che usa la chiave `IconFile` di
  `.xdg-volume-info` letta da gvfs: se una versione vecchia non la supportasse, comparirebbe
  l'icona generica del tema per i dischi rimovibili.
* **Desktop**: GNOME, KDE Plasma, XFCE, MATE, Cinnamon, LXQt (avvio con l'autostart XDG);
  sway, Hyprland, i3 e altri window manager (servizio utente systemd legato a
  `graphical-session.target`). X11 e Wayland.
* **Icona nel tray**: AppIndicator/StatusNotifierItem su KDE, XFCE, MATE, Cinnamon, waybar e
  GNOME con l'estensione "AppIndicator"; su GNOME senza estensione non c'è tray e il demone
  funziona con le sole notifiche.
* **Icone dei dischi sul desktop**: file manager basati su GIO (Caja, Nautilus, Nemo,
  Thunar, PCManFM). Dolphin (KDE) usa Solid e non è stato verificato.

Se lo provi su un'altra combinazione, il risultato è utile da aggiungere qui.

---

## Compilazione e installazione

```bash
cd waffle-fuse
make
./scripts/waffle-installer.py        # oppure doppio clic su waffle-installer.desktop
```

L'installer grafico (GTK; pensato per X11 e Wayland, per qualsiasi desktop e per Debian,
Arch, Fedora e openSUSE: vedi Compatibilità per cosa è stato provato) controlla le
dipendenze e suggerisce i pacchetti mancanti per la tua distribuzione, compila
waffle-fuse se serve, poi:

* **per l'utente**: installa launcher e icone e avvia **Waffle DiskFlashback** (il demone
  con l'icona nel tray) all'apertura della sessione grafica, con l'autostart XDG (GNOME, KDE,
  XFCE, MATE, Cinnamon, LXQt) oppure con un servizio utente systemd legato a
  `graphical-session.target` sui window manager senza autostart (sway, Hyprland, i3...);
  se lasci la casella attiva, associa le immagini floppy (`.adf`, `.img`, `.ima`) a
  waffle-fuse, così il doppio clic le apre come dischi virtuali (vedi sotto);
* **per il sistema**: chiede la password di amministratore con la finestra standard di
  polkit (`pkexec`; in mancanza `sudo -A` con un askpass grafico) ed esegue
  `scripts/install-system.sh`, per montare i dischi dove li monta udisks e per dare
  accesso alla porta seriale (gruppo `dialout` o `uucp`, solo se serve).

Ha anche un pulsante per disinstallare. Senza interfaccia grafica: `waffle-installer.py --text`.

L'icona nel tray usa AppIndicator/StatusNotifierItem (pacchetto
`gir1.2-ayatanaappindicator3-0.1` su Debian, `libayatana-appindicator` su Arch), necessaria
su Wayland, GNOME (con l'estensione AppIndicator) e KDE; su X11 c'è un ripiego senza.
Senza tray il demone funziona lo stesso, con le sole notifiche.

### Il menu del tray

| Voce | Cosa fa |
|---|---|
| **Open** | apre il disco montato nel file manager (anche con un clic sull'icona) |
| **Eject** | smonta il disco: scrive le modifiche in sospeso, poi avvisa che si può estrarre |
| **Mount again** | rimonta un disco smontato ma ancora nel drive |
| **Copy disk to image file…** | copia il disco nel drive in un file: `.adf`/`.img` copia i settori (anche dei dischi NDOS di giochi, come disco Amiga), `.scp` registra il flusso magnetico grezzo, anche dei dischi protetti. Propone il nome del volume e la cartella `~/WaffleFolder` se esiste. Circa 1 minuto per un `.adf` DD |
| **Write image to disk…** | scrive un'immagine `.adf`/`.img`/`.ima` sul disco nel drive, che viene sovrascritto (chiede conferma). Propone l'immagine aperta come disco virtuale e la chiude prima, così nel file c'è tutto. Di default riscrive solo le tracce diverse; la casella "Erase and rewrite every track" le cancella e riscrive tutte (circa 2 minuti e mezzo per un DD). Ogni traccia scritta viene riletta e verificata |
| **Quit** | smonta il disco in modo pulito e chiude il demone |

Durante una copia o una scrittura il disco montato viene smontato (la porta seriale serve
all'operazione) e alla fine rimontato: dopo una scrittura con il nuovo contenuto. Una
finestra mostra l'avanzamento traccia per traccia; **Cancel** ferma l'operazione tra una
traccia e l'altra, mai a metà di una traccia. La copia viene fatta in un file temporaneo
(`….part`) rinominato solo alla fine: annullando non si perde un file che si stava
sovrascrivendo.

### Immagini floppy come dischi virtuali

Con il doppio clic su un'immagine `.adf`, `.img` o `.ima` (`scripts/waffle-open-image.py`):

* le immagini AmigaDOS (OFS/FFS) e FAT vengono montate in lettura e scrittura nella stessa
  cartella dei dischi veri, con l'icona sul desktop, e si apre il file manager. Le modifiche
  finiscono direttamente nel file immagine; si chiude con "Smonta" sull'icona. Un secondo
  doppio clic su un'immagine già aperta apre solo la finestra. Un file senza permesso di
  scrittura viene aperto in sola lettura;
* le immagini che waffle-fuse non sa montare (dischi di giochi NDOS, dischi protetti,
  immagini più grandi di un floppy) vengono passate al programma che le apriva prima
  (ad esempio Amiberry per gli ADF o il mounter di immagini del desktop).

L'associazione segue gli standard XDG (shared-mime-info, `mimeapps.list`), quindi vale per
tutti i desktop: i tipi interessati sono `application/x-amiga-disk-image`,
`application/x-amiga-disk-format`, `application/vnd.efi.img` e
`application/x-raw-disk-image`. I programmi associati prima vengono ricordati in
`~/.config/waffle-fuse/previous-handlers` e riassociati alla disinstallazione.
A mano: `scripts/install-desktop.sh --no-image-handler` toglie solo l'associazione.

Attenzione: le modifiche fatte su un disco virtuale vanno nel file stesso. Per tenere
intatto un ADF originale, aprine una copia.

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
* **Desktop**: nella radice del disco compaiono un `.xdg-volume-info` e un
  `.waffle-icon.svg` *virtuali* (in sola lettura, mai scritti sul disco) con il nome del
  volume e l'icona Amiga o DOS, che i file manager usano per l'icona sul desktop. L'icona è
  data come file (`IconFile`), perché con un nome di tema GTK preferirebbe l'icona generica
  "disco rimovibile" del tema attivo. Un `.xdg-volume-info` già presente sul disco viene
  nascosto.
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
waffle-fuse --dump <porta> <file.adf|img|scp> [--format amiga|dos] [--density dd|hd]
            [--tracks N] [--revs N]       # copia tutto il disco in un'immagine
waffle-fuse --write-image <porta> <file.adf|img> [--full] [--yes] [--force]
                                          # scrive un'immagine sul disco: di default rilegge
                                          # ogni traccia e riscrive solo quelle diverse
```

`--dump` sceglie il tipo di copia dall'estensione: `.adf`/`.img` copia i settori del
formato riconosciuto (un disco non DOS viene copiato come disco Amiga, i settori
illeggibili diventano zeri e vengono contati); `.scp` registra il flusso magnetico
(`--tracks`, default 80, fino a 84; `--revs`, giri per traccia, default 3, fino a 5).
`--write-image` rifiuta un'immagine DD su un disco HD (o viceversa) senza `--force`.
`--dump`, `--write-image` e `--format` si fermano tra una traccia e l'altra con Ctrl+C o
SIGTERM, lasciando il drive in uno stato pulito.

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

**"Transport endpoint is not connected" su una directory di mount** (in `/media/$USER`,
`/run/media/$USER` o `~/Floppy`): è rimasta da un crash. Si pulisce con
`fusermount3 -uz <directory>`; il demone lo fa da solo all'avvio.

**Sul desktop il disco ha l'icona di una chiavetta USB**: il file manager non usa l'icona
fornita dal disco virtuale (vedi Compatibilità); nome e funzionamento non cambiano. Dopo
aver installato le icone può servire riavviare il file manager (`caja -q`, `nautilus -q`).

**L'icona nel tray non compare**: su Wayland, GNOME e KDE serve il pacchetto AppIndicator
(vedi Dipendenze); su GNOME anche l'estensione "AppIndicator and KStatusNotifierItem".

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
├── waffle-installer.py     – installer grafico (GTK o testo): utente + sistema via polkit
├── install-system.sh       – parte di sistema (root): /media o /run/media, porta seriale
├── install-desktop.sh      – launcher, icone e avvio automatico (usato dall'installer)
├── waffle-diskflashback.py – demone tray: automount, espulsione, copia e scrittura del disco
├── waffle-open-image.py    – apre un'immagine floppy come disco virtuale (doppio clic)
└── waffle-mount.sh         – monta/smonta con un clic (launcher)
waffle-installer.desktop    – apre l'installer con un doppio clic
waffle-open-image.desktop   – gestore delle immagini .adf/.img/.ima (non compare nel menu)
mime/waffle-fuse.xml        – tipi MIME per *.adf e *.ima dove il sistema non li definisce
icons/                      – icone dei dischi (incorporate anche nel binario) e del tray
tests/run-tests.sh          – test dei filesystem su immagini
```

---

## Limitazioni note

* Su AmigaDOS i nomi possono avere al massimo 30 caratteri (ISO-8859-1); i link (hard
  e soft) si leggono ma non si possono modificare.
* I volumi Amiga con bitmap non validata (disco da "validare") o con il checksum del blocco
  root sbagliato vengono montati in sola lettura (il motivo è scritto nel log): validali su
  un Amiga o in un emulatore. Per riparare un disco da un'immagine buona c'è `--write-image`.
* FAT32, settori diversi da 512 byte e dischi Amiga partizionati (RDB) non sono supportati.
* Un cambio di disco più veloce dell'intervallo di controllo (3 s) può non essere notato (il drive segnala il
  cambio solo con la linea DSKCHG): smonta sempre prima di cambiare disco.
* I file `.ipf` non sono supportati.
