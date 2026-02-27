<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="ja_JP">
<context>
    <name>DiagnosticThread</name>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="35"/>
        <source>Diagnostic interrupted by user.</source>
        <translation>診断を中断しました</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="111"/>
        <source>An unknown error occurred attempting to open access to the specified COM port.</source>
        <translation>COMポートへのアクセス中に不明なエラーが発生</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="113"/>
        <source>An unknown response was received from the Arduino.</source>
        <translation>Arduinoから不明な応答を受信</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="127"/>
        <source>&gt;&gt;&gt; PLEASE INSERT AN AMIGADOS DISK IN THE DRIVE &lt;&lt;&lt;</source>
        <translation>&gt;&gt;&gt; AmigaDOSディスク挿入 &lt;&lt;&lt;</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="128"/>
        <source>&gt;&gt;&gt; The disk will be used for testing &lt;&lt;&lt;</source>
        <translation>&gt;&gt;&gt; テスト用 &lt;&lt;&lt;</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="136"/>
        <source>Attempting to open device: %1</source>
        <translation>デバイスを開いています: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="139"/>
        <source>ERROR: Could not open serial port for diagnostic: %1
</source>
        <translation>エラー: 診断用シリアルポートを開けません: %1
</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="144"/>
        <source>Device opened successfully.</source>
        <translation>デバイスを正常に開きました</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="151"/>
        <source>Waiting for a disk to be inserted...</source>
        <translation>ディスク待機中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="164"/>
        <source>Disk detected!</source>
        <translation>ディスクを検出!</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="179"/>
        <source>ERROR: Timeout waiting for disk.</source>
        <translation>エラー: ディスク待機タイムアウト</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="186"/>
        <source>Diagnostic interrupted by user during disk wait.</source>
        <translation>ディスク待機中に診断を中断</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="195"/>
        <source>ERROR: Could not read firmware version: %1</source>
        <translation>エラー: ファームウェアバージョン読取失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="199"/>
        <source>Firmware Version: %1.%2.%3</source>
        <translation>ファームウェアバージョン: %1.%2.%3</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="209"/>
        <source>Testing CTS pin...</source>
        <translation>CTSピンをテスト中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="212"/>
        <source>ERROR: CTS test failed: %1</source>
        <translation>エラー: CTSテスト失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="218"/>
        <source>CTS test passed.</source>
        <translation>CTSテスト成功</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="225"/>
        <source>Testing USB-&gt;Serial transfer speed...</source>
        <translation>USB-シリアル転送速度をテスト中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="228"/>
        <source>ERROR: Transfer speed test failed: %1</source>
        <translation>エラー: 転送速度テスト失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="234"/>
        <source>Transfer speed test passed.</source>
        <translation>転送速度テスト成功</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="241"/>
        <source>Measuring Drive RPM...</source>
        <translation>ドライブRPMを測定中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="248"/>
        <source>  RPM test skipped (requires firmware v1.9+)</source>
        <translation>  RPMテストをスキップ(ファームウェアv1.9+が必要)</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="254"/>
        <source>ERROR: RPM measurement failed: %1</source>
        <translation>エラー: RPM測定失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="261"/>
        <source>Drive RPM: %1</source>
        <translation>ドライブRPM: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="265"/>
        <source>WARNING: RPM is outside optimal range (%1-%2).</source>
        <translation>警告: RPMが最適範囲外です(%1-%2)</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="275"/>
        <source>Performing track seek tests (Track 0 &lt;-&gt; Track 79)...</source>
        <translation>トラックシークテストを実行中(トラック0↔79)...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="280"/>
        <location filename="../src/diagnosticthread.cpp" line="297"/>
        <source>Diagnostic interrupted during track seek test.</source>
        <translation>トラックシークテスト中に診断を中断</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="286"/>
        <source>  Seek iteration %1: Finding Track 0</source>
        <translation>  シーク%1回目: トラック0を検索中</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="288"/>
        <source>ERROR: Failed to find Track 0: %1</source>
        <translation>エラー: トラック0を検出できません: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="303"/>
        <source>  Seek iteration %1: Selecting Track 79</source>
        <translation>  シーク%1回目: トラック79を選択中</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="305"/>
        <source>ERROR: Failed to select Track 79: %1</source>
        <translation>エラー: トラック79の選択に失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="319"/>
        <source>Reading Track 40 (Upper and Lower surfaces)...</source>
        <translation>トラック40を読取中(上面・下面)...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="323"/>
        <source>ERROR: Failed to select Track 40: %1</source>
        <translation>エラー: トラック40の選択に失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="330"/>
        <source>  Selecting Upper surface...</source>
        <translation>  上面を選択中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="332"/>
        <source>ERROR: Failed to select Upper surface: %1</source>
        <translation>エラー: 上面の選択に失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="339"/>
        <source>  Reading Upper track data...</source>
        <translation>  上面トラックデータを読取中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="342"/>
        <source>ERROR: Failed to read Upper track: %1</source>
        <translation>エラー: 上面トラックの読取に失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="347"/>
        <source>  Upper track read successfully.</source>
        <translation>  上面トラックの読取成功</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="351"/>
        <source>Diagnostic interrupted during track read.</source>
        <translation>トラック読取中に診断を中断</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="359"/>
        <source>  Selecting Lower surface...</source>
        <translation>  下面を選択中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="361"/>
        <source>ERROR: Failed to select Lower surface: %1</source>
        <translation>エラー: 下面の選択に失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="367"/>
        <source>  Reading Lower track data...</source>
        <translation>  下面トラックデータを読取中...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="370"/>
        <source>ERROR: Failed to read Lower track: %1</source>
        <translation>エラー: 下面トラックの読取に失敗: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="375"/>
        <source>  Lower track read successfully.</source>
        <translation>  下面トラックの読取成功</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="385"/>
        <source>Diagnostic tests completed.</source>
        <translation>診断テスト完了</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="388"/>
        <source>
