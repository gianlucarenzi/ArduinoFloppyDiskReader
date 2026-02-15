# Interfaccia Responsive e Supporto High DPI

## Panoramica

L'interfaccia di waffleCopyPRO-Universal è stata aggiornata per essere completamente responsive e supportare display ad alta risoluzione (High DPI). Tutte le modifiche sono state implementate mantenendo la compatibilità con il codice esistente e preservando l'aspetto visivo originale ispirato all'Amiga.

## Caratteristiche Implementate

### 1. Supporto High DPI Automatico
- **Attributi Qt abilitati**: `Qt::AA_EnableHighDpiScaling` e `Qt::AA_UseHighDpiPixmaps`
- **Posizionamento**: Gli attributi sono configurati prima della creazione di `QApplication` come richiesto da Qt
- **Risultato**: Su display 4K, Retina e altri schermi ad alta risoluzione, l'interfaccia viene automaticamente scalata per mantenere una dimensione visiva appropriata

### 2. Finestra Ridimensionabile
- **Dimensione minima**: 1024x610 pixel (dimensione originale del design)
- **Scalabilità**: La finestra può essere ridimensionata liberamente o massimizzata
- **Proporzioni**: Tutti gli elementi dell'interfaccia mantengono le proporzioni originali durante il ridimensionamento

### 3. Scalatura Dinamica degli Elementi UI

#### Elementi Scalati
Tutti i seguenti elementi vengono scalati proporzionalmente:

- **Loghi e immagini**: Logo sinistro/destro, titolo principale, immagini di sfondo
- **Campi di input**: Campi di testo per selezionare file ADF
- **Pulsanti**: START READ, START WRITE, STOP, Run Diagnostics
- **Checkbox**: Tutte le opzioni (Erase Before Writing, PCW, Skip Errors, HD Mode, ecc.)
- **Controlli porta seriale**: Selezione porta e etichette
- **Elementi di stato**: Messaggi di errore, completamento, overlay busy
- **Scroller di testo**: Il widget SineScroller con effetto ondulatorio
- **Finestra diagnostica**: Area di testo per i risultati dei test

#### Indicatori di Traccia (Quadrati Verdi/Rossi)
- **Posizionamento dinamico**: Tutti gli 84 indicatori di traccia vengono riposizionati e ridimensionati
- **Preservazione dello stato**: Durante il ridimensionamento, i colori (verde per successo, rosso per errore) e la visibilità vengono preservati
- **Funzionamento**: È possibile ridimensionare la finestra anche durante un'operazione di lettura/scrittura senza perdere il progresso visivo

#### VU Meter (Indicatore Audio)
- **Scalatura intelligente**: Il VU meter viene ridimensionato in base al numero di canali audio
- **Posizionamento centrato**: Rimane centrato orizzontalmente e ancorato in basso alla finestra
- **Aggiornamento in tempo reale**: Se visibile durante il ridimensionamento, si adatta automaticamente

### 4. Implementazione Tecnica

#### Costanti di Base
```cpp
static constexpr double BASE_WIDTH = 1024.0;
static constexpr double BASE_HEIGHT = 610.0;
```
Queste costanti definiscono le dimensioni originali del design e sono utilizzate per calcolare i fattori di scala.

#### Calcolo del Fattore di Scala
```cpp
double scaleX = currentWidth / BASE_WIDTH;
double scaleY = currentHeight / BASE_HEIGHT;
double scale = qMin(scaleX, scaleY);  // Mantiene le proporzioni
```

#### Funzione resizeEvent
La funzione `MainWindow::resizeEvent()` è stata completamente riscritta per:
1. Calcolare i fattori di scala basati sulle nuove dimensioni
2. Ridimensionare lo sfondo per riempire la finestra
3. Scalare tutti i widget UI utilizzando una lambda function efficiente
4. Aggiornare la posizione degli indicatori di traccia
5. Ridimensionare il VU meter se visibile

#### Preservazione dello Stato
La funzione `prepareTracksPosition()` è stata migliorata per:
- Salvare lo stato attuale di ogni indicatore (visibilità e colore)
- Applicare la nuova geometria scalata
- Ripristinare lo stato salvato invece di resettare tutto al nero nascosto

## Utilizzo

### Per l'Utente Finale
1. **Avviare l'applicazione**: Non sono necessarie configurazioni speciali
2. **Su display High DPI**: L'interfaccia si scala automaticamente alla risoluzione corretta
3. **Ridimensionare la finestra**: 
   - Trascinare i bordi della finestra per ridimensionare
   - Massimizzare la finestra per riempire lo schermo
   - Tutti gli elementi si adatteranno automaticamente

### Compatibilità
- **Windows**: Supporto completo per display High DPI e scaling percentuale di Windows
- **macOS**: Supporto nativo per display Retina e scaling su Intel/Apple Silicon
- **Linux**: Supporto per display HiDPI con configurazioni X11 e Wayland

## Test Consigliati

Per verificare il corretto funzionamento:

1. **Test di ridimensionamento base**: Ridimensionare la finestra e verificare che tutti gli elementi siano ancora visibili e utilizzabili

2. **Test durante operazione**: Avviare una lettura/scrittura di disco e ridimensionare la finestra mentre i quadrati verdi/rossi appaiono

3. **Test con VU meter**: Riprodurre musica MOD e ridimensionare la finestra per verificare che il VU meter si adatti

4. **Test su diversi DPI**: Testare su:
   - Display standard (96 DPI)
   - Display Full HD (120-144 DPI)
   - Display 4K (192 DPI)
   - Display Retina (220+ DPI)

## Limitazioni Note

1. **Cursore personalizzato**: Il cursore Amiga personalizzato non viene scalato e rimane alla dimensione originale
2. **Font Amiga**: Il font TopazPlus viene caricato con dimensione fissa (9pt) e non viene scalato dinamicamente per evitare problemi di compatibilità
3. **Proporzioni fisse**: L'interfaccia mantiene le proporzioni 1024:610 del design originale

## Modifiche ai File

### File Modificati
- `src/main.cpp`: Aggiunta abilitazione attributi High DPI
- `src/mainwindow.cpp`: 
  - Riscritta funzione `resizeEvent()`
  - Migliorata funzione `prepareTracksPosition()`
- `src/mainwindow.ui`: Rimossa la proprietà `maximumSize` dalla finestra principale
- `inc/mainwindow.h`: Aggiunte costanti `BASE_WIDTH` e `BASE_HEIGHT`
- `README.md`: Documentazione aggiornata con le nuove funzionalità

### Commit Principali
1. Implementazione scalatura dinamica UI e abilitazione High DPI
2. Correzione ordine attributi e aggiunta scaling immagini
3. Indirizzamento feedback code review (costanti, lambda, chiamata classe base)
4. Preservazione stato quadrati verdi/rossi durante ridimensionamento
5. Aggiunta scalatura VU meter durante ridimensionamento

## Sviluppi Futuri Possibili

- Scalatura del cursore personalizzato per display High DPI
- Opzione per scalare anche il font Amiga
- Layout alternativo per finestre molto larghe o alte
- Supporto per layout vertical/horizontal in base alle proporzioni della finestra
- Temi con dimensioni personalizzabili

## Conclusione

Tutte le modifiche sono state implementate con un approccio conservativo, minimizzando l'impatto sul codice esistente e mantenendo la piena compatibilità con le funzionalità precedenti. L'interfaccia ora offre un'esperienza ottimale su una vasta gamma di configurazioni di display, dalle risoluzioni standard ai moderni display 4K e oltre.
