<#
.SYNOPSIS
    Scarica un eseguibile da GitHub Release, lo installa in “C:\Program Files\wimo”
    e aggiunge automaticamente quella cartella alla variabile di sistema Path.

.DESCRIPTION
    Lo script:
      1. Verifica di avere i privilegi da Amministratore (se no, si interrompe).
      2. Crea la cartella “C:\Program Files\wimo” (se non esiste).
      3. Scarica l’exe in una posizione temporanea.
      4. Sposta (o rinomina) quel file in “C:\Program Files\wimo\wimo.exe”.
      5. Controlla se “C:\Program Files\wimo” è già nella variabile di sistema PATH; se no, lo aggiunge.
      6. Stampa un messaggio finale con le istruzioni per riavviare la sessione PowerShell (o il computer) per avere subito effetto.
#>

# Versione da scaricare; cambia solo questa per le release future
$WimoVersion = "v1.0.0-alpha.1"

# Costruisco dinamicamente l’URL
$DownloadUrl = "https://github.com/n-cupane/wimo/releases/download/$WimoVersion/wimo.exe"

function Test-IsAdministrator {
    $current = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $current.IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

if (-not (Test-IsAdministrator)) {
    Write-Warning "Devi eseguire questo script come Amministratore."
    exit 1
}

# 1) Preparo il path di installazione
$installDir = "C:\Program Files\wimo"
if (-not (Test-Path $installDir)) {
    try {
        New-Item -ItemType Directory -Path $installDir -ErrorAction Stop | Out-Null
        Write-Host "Creato directory di installazione: $installDir"
    } catch {
        Write-Error "Impossibile creare la cartella di installazione: $_"
        exit 1
    }
} else {
    Write-Host "Directory già esistente: $installDir"
}

# 2) Scarico l’exe in una cartella temporanea
$tempFile = [System.IO.Path]::GetTempFileName()
# Rinomino con .exe
$tempExe = [System.IO.Path]::ChangeExtension($tempFile, ".exe")

try {
    # Remove il file .tmp creato da GetTempFileName
    Remove-Item $tempFile -ErrorAction SilentlyContinue
    # Scarico con Invoke-WebRequest
    Write-Host "Scaricando dall’URL: $DownloadUrl ..."
    Invoke-WebRequest -Uri $DownloadUrl -OutFile $tempExe -UseBasicParsing -ErrorAction Stop
    Write-Host "Scaricato temporaneamente in: $tempExe"
} catch {
    Write-Error "Errore durante il download: $_"
    exit 1
}

# 3) Sposto/Rinomino in C:\Program Files\wimo\wimo.exe
$destExe = Join-Path $installDir ("wimo.exe")
try {
    # Se esiste già un file, lo sovrascrivo
    Copy-Item -Path $tempExe -Destination $destExe -Force -ErrorAction Stop
    Write-Host "Installato eseguibile in: $destExe"
} catch {
    Write-Error "Errore nello spostare l’exe in ${installDir}: $_"
    Remove-Item $tempExe -Force -ErrorAction SilentlyContinue
    exit 1
}

# Rimuovo il file temporaneo
Remove-Item $tempExe -Force -ErrorAction SilentlyContinue

# 4) Aggiungo $installDir alla variabile di sistema PATH (Machine)
#    se non è già presente.
$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
# Splitto su ';' e rimuovo eventuali spazi
$paths = $machinePath.Split(';') | ForEach-Object { $_.Trim() }
if ($paths -contains $installDir) {
    Write-Host "La cartella '$installDir' è già presente nella variabile PATH di sistema."
} else {
    # Aggiungo alla fine, separando con ';'
    $newPath = $machinePath.TrimEnd(';') + ";" + $installDir
    try {
        [Environment]::SetEnvironmentVariable("Path", $newPath, "Machine")
        Write-Host "Aggiunta '$installDir' alla variabile di sistema PATH."
    } catch {
        Write-Error "Impossibile aggiornare la variabile PATH di sistema: $_"
        exit 1
    }
}

Write-Host ""
Write-Host "=== Installazione completata con successo! ==="
Write-Host "Per rendere immediatamente effettive le modifiche alla variabile PATH,"
Write-Host "puoi chiudere tutte le finestre PowerShell/CMD aperte e riaprirne una nuova."