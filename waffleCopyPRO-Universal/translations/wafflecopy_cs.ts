<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="cs_CZ">
<context>
    <name>DiagnosticThread</name>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="35"/>
        <source>Diagnostic interrupted by user.</source>
        <translation>Diagnostika přerušena uživatelem</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="111"/>
        <source>An unknown error occurred attempting to open access to the specified COM port.</source>
        <translation>Neznámá chyba při otevírání COM portu</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="113"/>
        <source>An unknown response was received from the Arduino.</source>
        <translation>Neznámá odpověď z Arduina</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="127"/>
        <source>&gt;&gt;&gt; PLEASE INSERT AN AMIGADOS DISK IN THE DRIVE &lt;&lt;&lt;</source>
        <translation>&gt;&gt;&gt; Vložte Amiga disk &lt;&lt;&lt;</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="128"/>
        <source>&gt;&gt;&gt; The disk will be used for testing &lt;&lt;&lt;</source>
        <translation>&gt;&gt;&gt; Pro test &lt;&lt;&lt;</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="136"/>
        <source>Attempting to open device: %1</source>
        <translation>Pokus o otevření zařízení: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="139"/>
        <source>ERROR: Could not open serial port for diagnostic: %1
</source>
        <translation>CHYBA: Nelze otevřít sériový port pro diagnostiku: %1
</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="144"/>
        <source>Device opened successfully.</source>
        <translation>Zařízení úspěšně otevřeno</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="151"/>
        <source>Waiting for a disk to be inserted...</source>
        <translation>Čekání na vložení disku...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="164"/>
        <source>Disk detected!</source>
        <translation>Disk detekován!</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="179"/>
        <source>ERROR: Timeout waiting for disk.</source>
        <translation>CHYBA: Timeout při čekání na disk</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="186"/>
        <source>Diagnostic interrupted by user during disk wait.</source>
        <translation>Diagnostika přerušena při čekání na disk</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="195"/>
        <source>ERROR: Could not read firmware version: %1</source>
        <translation>CHYBA: Nelze načíst verzi firmware: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="199"/>
        <source>Firmware Version: %1.%2.%3</source>
        <translation>Verze firmware: %1.%2.%3</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="209"/>
        <source>Testing CTS pin...</source>
        <translation>Test CTS pinu...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="212"/>
        <source>ERROR: CTS test failed: %1</source>
        <translation>CHYBA: Test CTS selhal: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="218"/>
        <source>CTS test passed.</source>
        <translation>Test CTS proběhl úspěšně</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="225"/>
        <source>Testing USB-&gt;Serial transfer speed...</source>
        <translation>Test rychlosti USB-&gt;Sériový přenos...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="228"/>
        <source>ERROR: Transfer speed test failed: %1</source>
        <translation>CHYBA: Test rychlosti přenosu selhal: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="234"/>
        <source>Transfer speed test passed.</source>
        <translation>Test rychlosti přenosu proběhl úspěšně</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="241"/>
        <source>Measuring Drive RPM...</source>
        <translation>Měření RPM mechaniky...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="248"/>
        <source>  RPM test skipped (requires firmware v1.9+)</source>
        <translation>  Test RPM přeskočen (vyžaduje firmware v1.9+)</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="254"/>
        <source>ERROR: RPM measurement failed: %1</source>
        <translation>CHYBA: Měření RPM selhalo: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="261"/>
        <source>Drive RPM: %1</source>
        <translation>RPM mechaniky: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="265"/>
        <source>WARNING: RPM is outside optimal range (%1-%2).</source>
        <translation>VAROVÁNÍ: RPM je mimo optimální rozsah (%1-%2)</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="275"/>
        <source>Performing track seek tests (Track 0 &lt;-&gt; Track 79)...</source>
        <translation>Provádění testu vyhledávání stop (Stopa 0 ↔ Stopa 79)...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="280"/>
        <location filename="../src/diagnosticthread.cpp" line="297"/>
        <source>Diagnostic interrupted during track seek test.</source>
        <translation>Diagnostika přerušena během testu vyhledávání stop</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="286"/>
        <source>  Seek iteration %1: Finding Track 0</source>
        <translation>  Iterace %1: Hledání stopy 0</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="288"/>
        <source>ERROR: Failed to find Track 0: %1</source>
        <translation>CHYBA: Nelze najít stopu 0: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="303"/>
        <source>  Seek iteration %1: Selecting Track 79</source>
        <translation>  Iterace %1: Výběr stopy 79</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="305"/>
        <source>ERROR: Failed to select Track 79: %1</source>
        <translation>CHYBA: Nelze vybrat stopu 79: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="319"/>
        <source>Reading Track 40 (Upper and Lower surfaces)...</source>
        <translation>Čtení stopy 40 (horní a dolní strana)...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="323"/>
        <source>ERROR: Failed to select Track 40: %1</source>
        <translation>CHYBA: Nelze vybrat stopu 40: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="330"/>
        <source>  Selecting Upper surface...</source>
        <translation>  Výběr horní strany...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="332"/>
        <source>ERROR: Failed to select Upper surface: %1</source>
        <translation>CHYBA: Nelze vybrat horní stranu: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="339"/>
        <source>  Reading Upper track data...</source>
        <translation>  Čtení dat horní stopy...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="342"/>
        <source>ERROR: Failed to read Upper track: %1</source>
        <translation>CHYBA: Nelze číst horní stopu: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="347"/>
        <source>  Upper track read successfully.</source>
        <translation>  Horní stopa úspěšně načtena</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="351"/>
        <source>Diagnostic interrupted during track read.</source>
        <translation>Diagnostika přerušena během čtení stopy</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="359"/>
        <source>  Selecting Lower surface...</source>
        <translation>  Výběr dolní strany...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="361"/>
        <source>ERROR: Failed to select Lower surface: %1</source>
        <translation>CHYBA: Nelze vybrat dolní stranu: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="367"/>
        <source>  Reading Lower track data...</source>
        <translation>  Čtení dat dolní stopy...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="370"/>
        <source>ERROR: Failed to read Lower track: %1</source>
        <translation>CHYBA: Nelze číst dolní stopu: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="375"/>
        <source>  Lower track read successfully.</source>
        <translation>  Dolní stopa úspěšně načtena</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="385"/>
        <source>Diagnostic tests completed.</source>
        <translation>Diagnostické testy dokončeny</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="388"/>
        <source>
