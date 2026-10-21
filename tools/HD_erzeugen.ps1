<#
HD_erzeugen.ps1 - H29: erzeugt alle HD-Grafiken aus der eigenen Spielkopie, ohne Vorkenntnisse.

Wird von HD_erzeugen.bat (liegt neben AT.exe) aufgerufen; im Release liegt dieses Skript in hd_tools\.
Schritte: Spielordner pruefen, Python finden (oder per winget installieren), eigene Python-Umgebung in
hd_tools\venv, Modelle laden (nur nach Zustimmung zu den Lizenzen), Animationen mit AT.exe --hd-export
exportieren, HD-Grafiken mit hd_generate.py rechnen, AT.json einstellen, Zwischendateien aufraeumen.
Jeder Schritt erkennt, was schon erledigt ist: Abbrechen mit Strg+C, ein neuer Start setzt fort.
Protokoll: hd_erzeugen.log im Spielordner.

Parameter (normalerweise nicht noetig):
  -Spiel ORDNER   Spielordner mit AT.exe (Standard: Ordner der .bat bzw. Ordner ueber hd_tools)
  -Nur LISTE      nur diese Kategorien (z. B. schriften,stadtfotos), sonst alles inkl. Animationen
  -Cpu            nicht auf der Grafikkarte rechnen
  -Ja             alle Rueckfragen mit Ja beantworten (fuer automatische Tests)
  -Behalten       am Ende nichts loeschen und nicht danach fragen
  -NichtWarten    am Ende nicht auf einen Tastendruck warten

