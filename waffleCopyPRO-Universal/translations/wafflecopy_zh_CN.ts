<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>DiagnosticThread</name>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="35"/>
        <source>Diagnostic interrupted by user.</source>
        <translation>诊断被用户中断</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="111"/>
        <source>An unknown error occurred attempting to open access to the specified COM port.</source>
        <translation>打开指定COM端口时发生未知错误</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="113"/>
        <source>An unknown response was received from the Arduino.</source>
        <translation>从Arduino接收到未知响应</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="127"/>
        <source>&gt;&gt;&gt; PLEASE INSERT AN AMIGADOS DISK IN THE DRIVE &lt;&lt;&lt;</source>
        <translation>&gt;&gt;&gt; 请在驱动器中插入AmigaDOS磁盘 &lt;&lt;&lt;</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="128"/>
        <source>&gt;&gt;&gt; The disk will be used for testing &lt;&lt;&lt;</source>
        <translation>&gt;&gt;&gt; 此磁盘将用于测试 &lt;&lt;&lt;</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="136"/>
        <source>Attempting to open device: %1</source>
        <translation>尝试打开设备: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="139"/>
        <source>ERROR: Could not open serial port for diagnostic: %1
</source>
        <translation>错误: 无法打开串口进行诊断: %1
</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="144"/>
        <source>Device opened successfully.</source>
        <translation>设备打开成功</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="151"/>
        <source>Waiting for a disk to be inserted...</source>
        <translation>等待插入磁盘...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="164"/>
        <source>Disk detected!</source>
        <translation>检测到磁盘!</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="179"/>
        <source>ERROR: Timeout waiting for disk.</source>
        <translation>错误: 等待磁盘超时</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="186"/>
        <source>Diagnostic interrupted by user during disk wait.</source>
        <translation>等待磁盘时诊断被用户中断</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="195"/>
        <source>ERROR: Could not read firmware version: %1</source>
        <translation>错误: 无法读取固件版本: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="199"/>
        <source>Firmware Version: %1.%2.%3</source>
        <translation>固件版本: %1.%2.%3</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="209"/>
        <source>Testing CTS pin...</source>
        <translation>测试CTS引脚...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="212"/>
        <source>ERROR: CTS test failed: %1</source>
        <translation>错误: CTS测试失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="218"/>
        <source>CTS test passed.</source>
        <translation>CTS测试通过</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="225"/>
        <source>Testing USB-&gt;Serial transfer speed...</source>
        <translation>测试USB到串口传输速度...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="228"/>
        <source>ERROR: Transfer speed test failed: %1</source>
        <translation>错误: 传输速度测试失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="234"/>
        <source>Transfer speed test passed.</source>
        <translation>传输速度测试通过</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="241"/>
        <source>Measuring Drive RPM...</source>
        <translation>测量驱动器转速(RPM)...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="248"/>
        <source>  RPM test skipped (requires firmware v1.9+)</source>
        <translation>  跳过RPM测试(需固件v1.9+)</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="254"/>
        <source>ERROR: RPM measurement failed: %1</source>
        <translation>错误: RPM测量失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="261"/>
        <source>Drive RPM: %1</source>
        <translation>驱动器转速: %1 RPM</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="265"/>
        <source>WARNING: RPM is outside optimal range (%1-%2).</source>
        <translation>警告: RPM超出最佳范围(%1-%2)</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="275"/>
        <source>Performing track seek tests (Track 0 &lt;-&gt; Track 79)...</source>
        <translation>执行磁道寻道测试(磁道0↔磁道79)...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="280"/>
        <location filename="../src/diagnosticthread.cpp" line="297"/>
        <source>Diagnostic interrupted during track seek test.</source>
        <translation>磁道寻道测试时诊断被中断</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="286"/>
        <source>  Seek iteration %1: Finding Track 0</source>
        <translation>  寻道迭代%1: 查找磁道0</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="288"/>
        <source>ERROR: Failed to find Track 0: %1</source>
        <translation>错误: 查找磁道0失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="303"/>
        <source>  Seek iteration %1: Selecting Track 79</source>
        <translation>  寻道迭代%1: 选择磁道79</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="305"/>
        <source>ERROR: Failed to select Track 79: %1</source>
        <translation>错误: 选择磁道79失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="319"/>
        <source>Reading Track 40 (Upper and Lower surfaces)...</source>
        <translation>读取磁道40(上表面和下表面)...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="323"/>
        <source>ERROR: Failed to select Track 40: %1</source>
        <translation>错误: 选择磁道40失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="330"/>
        <source>  Selecting Upper surface...</source>
        <translation>  选择上表面...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="332"/>
        <source>ERROR: Failed to select Upper surface: %1</source>
        <translation>错误: 选择上表面失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="339"/>
        <source>  Reading Upper track data...</source>
        <translation>  读取上表面磁道数据...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="342"/>
        <source>ERROR: Failed to read Upper track: %1</source>
        <translation>错误: 读取上表面磁道失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="347"/>
        <source>  Upper track read successfully.</source>
        <translation>  上表面磁道读取成功</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="351"/>
        <source>Diagnostic interrupted during track read.</source>
        <translation>磁道读取时诊断被中断</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="359"/>
        <source>  Selecting Lower surface...</source>
        <translation>  选择下表面...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="361"/>
        <source>ERROR: Failed to select Lower surface: %1</source>
        <translation>错误: 选择下表面失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="367"/>
        <source>  Reading Lower track data...</source>
        <translation>  读取下表面磁道数据...</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="370"/>
        <source>ERROR: Failed to read Lower track: %1</source>
        <translation>错误: 读取下表面磁道失败: %1</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="375"/>
        <source>  Lower track read successfully.</source>
        <translation>  下表面磁道读取成功</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="385"/>
        <source>Diagnostic tests completed.</source>
        <translation>诊断测试完成</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="388"/>
        <source>