=== DIAGNOSTIC TEST COMPLETED SUCCESSFULLY ===</source>
        <translation>
=== DIAGNOSTICKÝ TEST ÚSPĚŠNĚ DOKONČEN ===</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="390"/>
        <source>
=== DIAGNOSTIC TEST FAILED ===</source>
        <translation>
=== DIAGNOSTICKÝ TEST SELHAL ===</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../src/mainwindow.ui" line="26"/>
        <source>WAFFLE-Copy-Professional</source>
        <translation>WAFFLE-Copy-Professional</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="118"/>
        <source>START WRITE</source>
        <translation>Zápis</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="181"/>
        <source>START READ</source>
        <translation>Čtení</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="222"/>
        <source>select file name to read from Waffle</source>
        <translation>Soubor čtení</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="235"/>
        <location filename="../src/mainwindow.ui" line="261"/>
        <source>...</source>
        <translation>...</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="248"/>
        <source>select file image to write to Waffle</source>
        <translation>Obraz</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="298"/>
        <source>WAFFLE DRIVE USB PORT</source>
        <translation>USB Port</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="329"/>
        <source>STOP</source>
        <translation>STOP</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="379"/>
        <location filename="../src/mainwindow.cpp" line="893"/>
        <source>AMIGA DISK COPY COMPLETED</source>
        <translation>Dokončeno</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="404"/>
        <source>AMIGA DISK COPY ERROR</source>
        <translation>Chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="451"/>
        <source>PCW</source>
        <translation>PCW</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="489"/>
        <source>Erase Before Writing</source>
        <translation>Smazání</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="545"/>
        <source>Version</source>
        <translation>Ver</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="582"/>
        <source>Disk Error!</source>
        <translation>Chyba!</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="607"/>
        <source>Retry</source>
        <translation>Retry</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="614"/>
        <source>Skip</source>
        <translation>Skip</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="621"/>
        <source>Abort</source>
        <translation>Abort</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="667"/>
        <source>Run Diagnostics</source>
        <translation>Diagnostika</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="841"/>
        <source>UPPER SIDE</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="867"/>
        <source>LOWER SIDE</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="146"/>
        <source> --- WAFFLE COPY PROFESSIONAL v%1 --- The essential USB floppy drive solution for the real Amiga user.  It allows you to write from ADF, SCP and IPF files, and read floppy disks as ADF or SCP format and, thanks to a specific version of an Amiga Emulator Software, like WinUAE (by Toni Wilen) or AmiBerry (by MiDWaN), it works like a real Amiga disk drive allowing you to directly read and write your floppies through an emulator! Sometime you may need a special USB cable (Y-Type) with the possibility of double powering if the USB port of the PC is not powerful enough. Original Concept by Rob Smith, modified version by Gianluca Renzi, Waffle is a product by RetroBit Lab and RetroGiovedi.</source>
        <translation>WAFFLE v%1 - USB floppy pro Amigu. ADF/SCP/IPF zápis/čtení. Funguje s WinUAE, AmiBerry. Y-kabel možná potřeba. Rob Smith, Gianluca Renzi, RetroBit Lab.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="155"/>
        <source> --- CREDITS --- IPF support powered by CAPS image library. Copyright (C) Software Preservation Society. Music playback powered by libmikmod. Copyright (C) 1998-2004 Miodrag Vallat and others. Music: &quot;Stardust Memories&quot; by Jester/Sanity (C) 1992 Volker Tripp.</source>
        <translation>CREDITS: IPF CAPS library, libmikmod, Jester/Sanity.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="287"/>
        <source>File</source>
        <translation>File</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="289"/>
        <location filename="../src/mainwindow.cpp" line="1616"/>
        <source>Language Settings</source>
        <translation>Jazyk</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="292"/>
        <source>Quit</source>
        <translation>Exit</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="296"/>
        <source>Help</source>
        <translation>Help</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="298"/>
        <source>About</source>
        <translation>About</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="302"/>
        <source>Debug</source>
        <translation>Ladění</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="304"/>
        <source>Simulate Read/Write</source>
        <translation>Simulovat čtení/zápis</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="615"/>
        <source>OPERATION TERMINATED BY USER</source>
        <translation>Přerušeno</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="811"/>
        <location filename="../src/mainwindow.cpp" line="903"/>
        <location filename="../src/mainwindow.cpp" line="1238"/>
        <source>ERROR: No serial port selected!