=== DIAGNOSTIC TEST COMPLETED SUCCESSFULLY ===</source>
        <translation>
=== 診断テスト正常完了 ===</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="390"/>
        <source>
=== DIAGNOSTIC TEST FAILED ===</source>
        <translation>
=== 診断テスト失敗 ===</translation>
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
        <translation>書込</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="181"/>
        <source>START READ</source>
        <translation>読取</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="222"/>
        <source>select file name to read from Waffle</source>
        <translation>読取先選択</translation>
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
        <translation>書込元選択</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="298"/>
        <source>WAFFLE DRIVE USB PORT</source>
        <translation>USBポート</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="329"/>
        <source>STOP</source>
        <translation>停止</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="379"/>
        <location filename="../src/mainwindow.cpp" line="829"/>
        <source>AMIGA DISK COPY COMPLETED</source>
        <translation>完了</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="821"/>
        <source>SCP COPY COMPLETED</source>
        <translation>SCPコピー完了</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="823"/>
        <source>IPF COPY COMPLETED</source>
        <translation>IPFコピー完了</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="825"/>
        <source>ST COPY COMPLETED</source>
        <translation>STコピー完了</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="827"/>
        <source>PC-DOS COPY COMPLETED</source>
        <translation>PC-DOSコピー完了</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="404"/>
        <source>AMIGA DISK COPY ERROR</source>
        <translation>エラー</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="451"/>
        <source>PCW</source>
        <translation>PCW</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="489"/>
        <source>Erase Before Writing</source>
        <translation>事前消去</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="545"/>
        <source>Version</source>
        <translation>Ver</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="582"/>
        <source>Disk Error!</source>
        <translation>エラー!</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="607"/>
        <source>Retry</source>
        <translation>再実行</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="614"/>
        <source>Skip</source>
        <translation>スキップ</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="621"/>
        <source>Abort</source>
        <translation>中止</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="667"/>
        <source>Run Diagnostics</source>
        <translation>診断実行</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="841"/>
        <source>UPPER SIDE</source>
        <translation>上面</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="867"/>
        <source>LOWER SIDE</source>
        <translation>下面</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="156"/>
        <source> --- WAFFLE COPY PROFESSIONAL v%1 --- The essential USB floppy drive solution for the real Amiga user.  It allows you to write from ADF, SCP and IPF files, and read floppy disks as ADF or SCP format and, thanks to a specific version of an Amiga Emulator Software, like WinUAE (by Toni Wilen) or AmiBerry (by MiDWaN), it works like a real Amiga disk drive allowing you to directly read and write your floppies through an emulator! Sometime you may need a special USB cable (Y-Type) with the possibility of double powering if the USB port of the PC is not powerful enough. Original Concept by Rob Smith, modified version by Gianluca Renzi, Waffle is a product by RetroBit Lab and RetroGiovedi.</source>
        <translation> --- WAFFLE COPY PROFESSIONAL v%1 --- 本格的なAmigaユーザー向けの必須USBフロッピードライブソリューション。ADF、SCP、IPFファイルからの書込、フロッピーディスクのADFまたはSCP形式での読取が可能で、WinUAE（Toni Wilen作）やAmiBerry（MiDWaN作）などの特定バージョンのAmigaエミュレーターソフトウェアにより、実際のAmigaディスクドライブとして動作し、エミュレーターを通じて直接フロッピーの読み書きができます！PCのUSBポートの電力が不足する場合は、特殊なUSBケーブル（Y型）による二重電源が必要な場合があります。オリジナルコンセプト: Rob Smith、改良版: Gianluca Renzi、WaffleはRetroBit LabとRetroGiovediの製品です。</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="165"/>
        <source> --- CREDITS --- IPF support powered by CAPS image library. Copyright (C) Software Preservation Society. Music playback powered by libmikmod. Copyright (C) 1998-2004 Miodrag Vallat and others. Music: &quot;Stardust Memories&quot; by Jester/Sanity (C) 1992 Volker Tripp.</source>
        <translation> --- クレジット --- IPFサポート: CAPS image library提供。Copyright (C) Software Preservation Society。音楽再生: libmikmod提供。Copyright (C) 1998-2004 Miodrag Vallat and others。楽曲: &quot;Stardust Memories&quot; by Jester/Sanity (C) 1992 Volker Tripp。</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="297"/>
        <source>File</source>
        <translation>File</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="299"/>
        <location filename="../src/mainwindow.cpp" line="1687"/>
        <source>Language Settings</source>
        <translation>言語</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="302"/>
        <source>Quit</source>
        <translation>終了</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="306"/>
        <source>Help</source>
        <translation>Help</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="308"/>
        <source>About</source>
        <translation>About</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="313"/>
        <source>Debug</source>
        <translation>デバッグ</translation>
    </message>
    <message>
        <source>Simulate Read/Write</source>
        <translation type="vanished">読み取り/書き込みをシミュレート</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="629"/>
        <source>OPERATION TERMINATED BY USER</source>
        <translation>中止</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="470"/>
        <source>Skip Write Errors</source>
        <translation>書込エラー無視</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="508"/>
        <source>Tracks: 82</source>
        <translation>トラック：82</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="524"/>
        <source>Skip Read Errors</source>
        <translation>読込エラー無視</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="641"/>
        <source>HIGH DENSITY DISK SELECTION</source>
        <translation>高密度ディスク</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="315"/>
        <source>Simulate Read</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="317"/>
        <source>Simulate Write</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="846"/>
        <location filename="../src/mainwindow.cpp" line="939"/>
        <location filename="../src/mainwindow.cpp" line="1309"/>
        <source>ERROR: No serial port selected!