=== DIAGNOSTIC TEST COMPLETED SUCCESSFULLY ===</source>
        <translation>
=== 诊断测试成功完成 ===</translation>
    </message>
    <message>
        <location filename="../src/diagnosticthread.cpp" line="390"/>
        <source>
=== DIAGNOSTIC TEST FAILED ===</source>
        <translation>
=== 诊断测试失败 ===</translation>
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
        <translation>写</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="181"/>
        <source>START READ</source>
        <translation>读</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="222"/>
        <source>select file name to read from Waffle</source>
        <translation>选读取文件</translation>
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
        <translation>选写入文件</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="298"/>
        <source>WAFFLE DRIVE USB PORT</source>
        <translation>USB端口</translation>
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
        <translation>完成</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="821"/>
        <source>SCP COPY COMPLETED</source>
        <translation>SCP 复制完成</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="823"/>
        <source>IPF COPY COMPLETED</source>
        <translation>IPF 复制完成</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="825"/>
        <source>ST COPY COMPLETED</source>
        <translation>ST 复制完成</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="827"/>
        <source>PC-DOS COPY COMPLETED</source>
        <translation>PC-DOS 复制完成</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="404"/>
        <source>AMIGA DISK COPY ERROR</source>
        <translation>错误</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="451"/>
        <source>PCW</source>
        <translation>PCW</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="489"/>
        <source>Erase Before Writing</source>
        <translation>擦除</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="545"/>
        <source>Version</source>
        <translation>Ver</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="582"/>
        <source>Disk Error!</source>
        <translation>错误!</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="607"/>
        <source>Retry</source>
        <translation>重试</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="614"/>
        <source>Skip</source>
        <translation>跳过</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="621"/>
        <source>Abort</source>
        <translation>中止</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="667"/>
        <source>Run Diagnostics</source>
        <translation>诊断</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="841"/>
        <source>UPPER SIDE</source>
        <translation>上侧</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="867"/>
        <source>LOWER SIDE</source>
        <translation>下侧</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="156"/>
        <source> --- WAFFLE COPY PROFESSIONAL v%1 --- The essential USB floppy drive solution for the real Amiga user.  It allows you to write from ADF, SCP and IPF files, and read floppy disks as ADF or SCP format and, thanks to a specific version of an Amiga Emulator Software, like WinUAE (by Toni Wilen) or AmiBerry (by MiDWaN), it works like a real Amiga disk drive allowing you to directly read and write your floppies through an emulator! Sometime you may need a special USB cable (Y-Type) with the possibility of double powering if the USB port of the PC is not powerful enough. Original Concept by Rob Smith, modified version by Gianluca Renzi, Waffle is a product by RetroBit Lab and RetroGiovedi.</source>
        <translation> --- WAFFLE v%1 --- Amiga USB软驱方案。支持ADF/SCP/IPF写入和读取。兼容WinUAE/AmiBerry模拟器直接读写。USB供电不足时需Y型线。作者: Rob Smith/Gianluca Renzi。产品: RetroBit Lab/RetroGiovedi。</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="165"/>
        <source> --- CREDITS --- IPF support powered by CAPS image library. Copyright (C) Software Preservation Society. Music playback powered by libmikmod. Copyright (C) 1998-2004 Miodrag Vallat and others. Music: &quot;Stardust Memories&quot; by Jester/Sanity (C) 1992 Volker Tripp.</source>
        <translation> --- 致谢 --- IPF: CAPS library (C) SPS。音乐: libmikmod (C) 1998-2004 M.Vallat。曲: Stardust Memories - Jester/Sanity (C) 1992 V.Tripp。</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="297"/>
        <source>File</source>
        <translation>文件</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="299"/>
        <location filename="../src/mainwindow.cpp" line="1687"/>
        <source>Language Settings</source>
        <translation>语言</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="302"/>
        <source>Quit</source>
        <translation>退出</translation>
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
        <translation>调试</translation>
    </message>
    <message>
        <source>Simulate Read/Write</source>
        <translation type="vanished">模拟读/写</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="629"/>
        <source>OPERATION TERMINATED BY USER</source>
        <translation>中止</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="470"/>
        <source>Skip Write Errors</source>
        <translation>跳过写错误</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="508"/>
        <source>Tracks: 82</source>
        <translation>磁道：82</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="524"/>
        <source>Skip Read Errors</source>
        <translation>跳过读错误</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="641"/>
        <source>HIGH DENSITY DISK SELECTION</source>
        <translation>高密度选择</translation>
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
        <translation>错误: 未选端口

