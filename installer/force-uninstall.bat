@echo off
REM =============================================================
REM  Simanta - Force Uninstall
REM
REM  Untuk komputer yang uninstall-nya stuck / nggak bisa dihapus.
REM  Jalankan sebagai Administrator.
REM =============================================================

echo ==========================================================
echo   Simanta Force Uninstall
echo ==========================================================
echo.

REM Auto-elevate ke admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Meminta hak Administrator...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo [1/8] Mematikan proses Simanta...
taskkill /f /im SimantaStudent.exe >nul 2>&1
taskkill /f /im SimantaTeacher.exe >nul 2>&1
taskkill /f /im unins000.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/8] Hapus firewall rules...
netsh advfirewall firewall delete rule name="Simanta Teacher TCP" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Teacher UDP" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Beacon Out" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Student TCP" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Student UDP" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Discovery In" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Block QUIC" >nul 2>&1
netsh advfirewall firewall delete rule name="Simanta Block TCP" >nul 2>&1

echo [3/8] Reset proxy/PAC...
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxySettingsPerUser /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Policies\Microsoft\Windows\CurrentVersion\Internet Settings" /v EnableLegacyAutoProxyFeatures /f >nul 2>&1
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings" /v AutoConfigURL /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v AutoConfigURL /f >nul 2>&1
reg add "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyEnable /t REG_DWORD /d 0 /f >nul 2>&1
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyEnable /t REG_DWORD /d 0 /f >nul 2>&1

echo [4/8] Enable Task Manager, Registry, Control Panel, USB...
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableTaskMgr /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableTaskMgr /f >nul 2>&1
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableRegistryTools /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\System" /v DisableRegistryTools /f >nul 2>&1
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer" /v NoControlPanel /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Policies\Explorer" /v NoControlPanel /f >nul 2>&1
reg add "HKLM\System\CurrentControlSet\Services\USBSTOR" /v Start /t REG_DWORD /d 3 /f >nul 2>&1

echo [4b/8] Hapus browser policy (DoH/QUIC/proxy lock)...
REM Hapus HANYA value yang Simanta tulis -- jangan key utuh karena policy
REM lain (Chrome Enterprise, antivirus, dll) bisa pakai key yang sama.
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

echo [5/8] Hapus autorun registry...
reg delete "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Run" /v SimantaStudent /f >nul 2>&1

echo [6/8] Hapus uninstall registry entry...
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}_is1" /f >nul 2>&1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}_is1" /f >nul 2>&1

echo [7/8] Hapus folder instalasi...
rmdir /s /q "%ProgramFiles%\Simanta" >nul 2>&1
rmdir /s /q "%ProgramFiles(x86)%\Simanta" >nul 2>&1

echo [8/8] Hapus Start Menu shortcuts...
rmdir /s /q "%ProgramData%\Microsoft\Windows\Start Menu\Programs\Simanta" >nul 2>&1
del /f /q "%PUBLIC%\Desktop\Simanta Teacher.lnk" >nul 2>&1
del /f /q "%USERPROFILE%\Desktop\Simanta Teacher.lnk" >nul 2>&1

echo.
echo ==========================================================
echo   SELESAI. Simanta sudah dihapus sepenuhnya.
echo   Restart komputer untuk memastikan semua bersih.
echo ==========================================================
echo.
pause