Please select a serial port from the dropdown menu.</source>
        <translation>Chyba: Bez portu

Vyberte</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="818"/>
        <source>NEED VALID IMAGE FILENAME FIRST TO WRITE TO FLOPPY</source>
        <translation>Soubor nutný</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1061"/>
        <source>Select Disk Image File</source>
        <translation>Výběr</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1063"/>
        <source>Disk Images (*.adf *.ADF *.scp *.SCP *.ima *.IMA *.img *.IMG *.st *.ST *.ipf *.IPF);;ADF Files (*.adf *.ADF);;SCP Files (*.scp *.SCP);;IMA Files (*.ima *.IMA);;IMG Files (*.img *.IMG);;ST Files (*.st *.ST);;IPF Files (*.ipf *.IPF)</source>
        <translation>Obrazy (*.adf *.scp *.ima *.img *.st *.ipf)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1070"/>
        <source>Write Disk Image File to be written on hard disk</source>
        <translation>Uložit</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1072"/>
        <source>Disk Images (*.adf *.ADF *.scp *.SCP *.ima *.IMA *.img *.IMG *.st *.ST);;ADF Files (*.adf *.ADF);;SCP Files (*.scp *.SCP);;IMA Files (*.ima *.IMA);;IMG Files (*.img *.IMG);;ST Files (*.st *.ST)</source>
        <translation>Obrazy (*.adf *.scp *.ima *.img *.st)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1112"/>
        <source>Port In Use</source>
        <translation>Používán</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1113"/>
        <source>Port Not Found</source>
        <translation>Nenalezen</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1114"/>
        <source>Port Error</source>
        <translation>Port chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1115"/>
        <source>Access Denied</source>
        <translation>Odepřen</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1116"/>
        <source>Comport Config Error</source>
        <translation>Config chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1117"/>
        <source>BaudRate Not Supported</source>
        <translation>Baud chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1118"/>
        <source>Error Reading Version</source>
        <translation>Ver chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1119"/>
        <source>Error Malformed Version</source>
        <translation>Ver špatná</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1120"/>
        <source>Old Firmware</source>
        <translation>FW starý</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1122"/>
        <source>Send Failed</source>
        <translation>Odesílání selhalo</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1123"/>
        <source>Send Parameter Failed</source>
        <translation>Param selhání</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1124"/>
        <source>Read Response Failed or No Disk in Drive</source>
        <translation>Selhání/Chybí</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1125"/>
        <source>Write Timeout</source>
        <translation>Timeout</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1126"/>
        <source>Serial Overrun</source>
        <translation>Overrun</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1127"/>
        <source>Framing Error</source>
        <translation>Rámec chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1128"/>
        <source>Error</source>
        <translation>Chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1131"/>
        <source>Track Range Error</source>
        <translation>Rozsah chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1132"/>
        <source>Select Track Error</source>
        <translation>Výběr chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1133"/>
        <source>Write Protected</source>
        <translation>Chráněno</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1134"/>
        <source>Status Error</source>
        <translation>Status chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1135"/>
        <source>Send Data Failed</source>
        <translation>Data selhání</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1136"/>
        <source>Track Write Response Error</source>
        <translation>Odpověď chyba</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1139"/>
        <source>No Disk In Drive</source>
        <translation>Chybí disk</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1141"/>
        <source>Diagnostic Not Available</source>
        <translation>Diagnostika N/A</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1142"/>
        <source>USB Serial Bad</source>
        <translation>USB vadný</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1143"/>
        <source>CTS Failure</source>
        <translation>CTS selhání</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1144"/>
        <source>Rewind Failure</source>
        <translation>Rewind selhání</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1145"/>
        <source>Media Type Mismatch or No Disk in Drive</source>
        <translation>Typ špatný</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1146"/>
        <source>Unknown Error</source>
        <translation>Neznámá</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1270"/>
        <source>=== Waffle Copy Pro - Diagnostic Test ===