请选择</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="853"/>
        <source>NEED VALID IMAGE FILENAME FIRST TO WRITE TO FLOPPY</source>
        <translation>需有效文件</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1119"/>
        <source>Select Disk Image File</source>
        <translation>选镜像</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1121"/>
        <source>Disk Images (*.adf *.ADF *.scp *.SCP *.ima *.IMA *.img *.IMG *.st *.ST *.ipf *.IPF);;ADF Files (*.adf *.ADF);;SCP Files (*.scp *.SCP);;IMA Files (*.ima *.IMA);;IMG Files (*.img *.IMG);;ST Files (*.st *.ST);;IPF Files (*.ipf *.IPF)</source>
        <translation>镜像(*.adf *.scp *.ima *.img *.st *.ipf);;ADF(*.adf);;SCP(*.scp);;IMA(*.ima);;IMG(*.img);;ST(*.st);;IPF(*.ipf)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1128"/>
        <source>Write Disk Image File to be written on hard disk</source>
        <translation>保存文件</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1130"/>
        <source>Disk Images (*.adf *.ADF *.scp *.SCP *.ima *.IMA *.img *.IMG *.st *.ST);;ADF Files (*.adf *.ADF);;SCP Files (*.scp *.SCP);;IMA Files (*.ima *.IMA);;IMG Files (*.img *.IMG);;ST Files (*.st *.ST)</source>
        <translation>镜像(*.adf *.scp *.ima *.img *.st);;ADF(*.adf);;SCP(*.scp);;IMA(*.ima);;IMG(*.img);;ST(*.st)</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1183"/>
        <source>Port In Use</source>
        <translation>使用中</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1184"/>
        <source>Port Not Found</source>
        <translation>未找到端口</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1185"/>
        <source>Port Error</source>
        <translation>端口错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1186"/>
        <source>Access Denied</source>
        <translation>拒绝访问</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1187"/>
        <source>Comport Config Error</source>
        <translation>配置错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1188"/>
        <source>BaudRate Not Supported</source>
        <translation>波特率错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1189"/>
        <source>Error Reading Version</source>
        <translation>版本错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1190"/>
        <source>Error Malformed Version</source>
        <translation>版本格式错误</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1191"/>
        <source>Old Firmware</source>
        <translation>固件旧</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1193"/>
        <source>Send Failed</source>
        <translation>发送失败</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1194"/>
        <source>Send Parameter Failed</source>
        <translation>参数发送失败</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1195"/>
        <source>Read Response Failed or No Disk in Drive</source>
        <translation>失败/未插盘</translation>
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
        <translation>帧错误</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1199"/>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1202"/>
        <source>Track Range Error</source>
        <translation>Track范围错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1203"/>
        <source>Select Track Error</source>
        <translation>Track错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1204"/>
        <source>Write Protected</source>
        <translation>写保护</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1205"/>
        <source>Status Error</source>
        <translation>状态错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1206"/>
        <source>Send Data Failed</source>
        <translation>数据失败</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1207"/>
        <source>Track Write Response Error</source>
        <translation>写响应错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1210"/>
        <source>No Disk In Drive</source>
        <translation>未插盘</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1212"/>
        <source>Diagnostic Not Available</source>
        <translation>诊断不可用</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1213"/>
        <source>USB Serial Bad</source>
        <translation>USB故障</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1214"/>
        <source>CTS Failure</source>
        <translation>CTS故障</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1215"/>
        <source>Rewind Failure</source>
        <translation>回卷失败</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1216"/>
        <source>Media Type Mismatch or No Disk in Drive</source>
        <translation>类型错/未插</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1217"/>
        <source>Unknown Error</source>
        <translation>未知错</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1341"/>
        <source>=== Waffle Copy Pro - Diagnostic Test ===
