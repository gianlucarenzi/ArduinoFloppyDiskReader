# 🛠️ Eseguire WaffleCopyPRO-Universal in modalità DEBUG (Windows)

Questa guida spiega in modo semplice e visuale come avviare l'applicazione in modalità debug su Windows e raccogliere i log da inviare per assistenza.

> Nota: "Modalità debug" stampa messaggi diagnostici su schermo e/o su file di log per aiutare a risolvere problemi.

---

## ✅ Prerequisiti

- Windows 10/11 con l'applicazione compilata (file eseguibile: `waffleCopyPRO-Universal.exe` o `waffleCopyPRO-Universal`).
- Driver seriale installati (per esempio driver FTDI se usi adattatori FTDI).
- Una cartella dove si trova l'eseguibile o una copia del programma.
- (Opzionale) Permessi di amministratore se l'accesso alla porta COM lo richiede.

---

## 🎯 Obiettivo

Avviare l'eseguibile in modalità debug, salvare l'output su file e ottenere i messaggi utili per il supporto tecnico.

---

## 🔎 Metodo semplice: usare Esplora file + PowerShell

1. Apri la cartella che contiene l'eseguibile in Esplora file.
2. Nella barra degli indirizzi di Esplora file digita `powershell` e premi Invio (o tieni premuto Shift, poi fai clic destro in un punto vuoto della cartella e scegli "Apri finestra PowerShell qui").
3. Nella finestra PowerShell, esegui il comando:

```powershell
# Se il file è waffleCopyPRO-Universal.exe
.\waffleCopyPRO-Universal.exe -debug > debug_output.txt 2>&1
```

- Questo comando avvia il programma con l'opzione `-debug` e scrive tutto l'output nel file `debug_output.txt` nella stessa cartella.
- Se l'eseguibile si chiama semplicemente `waffleCopyPRO-Universal`, usa `./waffleCopyPRO-Universal -debug > debug_output.txt 2>&1`.

4. Usa l'app come fai normalmente (esegui i passaggi che riproducono il problema). Quando hai finito, chiudi l'app; il file `debug_output.txt` conterrà i messaggi di debug.

---

## 🖥️ Metodo alternativo: usare Prompt dei comandi (cmd)

1. Apri la cartella contenente l'eseguibile.
2. Shift + clic destro → "Apri finestra di PowerShell qui" oppure dal menu cerca "Prompt dei comandi", poi naviga con `cd` alla cartella.
3. Esegui:

```cmd
waffleCopyPRO-Universal.exe -debug > debug_output.txt 2>&1
```

---

## 📌 Se preferisci vedere l'output direttamente sullo schermo

- Esegui semplicemente:

```powershell
.\waffleCopyPRO-Universal.exe -debug
```

- L'output comparirà nella finestra PowerShell; puoi copiarlo selezionando il testo e incollandolo in un file di testo.

---

## 🧭 Consigli pratici per utenti non esperti

- Se compare un errore relativo alla porta seriale (es. nessuna COM disponibile): controlla che il cavo sia collegato e che i driver siano installati.
- Per sapere quale COM usare, apri Gestione dispositivi → Porte (COM e LPT) e verifica il numero (es. `COM3`).
- Se il dispositivo usa FTDI e non appare come COM, il programma può mostrare etichette tipo `FTDI:...` — segnala questo nella richiesta di supporto.
- Se l'eseguibile richiede privilegi, clic destro → "Esegui come amministratore".

---

## 🧾 Come fornire i log al supporto

1. Invia il file `debug_output.txt` creato nella cartella dell'app (se troppo grande, allega le ultime 500-1000 righe).
2. Se il problema riguarda la porta seriale, indica: modello dell'adattatore (es. FTDI), numero COM mostrato in Gestione dispositivi, screenshot della sezione "Porte (COM e LPT)".
3. Eventualmente aggiungi una breve descrizione dei passi esatti che hai fatto per riprodurre il problema.

---

## ⚠️ Risoluzione problemi comuni

- Nulla appare nella combo delle porte seriali nell'app: assicurati che il dispositivo sia collegato _prima_ di avviare l'app, prova a scollegare/ricollegare e riavviare l'app.
- Messaggi di permesso negato / accesso negato: prova ad avviare il programma come amministratore.
- Non trovi l'eseguibile: cerca nella cartella `dist`, `build` o dove hai estratto/compilato il programma.

---

## ✉️ Esempio di messaggio da inviare al supporto

> Ciao, ho eseguito l'app in debug e allego `debug_output.txt`. Problema: la combo delle porte è vuota su Windows 10; dispositivo FTDI collegato e visibile come COM4 in Gestione dispositivi. Ho provato a riavviare ma il problema persiste.

---

Se vuoi, posso aggiungere schermate esempio o una checklist stampabile per utenti finali; dimmi se preferisci la versione in inglese o tradotta in altre lingue.