</source>
        <translation>=== Diagnostika ===
</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1271"/>
        <source>Starting diagnostic on port: %1
</source>
        <translation>Start: %1
</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1329"/>
        <source>
=== ALL TESTS PASSED ===</source>
        <translation>
=== Vše OK ===</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1331"/>
        <source>
=== SOME TESTS FAILED ===</source>
        <translation>
=== Selhání ===</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1333"/>
        <source>
Click anywhere on this window to close.</source>
        <translation>
Klik = zavřít</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1617"/>
        <source>Select language:</source>
        <translation>Jazyk:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1710"/>
        <source>Language Changed</source>
        <translation>Změněno</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1711"/>
        <source>Language has been changed. The application will now close. Please restart it.</source>
        <translation>Restart</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1719"/>
        <source>About Waffle Copy Pro</source>
        <translation>About</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1720"/>
        <source>&lt;h3&gt;Waffle Copy Professional v%1&lt;/h3&gt;&lt;p&gt;The essential USB floppy drive solution for the real Amiga user.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Original Concept:&lt;/b&gt; Rob Smith&lt;br&gt;&lt;b&gt;Modified version by:&lt;/b&gt; Gianluca Renzi&lt;br&gt;&lt;b&gt;Product by:&lt;/b&gt; RetroBit Lab and RetroGiovedi&lt;/p&gt;&lt;p&gt;IPF support powered by CAPS image library.&lt;br&gt;Music playback powered by libmikmod.&lt;/p&gt;</source>
        <translation>&lt;h3&gt;Waffle v%1&lt;/h3&gt;&lt;p&gt;USB floppy pro Amigu&lt;/p&gt;&lt;p&gt;Rob Smith, Gianluca Renzi&lt;br&gt;RetroBit Lab&lt;/p&gt;&lt;p&gt;IPF: CAPS, Hudba: libmikmod&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1732"/>
        <source>Simulation</source>
        <translation>Simulace</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1732"/>
        <source>Simulation already running</source>
        <translation>Simulace již běží</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1767"/>
        <source>Simulation Starting</source>
        <translation>Spouštění simulace</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1768"/>
        <source>Simulation will process all %1 tracks:

Track 0: Side 0 (Lower) then Side 1 (Upper)
Track 1: Side 0 (Lower) then Side 1 (Upper)
...and so on

Side 0 = Lower row (right side)
Side 1 = Upper row (left side)</source>
        <translation>Simulace zpracuje všech %1 stop:

Stopa 0: Strana 0 (Dolní) pak Strana 1 (Horní)
Stopa 1: Strana 0 (Dolní) pak Strana 1 (Horní)
...a tak dále

Strana 0 = Dolní řada (pravá strana)
Strana 1 = Horní řada (levá strana)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1818"/>
        <source>Simulation Complete</source>
        <translation>Simulace dokončena</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1819"/>
        <source>Simulation completed successfully.

%1 tracks processed (both sides: lower and upper)</source>
        <translation>Simulace úspěšně dokončena.

%1 stop zpracováno (obě strany: dolní a horní)</translation>
    </message>
</context>
</TS>