</source>
        <translation>=== 诊断 ===
</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1342"/>
        <source>Starting diagnostic on port: %1
</source>
        <translation>启动: %1
</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1400"/>
        <source>
=== ALL TESTS PASSED ===</source>
        <translation>
=== 全通过 ===</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1402"/>
        <source>
=== SOME TESTS FAILED ===</source>
        <translation>
=== 部分失败 ===</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1404"/>
        <source>
Click anywhere on this window to close.</source>
        <translation>
点击关闭</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1688"/>
        <source>Select language:</source>
        <translation>语言:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1781"/>
        <source>Language Changed</source>
        <translation>已更改</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1782"/>
        <source>Language has been changed. The application will now close. Please restart it.</source>
        <translation>需重启</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1790"/>
        <source>About Waffle Copy Pro</source>
        <translation>About</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1791"/>
        <source>&lt;h3&gt;Waffle Copy Professional v%1&lt;/h3&gt;&lt;p&gt;The essential USB floppy drive solution for the real Amiga user.&lt;/p&gt;&lt;p&gt;&lt;b&gt;Original Concept:&lt;/b&gt; Rob Smith&lt;br&gt;&lt;b&gt;Modified version by:&lt;/b&gt; Gianluca Renzi&lt;br&gt;&lt;b&gt;Product by:&lt;/b&gt; RetroBit Lab and RetroGiovedi&lt;/p&gt;&lt;p&gt;IPF support powered by CAPS image library.&lt;br&gt;Music playback powered by libmikmod.&lt;/p&gt;</source>
        <translation>&lt;h3&gt;Waffle v%1&lt;/h3&gt;&lt;p&gt;Amiga USB软驱方案&lt;/p&gt;&lt;p&gt;&lt;b&gt;作者:&lt;/b&gt; Rob Smith&lt;br&gt;&lt;b&gt;改进:&lt;/b&gt; Gianluca Renzi&lt;br&gt;&lt;b&gt;产品:&lt;/b&gt; RetroBit Lab/RetroGiovedi&lt;/p&gt;&lt;p&gt;IPF: CAPS library&lt;br&gt;音乐: libmikmod&lt;/p&gt;</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1804"/>
        <location filename="../src/mainwindow.cpp" line="1849"/>
        <source>Simulation</source>
        <translation>模拟</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1804"/>
        <location filename="../src/mainwindow.cpp" line="1849"/>
        <source>Simulation already running</source>
        <translation>模拟已在运行</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1836"/>
        <location filename="../src/mainwindow.cpp" line="1881"/>
        <source>Simulation Starting</source>
        <translation>启动模拟</translation>
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
        <translation type="vanished">模拟将处理所有 %1 磁道：

磁道 0：面 0（下面）然后面 1（上面）
磁道 1：面 0（下面）然后面 1（上面）
...依此类推

面 0 = 下排（右侧）
面 1 = 上排（左侧）</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1943"/>
        <source>Simulation Complete</source>
        <translation>模拟完成</translation>
    </message>
    <message>
        <source>Simulation completed successfully.

%1 tracks processed (both sides: lower and upper)</source>
        <translation type="vanished">模拟成功完成。

已处理 %1 磁道（两面：下面和上面）</translation>
    </message>
</context>
</TS>