Please select a serial port from the dropdown menu.</source>
        <translation>エラー: ポート未選択

ポート選択してください</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="853"/>
        <source>NEED VALID IMAGE FILENAME FIRST TO WRITE TO FLOPPY</source>
        <translation>有効ファイル名必要</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1119"/>
        <source>Select Disk Image File</source>
        <translation>イメージ選択</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1121"/>
        <source>Disk Images (*.adf *.ADF *.scp *.SCP *.ima *.IMA *.img *.IMG *.st *.ST *.ipf *.IPF);;ADF Files (*.adf *.ADF);;SCP Files (*.scp *.SCP);;IMA Files (*.ima *.IMA);;IMG Files (*.img *.IMG);;ST Files (*.st *.ST);;IPF Files (*.ipf *.IPF)</source>
        <translation>ディスクイメージ (*.adf *.scp *.ima *.img *.st *.ipf);;ADFファイル (*.adf);;SCPファイル (*.scp);;IMAファイル (*.ima);;IMGファイル (*.img);;STファイル (*.st);;IPFファイル (*.ipf)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1128"/>
        <source>Write Disk Image File to be written on hard disk</source>
        <translation>保存先選択</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1130"/>
        <source>Disk Images (*.adf *.ADF *.scp *.SCP *.ima *.IMA *.img *.IMG *.st *.ST);;ADF Files (*.adf *.ADF);;SCP Files (*.scp *.SCP);;IMA Files (*.ima *.IMA);;IMG Files (*.img *.IMG);;ST Files (*.st *.ST)</source>
        <translation>ディスクイメージ (*.adf *.scp *.ima *.img *.st);;ADFファイル (*.adf);;SCPファイル (*.scp);;IMAファイル (*.ima);;IMGファイル (*.img);;STファイル (*.st)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1183"/>
        <source>Port In Use</source>
        <translation>使用中</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1184"/>
        <source>Port Not Found</source>
        <translation>未検出</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1185"/>
        <source>Port Error</source>
        <translation>Port失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1186"/>
        <source>Access Denied</source>
        <translation>拒否</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1187"/>
        <source>Comport Config Error</source>
        <translation>設定エラー</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1188"/>
        <source>BaudRate Not Supported</source>
        <translation>Baud未対応</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1189"/>
        <source>Error Reading Version</source>
        <translation>Ver読取失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1190"/>
        <source>Error Malformed Version</source>
        <translation>Ver形式エラー</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1191"/>
        <source>Old Firmware</source>
        <translation>FW旧</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1193"/>
        <source>Send Failed</source>
        <translation>送信失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1194"/>
        <source>Send Parameter Failed</source>
        <translation>送信失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1195"/>
        <source>Read Response Failed or No Disk in Drive</source>
        <translation>失敗/未挿入</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1196"/>
        <source>Write Timeout</source>
        <translation>Timeout</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1197"/>
        <source>Serial Overrun</source>
        <translation>Overrun</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1198"/>
        <source>Framing Error</source>
        <translation>Frame失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1199"/>
        <source>Error</source>
        <translation>エラー</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1202"/>
        <source>Track Range Error</source>
        <translation>Track範囲外</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1203"/>
        <source>Select Track Error</source>
        <translation>Track選択失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1204"/>
        <source>Write Protected</source>
        <translation>保護中</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1205"/>
        <source>Status Error</source>
        <translation>Status失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1206"/>
        <source>Send Data Failed</source>
        <translation>Data送信失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1207"/>
        <source>Track Write Response Error</source>
        <translation>書込応答失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1210"/>
        <source>No Disk In Drive</source>
        <translation>未挿入</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1212"/>
        <source>Diagnostic Not Available</source>
        <translation>診断機能は利用できません</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1213"/>
        <source>USB Serial Bad</source>
        <translation>USBシリアル不良</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1214"/>
        <source>CTS Failure</source>
        <translation>CTS故障</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1215"/>
        <source>Rewind Failure</source>
        <translation>巻戻失敗</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1216"/>
        <source>Media Type Mismatch or No Disk in Drive</source>
        <translation>不一致/未挿入</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1217"/>
        <source>Unknown Error</source>
        <translation>不明エラー</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1341"/>
        <source>=== Waffle Copy Pro - Diagnostic Test ===
