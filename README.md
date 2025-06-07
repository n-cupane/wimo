# Apri PowerShell come Amministratore, poi copia e incolla:
Set-ExecutionPolicy Bypass -Scope Process -Force; `
  iex ((New-Object System.Net.WebClient).DownloadString(
    'https://raw.githubusercontent.com/n-cupane/wimo/main/install/install-wimo.ps1'
  ))