Keine Spieldaten, Modelle oder HD-Grafiken gehoeren ins Git: alles entsteht nur im Spielordner.
#>
param(
    [string]$Spiel,
    [string]$Nur,
    [switch]$Cpu,
    [switch]$Ja,
    [switch]$Behalten,
    [switch]$NichtWarten
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$MindestVersion = [version]'2.0.3'
$PythonWinget = 'Python.Python.3.12'
$PythonPasst = @('3.12', '3.13', '3.11', '3.10')  # Reihenfolge = Vorzug; torch gibt es fuer diese Versionen
$Kategorien = @('schriften', 'stadtfotos', 'ui', 'raeume', 'bausteine', 'figuren', 'animationen')
# Rechenpakete fuer NVIDIA: hoechste CUDA-Version, die der Treiber kann (nvidia-smi: "CUDA Version: x.y"), zuerst
$CudaPakete = @(
    @{ Version = [version]'13.0'; Name = 'cu130' },
    @{ Version = [version]'12.8'; Name = 'cu128' },
    @{ Version = [version]'12.6'; Name = 'cu126' },
    @{ Version = [version]'11.8'; Name = 'cu118' }
)
# Modelle: Pruefsummen aus OpenModelDB bzw. vom Release der Autoren. Eine Kopie mit gleicher SHA-256 ist Byte fuer Byte
# die offizielle Datei. 4x-UltraSharp verteilt der Autor selbst ueber Mega (nicht automatisch ladbar), deshalb die
# Hugging-Face-Kopien; die Pruefsumme stellt sicher, dass es genau die Datei des Autors ist.
$Modelle = @(
    @{
        Kuerzel = 'ultrasharp'; Datei = '4x-UltraSharp.pth'; Name = '4x-UltraSharp (Kim2091)'
        Lizenz = 'CC BY-NC-SA 4.0: nur nicht-kommerziell, Weitergabe nur unter gleicher Lizenz'
        Info = 'https://openmodeldb.info/models/4x-UltraSharp'; Wofuer = 'Raeume, Oberflaeche, Bausteine, Schriften'
        Groesse = 66961958; Sha256 = 'a5812231fc936b42af08a5edba784195495d303d5b3248c24489ef0c4021fe01'
        Urls = @('https://huggingface.co/Kim2091/UltraSharp/resolve/main/4x-UltraSharp.pth',
                 'https://huggingface.co/lokCX/4x-Ultrasharp/resolve/main/4x-UltraSharp.pth',
                 'https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x-UltraSharp.pth')
    },
    @{
        Kuerzel = 'x4plus'; Datei = 'RealESRGAN_x4plus.pth'; Name = 'RealESRGAN_x4plus (xinntao/Real-ESRGAN, Release v0.1.0)'
        Lizenz = 'BSD-3-Clause'
        Info = 'https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.1.0'; Wofuer = 'Stadtfotos, Figuren, Animationen'
        Groesse = 67040989; Sha256 = '4fa0d38905f75ac06eb49a7951b426670021be3018265fd191d2125df9d682f1'
        Urls = @('https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth')
    }
)

# ---------------------------------------------------------------- Ordner

$Werkzeuge = $PSScriptRoot
if (-not $Spiel) {
    if ((Split-Path -Leaf $Werkzeuge) -ieq 'hd_tools') { $Spiel = Split-Path -Parent $Werkzeuge } else { $Spiel = (Get-Location).Path }
}
$Spiel = [IO.Path]::GetFullPath($Spiel.TrimEnd('\', '/', '.') + [IO.Path]::DirectorySeparatorChar)
$Spiel = $Spiel.TrimEnd([IO.Path]::DirectorySeparatorChar)
$Log = Join-Path $Spiel 'hd_erzeugen.log'
$Venv = Join-Path (Join-Path $Spiel 'hd_tools') 'venv'
$ModellOrdner = Join-Path $Spiel 'models'
$Arbeit = Join-Path $Spiel 'hd_assets'
$AtExe = Join-Path $Spiel 'AT.exe'
$Generator = Join-Path $Werkzeuge 'hd_generate.py'
$Anforderungen = Join-Path (Join-Path $Werkzeuge 'hdgen') 'requirements.txt'
$AufWindows = [Environment]::OSVersion.Platform -eq 'Win32NT'

# ---------------------------------------------------------------- Ausgabe und Log

$Utf8 = New-Object System.Text.UTF8Encoding($false)
function Log([string]$Text) {
    try { [IO.File]::AppendAllText($Log, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss ') + $Text + "`r`n", $Utf8) } catch { }
}
function Info([string]$Text, [string]$Farbe = 'Gray') { Write-Host $Text -ForegroundColor $Farbe; Log $Text }
function Hinweis([string]$Text) { Info $Text 'Yellow' }
function Fehler([string]$Text) {
    Write-Host ''
    Info "FEHLER: $Text" 'Red'
    Write-Host "Details stehen in $Log. Bei Rueckfragen bitte diese Datei mitschicken." -ForegroundColor Red
    throw [System.OperationCanceledException]::new('Fehler')
}

$SchrittNr = 0
$SchritteGesamt = 8
$Start = Get-Date
function Schritt([string]$Titel) {
    $script:SchrittNr++
    $vergangen = (Get-Date) - $Start
    Write-Host ''
    Info ("=== Schritt {0} von {1}: {2}   (bisher {3:hh\:mm\:ss})" -f $SchrittNr, $SchritteGesamt, $Titel, $vergangen) 'Cyan'
}

function Frage([string]$Text) {
    if ($Ja) { Info "$Text J (automatisch)"; return $true }
    while ($true) {
        $a = Read-Host "$Text (J/N)"
        Log "$Text -> $a"
        if ($a -match '^\s*[JjYy]') { return $true }
        if ($a -match '^\s*[Nn]') { return $false }
        Write-Host 'Bitte J (ja) oder N (nein) eingeben.'
    }
}

function Groesse([long]$Bytes) {
    if ($Bytes -ge 1GB) { return '{0:N1} GB' -f ($Bytes / 1GB) }
    return '{0:N0} MB' -f ($Bytes / 1MB)
}

function OrdnerGroesse([string]$Pfad) {
    if (-not (Test-Path -LiteralPath $Pfad)) { return 0 }
    $s = (Get-ChildItem -LiteralPath $Pfad -Recurse -File -Force -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum
    if ($s) { return [long]$s } else { return 0 }
}

# Externes Programm ausfuehren, Ausgabe auf den Bildschirm und ins Log; Rueckgabe: Exit-Code
function Ausfuehren([string]$Programm, [string[]]$Argumente, [switch]$Leise) {
    Log ("> " + $Programm + ' ' + ($Argumente -join ' '))
    $alt = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'  # Meldungen auf stderr sind bei pip normal und kein Abbruchgrund
    try {
        & $Programm @Argumente 2>&1 | ForEach-Object {
            $zeile = "$_"
            # Drehstrich und Fortschrittsbalken (winget, pip) nicht zeilenweise ausgeben
            if ($zeile -match '^\s*[-\\|/]?\s*$' -or $zeile -match '[0-9.]+ [KMG]B / [0-9.]+ [KMG]B') { return }
            if (-not $Leise) { Write-Host "    $zeile" }
            Log "    $zeile"
        }
        return $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $alt
    }
}

# Kurzer Aufruf, nur die Ausgabe zurueck (ohne Log)
function Abfragen([string]$Programm, [string[]]$Argumente) {
    $alt = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $out = & $Programm @Argumente 2>$null
        if ($LASTEXITCODE -ne 0) { return $null }
        return (($out | ForEach-Object { "$_" }) -join "`n").Trim()
    } catch {
        return $null
    } finally {
        $ErrorActionPreference = $alt
    }
}

function Warten {
    if ($NichtWarten -or $Ja) { return }
    Write-Host ''
    Read-Host 'Fertig. Enter druecken zum Schliessen' | Out-Null
}

function SpielLaeuft {
    $p = Get-Process -Name 'AT' -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -and ([IO.Path]::GetFullPath($_.Path) -ieq $AtExe) } catch { $true }
    }
    return [bool]$p
}

function SpielBeendenLassen([string]$Wofuer) {
    while (SpielLaeuft) {
        if ($Ja) { Fehler "Das Spiel laeuft noch. $Wofuer geht nur bei beendetem Spiel." }
        Hinweis "Das Spiel laeuft noch. Bitte beenden, $Wofuer geht nur bei beendetem Spiel."
        Read-Host 'Danach Enter druecken' | Out-Null
    }
}

# ---------------------------------------------------------------- Python

function PythonInfo([string]$Exe) {
    # Version und Bitbreite; der Windows-Store-Platzhalter (WindowsApps) startet nur den Store und zaehlt nicht
    if (-not $Exe -or -not (Test-Path -LiteralPath $Exe) -or $Exe -match '\\WindowsApps\\') { return $null }
    $v = Abfragen $Exe @('-c', 'import sys, struct; print(str(sys.version_info[0]) + chr(46) + str(sys.version_info[1]), struct.calcsize(chr(80)) * 8)')
    if (-not $v) { return $null }
    $t = $v -split '\s+'
    if ($t.Count -lt 2) { return $null }
    return @{ Exe = $Exe; Version = $t[0]; Bits = [int]$t[1] }
}

function PythonKandidaten {
    $liste = New-Object System.Collections.Generic.List[string]
    $py = Get-Command 'py' -ErrorAction SilentlyContinue
    if ($py) {
        $out = Abfragen $py.Source @('-0p')
        if ($out) {
            foreach ($z in ($out -split "`n")) {
                if ($z -match '([A-Za-z]:\\.*?python\.exe)') { $liste.Add($Matches[1]) }
            }
        }
    }
    foreach ($n in @('python', 'python3')) {
        foreach ($c in @(Get-Command $n -All -CommandType Application -ErrorAction SilentlyContinue)) { $liste.Add($c.Source) }
    }
    foreach ($wurzel in @("$env:LOCALAPPDATA\Programs\Python", "$env:ProgramFiles", "${env:ProgramFiles(x86)}")) {
        if ($wurzel -and (Test-Path -LiteralPath $wurzel)) {
            foreach ($d in @(Get-ChildItem -LiteralPath $wurzel -Directory -Filter 'Python3*' -ErrorAction SilentlyContinue)) {
                $liste.Add((Join-Path $d.FullName 'python.exe'))
            }
        }
    }
    foreach ($schluessel in @('HKCU:\Software\Python\PythonCore', 'HKLM:\Software\Python\PythonCore')) {
        foreach ($k in @(Get-ChildItem $schluessel -ErrorAction SilentlyContinue)) {
            $ip = Get-ItemProperty (Join-Path $k.PSPath 'InstallPath') -ErrorAction SilentlyContinue
            if ($ip -and $ip.PSObject.Properties['ExecutablePath']) { $liste.Add($ip.ExecutablePath) }
            elseif ($ip -and $ip.PSObject.Properties['(default)']) { $liste.Add((Join-Path $ip.'(default)' 'python.exe')) }
        }
    }
    return $liste | Select-Object -Unique
}

function PythonFinden {
    $gefunden = @()
    foreach ($k in (PythonKandidaten)) {
        $i = PythonInfo $k
        if ($i) { $gefunden += $i; Log "Python gefunden: $($i.Exe) ($($i.Version), $($i.Bits) Bit)" }
    }
    foreach ($v in $PythonPasst) {
        $p = $gefunden | Where-Object { $_.Version -eq $v -and $_.Bits -eq 64 } | Select-Object -First 1
        if ($p) { return $p }
    }
    if ($gefunden) {
        Hinweis ("Gefundenes Python passt nicht (" + (($gefunden | ForEach-Object { "$($_.Version) $($_.Bits) Bit" }) -join ', ') +
                 "). Noetig ist Python " + ($PythonPasst -join ', ') + " in 64 Bit.")
    }
    return $null
}

function PythonInstallieren {
    $winget = Get-Command 'winget' -ErrorAction SilentlyContinue
    $anleitung = "Bitte Python 3.12 (64 Bit) von https://www.python.org/downloads/windows/ installieren " +
                 "(im Installer den Haken bei 'Add python.exe to PATH' setzen) und HD_erzeugen.bat danach erneut starten."
    if (-not $winget) {
        Hinweis 'Python ist nicht installiert, und winget (App-Installer von Microsoft) fehlt.'
        Fehler $anleitung
    }
    Info 'Python ist nicht installiert. Es kann jetzt mit winget (Paketverwaltung von Windows) installiert werden:'
    Info "  winget install $PythonWinget (nur fuer diesen Benutzer, keine Administratorrechte noetig)"
    if (-not (Frage 'Python jetzt installieren?')) { Fehler $anleitung }
    $rc = Ausfuehren $winget.Source @('install', '--id', $PythonWinget, '-e', '--scope', 'user', '--silent',
                                      '--accept-package-agreements', '--accept-source-agreements', '--disable-interactivity')
    Log "winget: Exit-Code $rc"
    $p = PythonFinden
    if (-not $p) { Fehler ("Python wurde nicht gefunden (winget Exit-Code $rc). " + $anleitung) }
    return $p
}

# ---------------------------------------------------------------- Herunterladen

function Herunterladen([string]$Url, [string]$Ziel, [long]$Erwartet) {
    Add-Type -AssemblyName System.Net.Http
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $client = New-Object System.Net.Http.HttpClient
    $client.Timeout = [TimeSpan]::FromMinutes(30)
    $client.DefaultRequestHeaders.UserAgent.ParseAdd('AirlineTycoon-HD_erzeugen/1.0')
    try {
        $antwort = $client.GetAsync($Url, [System.Net.Http.HttpCompletionOption]::ResponseHeadersRead).GetAwaiter().GetResult()
        if (-not $antwort.IsSuccessStatusCode) { Log "  $Url -> HTTP $([int]$antwort.StatusCode)"; return $false }
        $gesamt = $antwort.Content.Headers.ContentLength
        if (-not $gesamt) { $gesamt = $Erwartet }
        $quelle = $antwort.Content.ReadAsStreamAsync().GetAwaiter().GetResult()
        $datei = [IO.File]::Create($Ziel)
        try {
            $puffer = New-Object byte[] (1MB)
            $gelesen = [long]0
            $letzte = -1
            while (($n = $quelle.Read($puffer, 0, $puffer.Length)) -gt 0) {
                $datei.Write($puffer, 0, $n)
                $gelesen += $n
                $prozent = [int](100 * $gelesen / [Math]::Max($gesamt, 1))
                if ($prozent -ne $letzte -and $prozent % 10 -eq 0) { Write-Host ("    {0,3}% ({1})" -f $prozent, (Groesse $gelesen)); $letzte = $prozent }
            }
        } finally {
            $datei.Close()
            $quelle.Close()
        }
        return $true
    } catch {
        Log "  $Url -> $($_.Exception.Message)"
        return $false
    } finally {
        $client.Dispose()
    }
}

function Sha256([string]$Pfad) { return (Get-FileHash -LiteralPath $Pfad -Algorithm SHA256).Hash.ToLowerInvariant() }

# ---------------------------------------------------------------- Ablauf

$fertig = $false
try {
    Write-Host ''
    Info "HD-Grafiken fuer Airline Tycoon Deluxe erzeugen  ($(Get-Date -Format 'yyyy-MM-dd HH:mm'))" 'Green'
    Write-Host 'Abbrechen jederzeit mit Strg+C. Ein neuer Doppelklick auf HD_erzeugen.bat setzt an der Stelle fort.'
    Log "Spielordner: $Spiel; Parameter: Nur=$Nur Cpu=$Cpu Ja=$Ja Behalten=$Behalten; PowerShell $($PSVersionTable.PSVersion); $([Environment]::OSVersion.VersionString)"

    if ($Nur) {
        $auswahl = @($Nur -split '[,; ]+' | Where-Object { $_ })
        $falsch = @($auswahl | Where-Object { $Kategorien -notcontains $_ })
        if ($falsch) { Fehler ("Unbekannte Kategorie: " + ($falsch -join ', ') + ". Erlaubt: " + ($Kategorien -join ', ')) }
    } else {
        $auswahl = $Kategorien
    }
    $mitAnimationen = $auswahl -contains 'animationen'

    # ------------------------------------------------ 1. Spielordner
    Schritt 'Spielordner pruefen'
    Info "Spielordner: $Spiel"
    if (-not (Test-Path -LiteralPath $AtExe)) {
        Fehler "Im Spielordner liegt keine AT.exe. HD_erzeugen.bat gehoert zusammen mit der neuen AT.exe (Release 2.0.3 oder neuer) in den Spielordner, in dem auch die Ordner gli, room und misc liegen."
    }
    $vi = (Get-Item -LiteralPath $AtExe).VersionInfo
    $atVersion = [version]('{0}.{1}.{2}' -f $vi.FileMajorPart, $vi.FileMinorPart, $vi.FileBuildPart)
    Info "AT.exe: Version $atVersion"
    if ($atVersion -lt $MindestVersion) {
        Fehler "Die AT.exe ist zu alt ($atVersion). Bitte das aktuelle Release (mindestens $MindestVersion) in den Spielordner entpacken."
    }
    if (-not (Test-Path -LiteralPath $Generator)) { Fehler "Der Generator fehlt: $Generator. Bitte das Release-Archiv vollstaendig entpacken." }
    $gli = @(Get-ChildItem -LiteralPath (Join-Path $Spiel 'gli') -Filter '*.gli' -File -ErrorAction SilentlyContinue)
    $mcf = @(Get-ChildItem -LiteralPath (Join-Path $Spiel 'misc') -Filter '*.mcf' -File -ErrorAction SilentlyContinue)
    if ($gli.Count -eq 0) {
        Fehler "Keine Spieldaten gefunden (Ordner gli mit .gli-Dateien). Das Release gehoert in den Ordner der installierten Spielkopie (GOG oder Steam)."
    }
    Info "Spieldaten: $($gli.Count) Grafikarchive in gli, $($mcf.Count) Schriften in misc"
    $video = Join-Path $Spiel 'video'
    $clips = @(Get-ChildItem -LiteralPath $video -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.smk', '.flc' })
    if ($mitAnimationen -and $clips.Count -eq 0) {
        Hinweis 'Keine Animationen (Ordner video) gefunden; die Animationen werden uebersprungen.'
        $mitAnimationen = $false
        $auswahl = @($auswahl | Where-Object { $_ -ne 'animationen' })
    }
    $schreibbar = $true
    try {
        $probe = Join-Path $Spiel 'hd_erzeugen.probe'
        [IO.File]::WriteAllText($probe, 'ok')
        Remove-Item -LiteralPath $probe -Force
    } catch { $schreibbar = $false }
    if (-not $schreibbar) {
        Fehler "In den Spielordner kann nicht geschrieben werden (z. B. unter C:\Program Files). Bitte HD_erzeugen.bat mit Rechtsklick 'Als Administrator ausfuehren' starten oder das Spiel in einen anderen Ordner (z. B. C:\Spiele) installieren."
    }
    $frei = $null
    try { $frei = (New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($Spiel))).AvailableFreeSpace } catch { Log "Freier Platz nicht ermittelbar: $($_.Exception.Message)" }
    if ($null -ne $frei) {
        Info "Freier Platz: $(Groesse $frei)"
        if ($frei -lt 15GB) {
            Hinweis 'Benoetigt werden etwa 15 GB (Python-Umgebung, Zwischendateien, HD-Grafiken). Ein Teil wird am Ende wieder frei.'
            if (-not (Frage 'Trotzdem fortfahren?')) { Fehler 'Zu wenig freier Speicherplatz.' }
        }
    }

    # ------------------------------------------------ 2. Python
    Schritt 'Python suchen'
    $venvPython = Join-Path (Join-Path $Venv 'Scripts') 'python.exe'
    if (-not $AufWindows) { $venvPython = Join-Path (Join-Path $Venv 'bin') 'python' }
    $venvOk = PythonInfo $venvPython
    if ($venvOk) {
        Info "Eigene Python-Umgebung vorhanden: $Venv (Python $($venvOk.Version))"
    } else {
        $python = PythonFinden
        if (-not $python) { $python = PythonInstallieren }
        Info "Python: $($python.Exe) (Version $($python.Version))"
    }

    # ------------------------------------------------ 3. Python-Umgebung
    Schritt 'Python-Umgebung einrichten (hd_tools\venv)'
    $gpuName = $null
    $treiberCuda = $null
    $smi = Get-Command 'nvidia-smi' -ErrorAction SilentlyContinue
    if (-not $Cpu -and $smi) {
        $s = Abfragen $smi.Source @()
        if ($s -and $s -match 'CUDA Version:\s*([0-9]+\.[0-9]+)') { $treiberCuda = [version]$Matches[1] }
        $gpuName = Abfragen $smi.Source @('--query-gpu=name', '--format=csv,noheader')
        if ($gpuName) { $gpuName = ($gpuName -split "`n")[0] }
        Info "NVIDIA-Grafikkarte: $gpuName (Treiber kann CUDA $treiberCuda)"
    } elseif ($Cpu) {
        Info 'Grafikkarte wird nicht verwendet (-Cpu).'
    } else {
        Info 'Keine NVIDIA-Grafikkarte gefunden (nvidia-smi fehlt): gerechnet wird auf dem Prozessor. Das dauert viele Stunden.'
    }
    $varianten = @()
    if ($treiberCuda) { $varianten += @($CudaPakete | Where-Object { $_.Version -le $treiberCuda } | ForEach-Object { $_.Name }) }
    $varianten += 'cpu'
    $anfHash = (Get-FileHash -LiteralPath $Anforderungen -Algorithm SHA256).Hash.Substring(0, 16)
    $kennung = Join-Path $Venv 'hd_pakete.txt'
    $soll = "$($varianten[0])|$anfHash"
    $ist = $null
    if ((Test-Path -LiteralPath $kennung) -and $venvOk) { $ist = (Get-Content -LiteralPath $kennung -Raw).Trim() }
    $test = 'import numpy, PIL, torch, spandrel; print(torch.__version__, 1 if torch.cuda.is_available() else 0)'
    $torchInfo = $null
    if ($ist -eq $soll) { $torchInfo = Abfragen $venvPython @('-c', $test) }
    if ($torchInfo) {
        Info "Pakete schon installiert (torch $torchInfo)."
    } else {
        if (-not $venvOk) {
            Info "Lege die Python-Umgebung an: $Venv"
            $rc = Ausfuehren $python.Exe @('-m', 'venv', $Venv)
            if ($rc -ne 0 -or -not (PythonInfo $venvPython)) { Fehler "Die Python-Umgebung konnte nicht angelegt werden (Exit-Code $rc)." }
        }
        Info 'Installiere die Pakete (numpy, pillow, torch, spandrel). Mit Grafikkarte sind das einige GB, bitte Geduld.'
        Ausfuehren $venvPython @('-m', 'pip', 'install', '--upgrade', 'pip', '--disable-pip-version-check') -Leise | Out-Null
        $installiert = $null
        foreach ($v in $varianten) {
            Info "torch fuer $v ..."
            $rc = Ausfuehren $venvPython @('-m', 'pip', 'install', '--disable-pip-version-check', '--progress-bar', 'off', 'torch', 'torchvision',
                                           '--index-url', "https://download.pytorch.org/whl/$v")
            if ($rc -eq 0) { $installiert = $v; break }
            Hinweis "torch fuer $v liess sich nicht installieren (Exit-Code $rc)."
        }
        if (-not $installiert) { Fehler 'torch konnte nicht installiert werden (Internetverbindung?).' }
        $rc = Ausfuehren $venvPython @('-m', 'pip', 'install', '--disable-pip-version-check', '--progress-bar', 'off', '-r', $Anforderungen)
        if ($rc -ne 0) { Fehler "Die Pakete aus requirements.txt liessen sich nicht installieren (Exit-Code $rc)." }
        $torchInfo = Abfragen $venvPython @('-c', $test)
        if (-not $torchInfo) { Fehler 'Die installierten Pakete lassen sich nicht laden.' }
        [IO.File]::WriteAllText($kennung, "$($varianten[0])|$anfHash", $Utf8)
        Info "Pakete installiert (torch $torchInfo)."
    }
    $mitGpu = (-not $Cpu) -and ($torchInfo -match ' 1$')
    if ($mitGpu) { Info "Gerechnet wird auf der Grafikkarte ($gpuName)." 'Green' }
    elseif ($treiberCuda) { Hinweis 'torch sieht die Grafikkarte nicht (Treiber zu alt?). Gerechnet wird auf dem Prozessor.' }

    # ------------------------------------------------ 4. Modelle
    Schritt 'Modelle'
    New-Item -ItemType Directory -Force -Path $ModellOrdner | Out-Null
    $fehlend = @()
    foreach ($m in $Modelle) {
        $p = Join-Path $ModellOrdner $m.Datei
        if ((Test-Path -LiteralPath $p) -and ((Sha256 $p) -eq $m.Sha256)) {
            Info "$($m.Datei): vorhanden, Pruefsumme stimmt."
        } else {
            if (Test-Path -LiteralPath $p) { Hinweis "$($m.Datei): Pruefsumme stimmt nicht, wird neu geladen." }
            $fehlend += $m
        }
    }
    if ($fehlend) {
        Write-Host ''
        Info 'Zum Hochrechnen werden zwei KI-Modelle gebraucht. Sie stammen nicht von uns und haben eigene Lizenzen:' 'White'
        foreach ($m in $fehlend) {
            Info "  $($m.Name)" 'White'
            Info "    Lizenz: $($m.Lizenz)"
            Info "    Infos:  $($m.Info)"
            Info "    Fuer:   $($m.Wofuer)"
        }
        Info 'Die HD-Grafiken sind nur fuer den privaten Gebrauch und duerfen nicht weitergegeben werden.' 'White'
        if (-not (Frage 'Lizenzen gelesen und einverstanden, Modelle jetzt herunterladen?')) {
            Fehler "Ohne Modelle geht es nicht. Alternativ die Dateien selbst herunterladen und nach $ModellOrdner legen, dann neu starten."
        }
        foreach ($m in $fehlend) {
            $ziel = Join-Path $ModellOrdner $m.Datei
            $tmp = "$ziel.download"
            $ok = $false
            foreach ($u in $m.Urls) {
                Info "Lade $($m.Datei) von $u"
                if (-not (Herunterladen $u $tmp $m.Groesse)) { Hinweis '  nicht erreichbar, versuche die naechste Quelle.'; continue }
                $h = Sha256 $tmp
                if ($h -ne $m.Sha256) { Hinweis "  Pruefsumme falsch ($h), Datei verworfen."; Remove-Item -LiteralPath $tmp -Force; continue }
                Move-Item -LiteralPath $tmp -Destination $ziel -Force
                Info "  Pruefsumme stimmt (SHA-256 $h)."
                $ok = $true
                break
            }
            if (-not $ok) {
                Fehler "$($m.Datei) liess sich nicht laden. Bitte selbst von $($m.Info) herunterladen und nach $ModellOrdner legen (SHA-256 $($m.Sha256)), dann neu starten."
            }
        }
    }

    # ------------------------------------------------ 5. Animationen exportieren
    Schritt 'Animationen aus dem Spiel exportieren'
    $exportOrdner = Join-Path (Join-Path $Arbeit 'original') 'video'
    $marke = Join-Path $exportOrdner 'export_fertig.txt'
    if (-not $mitAnimationen) {
        Info 'Nicht gewaehlt, uebersprungen.'
    } elseif ((Test-Path -LiteralPath $marke) -and ((Get-Content -LiteralPath $marke -Raw) -match 'auswahl;alle')) {
        Info 'Schon exportiert.'
    } else {
        $inhalt = [IO.File]::ReadAllBytes($AtExe)
        if ([Text.Encoding]::ASCII.GetString($inhalt).IndexOf('--hd-export') -lt 0) {
            Fehler 'Diese AT.exe kennt den Export-Parameter --hd-export noch nicht. Bitte das aktuelle Release entpacken.'
        }
        $inhalt = $null
        SpielBeendenLassen 'Der Export'
        Info 'Starte AT.exe --hd-export alle (ohne Fenster; dauert einige Minuten) ...'
        $proz = Start-Process -FilePath $AtExe -ArgumentList @('--hd-export', 'alle') -WorkingDirectory $Spiel -PassThru
        $null = $proz.Handle  # sonst liefert ExitCode nach dem Ende nichts
        $t0 = Get-Date
        while (-not $proz.HasExited) {
            Start-Sleep -Seconds 2
            $n = @(Get-ChildItem -LiteralPath $exportOrdner -Directory -ErrorAction SilentlyContinue).Count
            Write-Host ("`r    {0} Clips exportiert, {1:mm\:ss} vergangen   " -f $n, ((Get-Date) - $t0)) -NoNewline
        }
        Write-Host ''
        $proz.WaitForExit()
        Log "AT.exe --hd-export: Exit-Code $($proz.ExitCode)"
        if (-not (Test-Path -LiteralPath $marke)) { Fehler "Der Export ist nicht fertig geworden (AT.exe Exit-Code $($proz.ExitCode))." }
        $werte = @{}
        foreach ($z in (Get-Content -LiteralPath $marke)) { $t = $z -split ';'; if ($t.Count -ge 2) { $werte[$t[0]] = $t[1] } }
        Info "Export fertig: $($werte['clips']) Clips mit $($werte['bilder']) Bildern."
        if ([int]$werte['fehlen'] -gt 0) { Log "$($werte['fehlen']) Clips aus der Liste gibt es in dieser Spielversion nicht." }
    }

    # ------------------------------------------------ 6. HD-Grafiken rechnen
    Schritt 'HD-Grafiken rechnen'
    $genArgs = @($Generator, $Spiel, '--modelle', $ModellOrdner, '--nicht-fragen', '--log', $Log)
    if ($Nur) { $genArgs += @('--nur', ($auswahl -join ',')) } elseif ($mitAnimationen) { $genArgs += '--alles' }
    if ($mitGpu) {
        $genArgs += '--gpu'
    } else {
        $genArgs += @('--threads', [string]([Math]::Max(4, [Environment]::ProcessorCount)))
    }
    Info 'Oben steht, wie viele Bilder noch offen sind; die Fortschrittszeile zeigt die geschaetzte Restzeit dieses Schritts.'
    Log ("> " + $venvPython + ' ' + ($genArgs -join ' '))
    $env:PYTHONIOENCODING = 'utf-8'
    & $venvPython @genArgs
    $rc = $LASTEXITCODE
    Log "hd_generate.py: Exit-Code $rc"
    if ($rc -eq 130) { throw [System.OperationCanceledException]::new('Abbruch') }
    if ($rc -ne 0) { Fehler "Der Generator meldet Fehler (Exit-Code $rc). Ein neuer Start rechnet nur, was noch fehlt." }

    # ------------------------------------------------ 7. Spiel einstellen
    Schritt 'Spiel auf HD einstellen (AT.json)'
    SpielBeendenLassen 'Das Einstellen'
    $json = Join-Path $Spiel 'AT.json'
    $text = $null
    if (Test-Path -LiteralPath $json) { $text = [IO.File]::ReadAllText($json) }
    if ($text -and $text -match '"OptionRenderScale"\s*:\s*-?\d+') {
        $text = [regex]::Replace($text, '"OptionRenderScale"\s*:\s*-?\d+', '"OptionRenderScale": 4')
    } elseif ($text -and $text -match '^\s*\{\s*"') {
        $text = ([regex]'^\s*\{').Replace($text, "{`n   `"OptionRenderScale`": 4,", 1)
    } else {
        $text = "{`n   `"OptionRenderScale`": 4`n}`n"
    }
    [IO.File]::WriteAllText($json, $text, $Utf8)
    Info 'AT.json: "OptionRenderScale": 4 gesetzt. Das Spiel startet jetzt in HD.'

    # ------------------------------------------------ 8. Aufraeumen
    Schritt 'Aufraeumen'
    $hdGroesse = OrdnerGroesse (Join-Path $Spiel 'hd')
    Info "HD-Grafiken: $(Join-Path $Spiel 'hd') ($(Groesse $hdGroesse))"
    $arbeitGroesse = OrdnerGroesse $Arbeit
    if ($Behalten) {
        Info 'Zwischendateien und Python-Umgebung bleiben (-Behalten).'
    } elseif ($arbeitGroesse -gt 0) {
        Info "Zwischendateien: $Arbeit ($(Groesse $arbeitGroesse)). Sie werden fuer das Spiel nicht gebraucht."
        if (Frage 'Zwischendateien loeschen?') { Remove-Item -LiteralPath $Arbeit -Recurse -Force; Info 'Geloescht.' }
    }
    $venvGroesse = OrdnerGroesse $Venv
    if (-not $Behalten -and $venvGroesse -gt 0) {
        Info "Python-Umgebung: $Venv ($(Groesse $venvGroesse)). Nur fuer einen neuen Lauf noetig, wird dann neu eingerichtet."
        if (Frage 'Python-Umgebung loeschen?') { Remove-Item -LiteralPath $Venv -Recurse -Force; Info 'Geloescht.' }
    }
    Write-Host ''
    Info ("Fertig nach {0:hh\:mm\:ss}. Viel Spass mit Airline Tycoon Deluxe in HD!" -f ((Get-Date) - $Start)) 'Green'
    $fertig = $true
} catch [System.OperationCanceledException] {
    # Meldung steht schon da
} catch {
    Info "FEHLER: $($_.Exception.Message)" 'Red'
    Log ($_ | Out-String)
    Write-Host "Details stehen in $Log. Bei Rueckfragen bitte diese Datei mitschicken." -ForegroundColor Red
} finally {
    if (-not $fertig) {
        Log 'Nicht fertig geworden (Fehler oder Abbruch). Ein neuer Start setzt fort.'
        Write-Host 'Ein neuer Start von HD_erzeugen.bat setzt an der Stelle fort.' -ForegroundColor Yellow
    }
    Warten
}
if ($fertig) { exit 0 } else { exit 1 }