</source>
        <translation>=== Waffle Copy Pro - 診断テスト ===
</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1342"/>
        <source>Starting diagnostic on port: %1
</source>
        <translation>ポート%1で診断を開始: 
</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1400"/>
        <source>
=== ALL TESTS PASSED ===</source>
        <translation>
=== 全テスト成功 ===</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1402"/>
        <source>
=== SOME TESTS FAILED ===</source>
        <translation>
=== 一部テスト失敗 ===</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1404"/>
        <source>
Click anywhere on this window to close.</source>
        <translation>
クリックで閉じる</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1688"/>
        <source>Select language:</source>
        <translation>言語:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1781"/>
        <source>Language Changed</source>
        <translation>変更済</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1782"/>
        <source>Language has been changed. The application will now close. Please restart it.</source>
        <translation>再起動必要</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1790"/>
        <source>About Waffle Copy Pro</source>
        <translation>About</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1791"/>
        <source>&lt;h3&gt;Waffle Copy Professional v%1&lt;/h3&gt;&lt;p&gt;The essential USB floppy drive solution for the real Amiga user.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Original Concept:&lt;/b&gt; Rob Smith&lt;br&gt;&lt;b&gt;Modified version by:&lt;/b&gt; Gianluca Renzi&lt;br&gt;&lt;b&gt;Product by:&lt;/b&gt; RetroBit Lab and RetroGiovedi&lt;/p&gt;&lt;p&gt;IPF support powered by CAPS image library.&lt;br&gt;Music playback powered by libmikmod.&lt;/p&gt;</source>
        <translation>&lt;h3&gt;Waffle Copy Professional v%1&lt;/h3&gt;&lt;p&gt;本格的なAmigaユーザー向けの必須USBフロッピードライブソリューション。&lt;/p&gt;&lt;p&gt;&lt;b&gt;オリジナルコンセプト:&lt;/b&gt; Rob Smith&lt;br&gt;&lt;b&gt;改良版:&lt;/b&gt; Gianluca Renzi&lt;br&gt;&lt;b&gt;製品:&lt;/b&gt; RetroBit Lab and RetroGiovedi&lt;/p&gt;&lt;p&gt;IPFサポート: CAPS image library提供。&lt;br&gt;音楽再生: libmikmod提供。&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1804"/>
        <location filename="../src/mainwindow.cpp" line="1849"/>
        <source>Simulation</source>
        <translation>シミュレーション</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1804"/>
        <location filename="../src/mainwindow.cpp" line="1849"/>
        <source>Simulation already running</source>
        <translation>シミュレーションは既に実行中です</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1836"/>
        <location filename="../src/mainwindow.cpp" line="1881"/>
        <source>Simulation Starting</source>
        <translation>シミュレーション開始</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1837"/>
        <source>Read simulation will process all %1 tracks:

