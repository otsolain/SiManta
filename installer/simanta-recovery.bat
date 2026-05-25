@echo off
REM =============================================================
REM  Simanta - Recovery Script (untuk PC murid yang stuck)
REM
REM  Jalankan sebagai Administrator di PC murid yang:
REM    - internetnya diblokir dan Simanta sudah tidak jalan
REM    - tidak bisa buka situs apa pun walau teacher sudah tutup
REM    - Task Manager / Control Panel / Registry Editor terkunci
REM
REM  Double-click, klik "Yes" di prompt UAC.
REM =============================================================

echo ==========================================================
echo   Simanta Recovery - Reset semua pembatasan
echo ==========================================================
echo.

REM Auto-elevate ke admin kalau belum.
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Meminta hak Administrator...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo [1/7] Mematikan proses Simanta jika masih jalan...
taskkill /f /im SimantaStudent.exe >nul 2>&1
taskkill /f /im SimantaTeacher.exe >nul 2>&1

echo [2/7] Hapus firewall rule Simanta...
netsh advfirewall firewall delete rule name="Simanta Block QUIC" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Block TCP"  >nul 2>&1

echo [3/7] Reset PAC / AutoConfigURL proxy...
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxySettingsPerUser /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Windows\CurrentVersion\Internet Settings" /v EnableLegacyAutoProxyFeatures /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings"          /v AutoConfigURL /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings"          /v AutoConfigURL /f >nul 2>&1
reg add    "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings"          /v ProxyEnable /t REG_DWORD /d 0 /f >nul 2>&1
reg add    "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings"          /v ProxyEnable /t REG_DWORD /d 0 /f >nul 2>&1

echo [4/7] Enable Task Manager...
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableTaskMgr /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableTaskMgr /f >nul 2>&1

echo [5/7] Enable Registry Editor...
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableRegistryTools /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableRegistryTools /f >nul 2>&1

echo [6/7] Enable Control Panel...
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer" /v NoControlPanel /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer" /v NoControlPanel /f >nul 2>&1

echo [7/7] Enable USB storage...
reg add "HKLM\System\CurrentControlSet\Services\USBSTOR" /v Start /t REG_DWORD /d 3 /f >nul 2>&1

echo [7b/7] Hapus browser policy (DoH/QUIC/proxy lock)...
REM Hapus HANYA value yang Simanta tulis -- key utuh bisa dipakai policy lain.
reg delete "HKLM\SOFTWARE\Policies\Google\Chrome"            /v DnsOverHttpsMode /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Google\Chrome"            /v QuicAllowed      /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Google\Chrome"            /v ProxySettings    /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Edge"           /v DnsOverHttpsMode /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Edge"           /v QuicAllowed      /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Edge"           /v ProxySettings    /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\BraveSoftware\Brave"      /v DnsOverHttpsMode /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\BraveSoftware\Brave"      /v QuicAllowed      /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Mozilla\Firefox"          /v DNSOverHTTPS     /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Mozilla\Firefox\Proxy"    /f >nul 2>&1

echo.
echo ==========================================================
echo   SELESAI. Tutup semua browser lalu buka lagi.
echo   Kalau masih diblokir, restart PC.
echo ==========================================================
echo.
pause