Track 0: Side 0 (Lower) then Side 1 (Upper)
Track 1: Side 0 (Lower) then Side 1 (Upper)
...and so on

Side 0 = Lower row (right side)
Side 1 = Upper row (left side)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1882"/>
        <source>Write simulation will process all %1 tracks:

Track 0: Side 0 (Lower) then Side 1 (Upper)
Track 1: Side 0 (Lower) then Side 1 (Upper)
...and so on

Side 0 = Lower row (right side)
Side 1 = Upper row (left side)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1944"/>
        <source>%1 simulation completed successfully.

%2 tracks processed (both sides: lower and upper)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1945"/>
        <source>Write</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1945"/>
        <source>Read</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Simulation will process all %1 tracks:

Track 0: Side 0 (Lower) then Side 1 (Upper)
Track 1: Side 0 (Lower) then Side 1 (Upper)
...and so on

Side 0 = Lower row (right side)
Side 1 = Upper row (left side)</source>
        <translation type="vanished">シミュレーションは全%1トラックを処理します：

トラック0：サイド0（下）次にサイド1（上）
トラック1：サイド0（下）次にサイド1（上）
...続く

サイド0 = 下の行（右側）
サイド1 = 上の行（左側）</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1943"/>
        <source>Simulation Complete</source>
        <translation>シミュレーション完了</translation>
    </message>
    <message>
        <source>Simulation completed successfully.

%1 tracks processed (both sides: lower and upper)</source>
        <translation type="vanished">シミュレーションが正常に完了しました。

%1トラック処理済み（両面：下と上）</translation>
    </message>
</context>
</TS>
