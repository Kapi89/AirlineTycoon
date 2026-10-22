<#
HD_generate.ps1 - H29: generates all HD graphics from your own copy of the game, no prior knowledge needed.

Called by HD_generate.bat (next to AT.exe); in the release this script lives in hd_tools\.
Steps: check the game folder, find Python (or install it with winget), own Python environment in hd_tools\venv,
download the models (only after accepting their licenses), export the room animations with AT.exe --hd-export,
compute the HD graphics with hd_generate.py, set up AT.json, clean up temporary files.
Every step detects what is already done: stop with Ctrl+C, the next start resumes.
Log: hd_generate.log in the game folder.

Parameters (normally not needed):
  -Spiel FOLDER   game folder with AT.exe (default: folder of the .bat, or the folder above hd_tools)
  -Nur LIST       only these categories (e.g. schriften,stadtfotos), otherwise everything incl. animations
  -Cpu            do not use the graphics card
  -Ja             answer every question with yes (for automated tests)
  -Behalten       keep everything at the end, do not ask
  -NichtWarten    do not wait for a key press at the end

No game data, models or HD graphics belong into Git: everything is created in the game folder only.
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
$PythonPasst = @('3.12', '3.13', '3.11', '3.10')  # order = preference; torch exists for these versions
$Kategorien = @('schriften', 'stadtfotos', 'ui', 'raeume', 'bausteine', 'figuren', 'animationen')
$ExportStarts = 4  # AT.exe --hd-export: further starts after an abort, each skips the clip it stopped at
# torch builds for NVIDIA: highest CUDA version the driver supports (nvidia-smi: "CUDA Version: x.y") first
$CudaPakete = @(
    @{ Version = [version]'13.0'; Name = 'cu130' },
    @{ Version = [version]'12.8'; Name = 'cu128' },
    @{ Version = [version]'12.6'; Name = 'cu126' },
    @{ Version = [version]'11.8'; Name = 'cu118' }
)
# Models: checksums from OpenModelDB or the authors' release. A copy with the same SHA-256 is byte for byte the
# official file. The author distributes 4x-UltraSharp via Mega (not downloadable by a script), hence the Hugging Face
# copies; the checksum guarantees it is exactly the author's file.
$Modelle = @(
    @{
        Kuerzel = 'ultrasharp'; Datei = '4x-UltraSharp.pth'; Name = '4x-UltraSharp (Kim2091)'
        Lizenz = 'CC BY-NC-SA 4.0: non-commercial use only, share only under the same license'
        Info = 'https://openmodeldb.info/models/4x-UltraSharp'; Wofuer = 'rooms, user interface, hall bricks, fonts'
        Groesse = 66961958; Sha256 = 'a5812231fc936b42af08a5edba784195495d303d5b3248c24489ef0c4021fe01'
        Urls = @('https://huggingface.co/Kim2091/UltraSharp/resolve/main/4x-UltraSharp.pth',
                 'https://huggingface.co/lokCX/4x-Ultrasharp/resolve/main/4x-UltraSharp.pth',
                 'https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x-UltraSharp.pth')
    },
    @{
        Kuerzel = 'x4plus'; Datei = 'RealESRGAN_x4plus.pth'; Name = 'RealESRGAN_x4plus (xinntao/Real-ESRGAN, release v0.1.0)'
        Lizenz = 'BSD-3-Clause'
        Info = 'https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.1.0'; Wofuer = 'city photos, figures, animations'
        Groesse = 67040989; Sha256 = '4fa0d38905f75ac06eb49a7951b426670021be3018265fd191d2125df9d682f1'
        Urls = @('https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth')
    }
)

# ---------------------------------------------------------------- Folders

$Werkzeuge = $PSScriptRoot
if (-not $Spiel) {
    if ((Split-Path -Leaf $Werkzeuge) -ieq 'hd_tools') { $Spiel = Split-Path -Parent $Werkzeuge } else { $Spiel = (Get-Location).Path }
}
$Spiel = [IO.Path]::GetFullPath($Spiel.TrimEnd('\', '/', '.') + [IO.Path]::DirectorySeparatorChar)
$Spiel = $Spiel.TrimEnd([IO.Path]::DirectorySeparatorChar)
$Log = Join-Path $Spiel 'hd_generate.log'
$Venv = Join-Path (Join-Path $Spiel 'hd_tools') 'venv'
$ModellOrdner = Join-Path $Spiel 'models'
$Arbeit = Join-Path $Spiel 'hd_assets'
$AtExe = Join-Path $Spiel 'AT.exe'
$DebugTxt = Join-Path $Spiel 'debug.txt'
$Generator = Join-Path $Werkzeuge 'hd_generate.py'
$Anforderungen = Join-Path (Join-Path $Werkzeuge 'hdgen') 'requirements.txt'
$AufWindows = [Environment]::OSVersion.Platform -eq 'Win32NT'
$Fehlt = New-Object System.Collections.Generic.List[string]  # what the result does not contain, for the summary

# ---------------------------------------------------------------- Output and log

$Utf8 = New-Object System.Text.UTF8Encoding($false)
function Log([string]$Text) {
    try { [IO.File]::AppendAllText($Log, (Get-Date -Format 'yyyy-MM-dd HH:mm:ss ') + $Text + "`r`n", $Utf8) } catch { }
}
function Info([string]$Text, [string]$Farbe = 'Gray') { Write-Host $Text -ForegroundColor $Farbe; Log $Text }
function Hinweis([string]$Text) { Info $Text 'Yellow' }
function Fehler([string]$Text) {
    Write-Host ''
    Info "ERROR: $Text" 'Red'
    Write-Host "Details are in $Log. Please send this file along with any questions." -ForegroundColor Red
    throw [System.OperationCanceledException]::new('Fehler')
}

$SchrittNr = 0
$SchritteGesamt = 8
$Start = Get-Date
function Schritt([string]$Titel) {
    $script:SchrittNr++
    $vergangen = (Get-Date) - $Start
    Write-Host ''
    Info ("=== Step {0} of {1}: {2}   (elapsed {3:hh\:mm\:ss})" -f $SchrittNr, $SchritteGesamt, $Titel, $vergangen) 'Cyan'
}

function Frage([string]$Text) {
    if ($Ja) { Info "$Text Y (automatic)"; return $true }
    for ($i = 0; $i -lt 10; $i++) {  # limited, so that a missing console (no input) does not loop forever
        $a = Read-Host "$Text (Y/N)"
        Log "$Text -> $a"
        if ($a -match '^\s*[YyJj]') { return $true }
        if ($a -match '^\s*[Nn]') { return $false }
        Write-Host 'Please type Y (yes) or N (no).'
    }
    return $false
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

# Run an external program, output to screen and log; returns the exit code
function Ausfuehren([string]$Programm, [string[]]$Argumente, [switch]$Leise) {
    Log ("> " + $Programm + ' ' + ($Argumente -join ' '))
    $alt = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'  # messages on stderr are normal for pip and no reason to stop
    try {
        & $Programm @Argumente 2>&1 | ForEach-Object {
            $zeile = "$_"
            # spinner and progress bars (winget, pip) are not shown line by line
            if ($zeile -match '^\s*[-\\|/]?\s*$' -or $zeile -match '[0-9.]+ [KMG]B / [0-9.]+ [KMG]B') { return }
            if (-not $Leise) { Write-Host "    $zeile" }
            Log "    $zeile"
        }
        return $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $alt
    }
}

# Short call, returns only the output (not logged)
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
    Read-Host 'Press Enter to close' | Out-Null
}

function SpielLaeuft {
    $p = Get-Process -Name 'AT' -ErrorAction SilentlyContinue | Where-Object {
        try { $_.Path -and ([IO.Path]::GetFullPath($_.Path) -ieq $AtExe) } catch { $true }
    }
    return [bool]$p
}

function SpielBeendenLassen([string]$Wofuer) {
    while (SpielLaeuft) {
        if ($Ja) { Fehler "The game is still running. $Wofuer only works while the game is closed." }
        Hinweis "The game is still running. Please close it; $Wofuer only works while the game is closed."
        Read-Host 'Then press Enter' | Out-Null
    }
}

function ExitCodeText([int]$Code) {
    $hex = '0x{0:X8}' -f $Code
    $art = switch ($hex) {
        '0xC0000005' { 'access violation (crash)' }
        '0xC0000374' { 'heap corruption (crash)' }
        '0xC0000409' { 'fast fail / stack buffer overrun (crash)' }
        '0xC00000FD' { 'stack overflow (crash)' }
        '0x00000001' { 'program ended itself, e.g. out of memory in the video decoder' }
        default { '' }
    }
    if ($art) { return "$Code ($hex, $art)" }
    return "$Code ($hex)"
}

# Last lines of debug.txt into the log (AT.exe writes it next to itself and overwrites it on every start)
function DebugAuszug([int]$Zeilen = 20) {
    if (-not (Test-Path -LiteralPath $DebugTxt)) { Log 'debug.txt not found.'; return }
    Log "----- last $Zeilen lines of debug.txt -----"
    foreach ($z in @(Get-Content -LiteralPath $DebugTxt -Tail $Zeilen -ErrorAction SilentlyContinue)) { Log "    $z" }
    Log '----- end of debug.txt -----'
}

# ---------------------------------------------------------------- Python

function PythonInfo([string]$Exe) {
    # version and bitness; the Windows Store placeholder (WindowsApps) only opens the Store and does not count
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
        if ($i) { $gefunden += $i; Log "Python found: $($i.Exe) ($($i.Version), $($i.Bits) bit)" }
    }
    foreach ($v in $PythonPasst) {
        $p = $gefunden | Where-Object { $_.Version -eq $v -and $_.Bits -eq 64 } | Select-Object -First 1
        if ($p) { return $p }
    }
    if ($gefunden) {
        Hinweis ("The Python found does not fit (" + (($gefunden | ForEach-Object { "$($_.Version) $($_.Bits) bit" }) -join ', ') +
                 "). Needed is Python " + ($PythonPasst -join ', ') + " in 64 bit.")
    }
    return $null
}

function PythonInstallieren {
    $winget = Get-Command 'winget' -ErrorAction SilentlyContinue
    $anleitung = "Please install Python 3.12 (64 bit) from https://www.python.org/downloads/windows/ " +
                 "(tick 'Add python.exe to PATH' in the installer) and start HD_generate.bat again."
    if (-not $winget) {
        Hinweis 'Python is not installed, and winget (App Installer by Microsoft) is missing.'
        Fehler $anleitung
    }
    Info 'Python is not installed. It can be installed now with winget (the Windows package manager):'
    Info "  winget install $PythonWinget (for this user only, no administrator rights needed)"
    if (-not (Frage 'Install Python now?')) { Fehler $anleitung }
    $rc = Ausfuehren $winget.Source @('install', '--id', $PythonWinget, '-e', '--scope', 'user', '--silent',
                                      '--accept-package-agreements', '--accept-source-agreements', '--disable-interactivity')
    Log "winget: exit code $rc"
    $p = PythonFinden
    if (-not $p) { Fehler ("Python was not found (winget exit code $rc). " + $anleitung) }
    return $p
}

# ---------------------------------------------------------------- Download

function Herunterladen([string]$Url, [string]$Ziel, [long]$Erwartet) {
    Add-Type -AssemblyName System.Net.Http
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $client = New-Object System.Net.Http.HttpClient
    $client.Timeout = [TimeSpan]::FromMinutes(30)
    $client.DefaultRequestHeaders.UserAgent.ParseAdd('AirlineTycoon-HD_generate/1.0')
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

# ---------------------------------------------------------------- Main

$fertig = $false
try {
    Write-Host ''
    Info "Generate HD graphics for Airline Tycoon Deluxe  ($(Get-Date -Format 'yyyy-MM-dd HH:mm'))" 'Green'
    Write-Host 'You can stop at any time with Ctrl+C. Double-clicking HD_generate.bat again continues where it stopped.'
    Log "Game folder: $Spiel; parameters: Nur=$Nur Cpu=$Cpu Ja=$Ja Behalten=$Behalten; PowerShell $($PSVersionTable.PSVersion); $([Environment]::OSVersion.VersionString)"

    if ($Nur) {
        $auswahl = @($Nur -split '[,; ]+' | Where-Object { $_ })
        $falsch = @($auswahl | Where-Object { $Kategorien -notcontains $_ })
        if ($falsch) { Fehler ("Unknown category: " + ($falsch -join ', ') + ". Allowed: " + ($Kategorien -join ', ')) }
    } else {
        $auswahl = $Kategorien
    }
    $mitAnimationen = $auswahl -contains 'animationen'

    # ------------------------------------------------ 1. Game folder
    Schritt 'Check the game folder'
    Info "Game folder: $Spiel"
    if (-not (Test-Path -LiteralPath $AtExe)) {
        Fehler "There is no AT.exe in the game folder. HD_generate.bat belongs together with the new AT.exe (release 2.0.3 or later) into the game folder that also contains the folders gli, room and misc."
    }
    $vi = (Get-Item -LiteralPath $AtExe).VersionInfo
    $atVersion = [version]('{0}.{1}.{2}' -f $vi.FileMajorPart, $vi.FileMinorPart, $vi.FileBuildPart)
    Info "AT.exe: version $atVersion"
    if ($atVersion -lt $MindestVersion) {
        Fehler "AT.exe is too old ($atVersion). Please unpack the current release (at least $MindestVersion) into the game folder."
    }
    if (-not (Test-Path -LiteralPath $Generator)) { Fehler "The generator is missing: $Generator. Please unpack the complete release archive." }
    $gli = @(Get-ChildItem -LiteralPath (Join-Path $Spiel 'gli') -Filter '*.gli' -File -ErrorAction SilentlyContinue)
    $mcf = @(Get-ChildItem -LiteralPath (Join-Path $Spiel 'misc') -Filter '*.mcf' -File -ErrorAction SilentlyContinue)
    if ($gli.Count -eq 0) {
        Fehler "No game data found (folder gli with .gli files). The release belongs into the folder of your installed copy of the game (GOG or Steam)."
    }
    Info "Game data: $($gli.Count) graphics archives in gli, $($mcf.Count) fonts in misc"
    $video = Join-Path $Spiel 'video'
    $clips = @(Get-ChildItem -LiteralPath $video -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -in '.smk', '.flc' })
    if ($mitAnimationen -and $clips.Count -eq 0) {
        Hinweis 'No animations found (folder video); the animations are skipped.'
        $mitAnimationen = $false
        $auswahl = @($auswahl | Where-Object { $_ -ne 'animationen' })
        $Fehlt.Add('animations (no folder video in the game folder)')
    }
    $schreibbar = $true
    try {
        $probe = Join-Path $Spiel 'hd_generate.probe'
        [IO.File]::WriteAllText($probe, 'ok')
        Remove-Item -LiteralPath $probe -Force
    } catch { $schreibbar = $false }
    if (-not $schreibbar) {
        Fehler "Cannot write into the game folder (e.g. below C:\Program Files). Please right-click HD_generate.bat and choose 'Run as administrator', or install the game into another folder (e.g. C:\Games)."
    }
    $frei = $null
    try { $frei = (New-Object IO.DriveInfo ([IO.Path]::GetPathRoot($Spiel))).AvailableFreeSpace } catch { Log "Free space unknown: $($_.Exception.Message)" }
    if ($null -ne $frei) {
        Info "Free space: $(Groesse $frei)"
        if ($frei -lt 15GB) {
            Hinweis 'About 15 GB are needed (Python environment, temporary files, HD graphics). Part of it is freed again at the end.'
            if (-not (Frage 'Continue anyway?')) { Fehler 'Not enough free disk space.' }
        }
    }

    # ------------------------------------------------ 2. Python
    Schritt 'Find Python'
    $venvPython = Join-Path (Join-Path $Venv 'Scripts') 'python.exe'
    if (-not $AufWindows) { $venvPython = Join-Path (Join-Path $Venv 'bin') 'python' }
    $venvOk = PythonInfo $venvPython
    if ($venvOk) {
        Info "Own Python environment present: $Venv (Python $($venvOk.Version))"
    } else {
        $python = PythonFinden
        if (-not $python) { $python = PythonInstallieren }
        Info "Python: $($python.Exe) (version $($python.Version))"
    }

    # ------------------------------------------------ 3. Python environment
    Schritt 'Set up the Python environment (hd_tools\venv)'
    $gpuName = $null
    $treiberCuda = $null
    $smi = Get-Command 'nvidia-smi' -ErrorAction SilentlyContinue
    if (-not $Cpu -and $smi) {
        $s = Abfragen $smi.Source @()
        if ($s -and $s -match 'CUDA Version:\s*([0-9]+\.[0-9]+)') { $treiberCuda = [version]$Matches[1] }
        $gpuName = Abfragen $smi.Source @('--query-gpu=name', '--format=csv,noheader')
        if ($gpuName) { $gpuName = ($gpuName -split "`n")[0] }
        Info "NVIDIA graphics card: $gpuName (driver supports CUDA $treiberCuda)"
    } elseif ($Cpu) {
        Info 'The graphics card is not used (-Cpu).'
    } else {
        Info 'No NVIDIA graphics card found (nvidia-smi missing): computing on the processor. This takes many hours.'
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
        Info "Packages already installed (torch $torchInfo)."
    } else {
        if (-not $venvOk) {
            Info "Creating the Python environment: $Venv"
            $rc = Ausfuehren $python.Exe @('-m', 'venv', $Venv)
            if ($rc -ne 0 -or -not (PythonInfo $venvPython)) { Fehler "The Python environment could not be created (exit code $rc)." }
        }
        Info 'Installing the packages (numpy, pillow, torch, spandrel). With a graphics card this is several GB, please be patient.'
        Ausfuehren $venvPython @('-m', 'pip', 'install', '--upgrade', 'pip', '--disable-pip-version-check') -Leise | Out-Null
        $installiert = $null
        foreach ($v in $varianten) {
            Info "torch for $v ..."
            $rc = Ausfuehren $venvPython @('-m', 'pip', 'install', '--disable-pip-version-check', '--progress-bar', 'off', 'torch', 'torchvision',
                                           '--index-url', "https://download.pytorch.org/whl/$v")
            if ($rc -eq 0) { $installiert = $v; break }
            Hinweis "torch for $v could not be installed (exit code $rc)."
        }
        if (-not $installiert) { Fehler 'torch could not be installed (internet connection?).' }
        $rc = Ausfuehren $venvPython @('-m', 'pip', 'install', '--disable-pip-version-check', '--progress-bar', 'off', '-r', $Anforderungen)
        if ($rc -ne 0) { Fehler "The packages from requirements.txt could not be installed (exit code $rc)." }
        $torchInfo = Abfragen $venvPython @('-c', $test)
        if (-not $torchInfo) { Fehler 'The installed packages cannot be loaded.' }
        [IO.File]::WriteAllText($kennung, "$($varianten[0])|$anfHash", $Utf8)
        Info "Packages installed (torch $torchInfo)."
    }
    $mitGpu = (-not $Cpu) -and ($torchInfo -match ' 1$')
    if ($mitGpu) { Info "Computing on the graphics card ($gpuName)." 'Green' }
    elseif ($treiberCuda) { Hinweis 'torch does not see the graphics card (driver too old?). Computing on the processor.' }

    # ------------------------------------------------ 4. Models
    Schritt 'Models'
    New-Item -ItemType Directory -Force -Path $ModellOrdner | Out-Null
    $fehlend = @()
    foreach ($m in $Modelle) {
        $p = Join-Path $ModellOrdner $m.Datei
        if ((Test-Path -LiteralPath $p) -and ((Sha256 $p) -eq $m.Sha256)) {
            Info "$($m.Datei): present, checksum correct."
        } else {
            if (Test-Path -LiteralPath $p) { Hinweis "$($m.Datei): wrong checksum, downloading again." }
            $fehlend += $m
        }
    }
    if ($fehlend) {
        Write-Host ''
        Info 'Upscaling needs two AI models. They are not made by us and have their own licenses:' 'White'
        foreach ($m in $fehlend) {
            Info "  $($m.Name)" 'White'
            Info "    License: $($m.Lizenz)"
            Info "    Details: $($m.Info)"
            Info "    Used for: $($m.Wofuer)"
        }
        Info 'The HD graphics are for private use only and must not be passed on.' 'White'
        if (-not (Frage 'Have you read and do you accept the licenses? Download the models now?')) {
            Fehler "The models are required. Alternatively download the files yourself into $ModellOrdner and start again."
        }
        foreach ($m in $fehlend) {
            $ziel = Join-Path $ModellOrdner $m.Datei
            $tmp = "$ziel.download"
            $ok = $false
            foreach ($u in $m.Urls) {
                Info "Downloading $($m.Datei) from $u"
                if (-not (Herunterladen $u $tmp $m.Groesse)) { Hinweis '  not reachable, trying the next source.'; continue }
                $h = Sha256 $tmp
                if ($h -ne $m.Sha256) { Hinweis "  wrong checksum ($h), file discarded."; Remove-Item -LiteralPath $tmp -Force; continue }
                Move-Item -LiteralPath $tmp -Destination $ziel -Force
                Info "  checksum correct (SHA-256 $h)."
                $ok = $true
                break
            }
            if (-not $ok) {
                Fehler "$($m.Datei) could not be downloaded. Please download it yourself from $($m.Info) into $ModellOrdner (SHA-256 $($m.Sha256)) and start again."
            }
        }
    }

    # ------------------------------------------------ 5. Export the animations
    Schritt 'Export the animations from the game'
    $exportOrdner = Join-Path (Join-Path $Arbeit 'original') 'video'
    $marke = Join-Path $exportOrdner 'export_fertig.txt'
    $laeuft = Join-Path $exportOrdner 'export_laeuft.txt'
    if (-not $mitAnimationen) {
        Info 'Not selected, skipped.'
    } elseif ((Test-Path -LiteralPath $marke) -and ((Get-Content -LiteralPath $marke -Raw) -match 'auswahl;alle')) {
        Info 'Already exported.'
    } else {
        $inhalt = [IO.File]::ReadAllBytes($AtExe)
        if ([Text.Encoding]::ASCII.GetString($inhalt).IndexOf('--hd-export') -lt 0) {
            Fehler 'This AT.exe does not know the start parameter --hd-export yet. Please unpack the current release.'
        }
        $inhalt = $null
        SpielBeendenLassen 'The export'
        for ($versuch = 1; $versuch -le $ExportStarts; $versuch++) {
            Info 'Starting AT.exe --hd-export alle (no window; takes a few minutes) ...'
            $proz = Start-Process -FilePath $AtExe -ArgumentList @('--hd-export', 'alle') -WorkingDirectory $Spiel -PassThru
            $null = $proz.Handle  # otherwise ExitCode stays empty after the process ended
            $t0 = Get-Date
            while (-not $proz.HasExited) {
                Start-Sleep -Seconds 2
                $n = @(Get-ChildItem -LiteralPath $exportOrdner -Directory -ErrorAction SilentlyContinue).Count
                Write-Host ("`r    {0} clips exported, {1:mm\:ss} elapsed   " -f $n, ((Get-Date) - $t0)) -NoNewline
            }
            Write-Host ''
            $proz.WaitForExit()
            $code = $proz.ExitCode
            Log "AT.exe --hd-export: exit code $(ExitCodeText $code)"
            if (Test-Path -LiteralPath $marke) { break }
            Hinweis "AT.exe stopped during the export (exit code $(ExitCodeText $code))."
            DebugAuszug 20
            $clip = $null
            if (Test-Path -LiteralPath $laeuft) { $clip = (Get-Content -LiteralPath $laeuft -TotalCount 1) }
            if (-not $clip) { break }  # no clip to blame: another start would end the same way
            if ($versuch -lt $ExportStarts) { Hinweis "It stopped at clip '$clip'. Starting again, this clip is skipped." }
        }
        if (Test-Path -LiteralPath $marke) {
            $werte = @{}
            $kaputt = @()
            foreach ($z in (Get-Content -LiteralPath $marke)) {
                $t = $z -split ';'
                if ($t.Count -ge 2) { if ($t[0] -eq 'fehler_clip') { $kaputt += $t[1] } else { $werte[$t[0]] = $t[1] } }
            }
            Info "Export done: $($werte['clips']) clips."
            if ([int]$werte['fehlen'] -gt 0) { Log "$($werte['fehlen']) clips from the list do not exist in this version of the game." }
            if ($kaputt) {
                Hinweis ("$($kaputt.Count) clip(s) could not be exported and stay in normal resolution: " + ($kaputt -join ', '))
                $Fehlt.Add("$($kaputt.Count) animation clip(s), export failed: " + ($kaputt -join ', '))
                $fehlerDatei = Join-Path $exportOrdner 'export_fehler.txt'
                if (Test-Path -LiteralPath $fehlerDatei) { foreach ($z in (Get-Content -LiteralPath $fehlerDatei)) { Log "    export error: $z" } }
            }
        } else {
            Hinweis 'The animations could not be exported. Continuing with everything else; the animations stay in normal resolution.'
            $Fehlt.Add("animations (export with AT.exe failed, see $Log)")
            $mitAnimationen = $false
            $auswahl = @($auswahl | Where-Object { $_ -ne 'animationen' })
        }
    }

    # ------------------------------------------------ 6. Compute the HD graphics
    Schritt 'Compute the HD graphics'
    $genArgs = @($Generator, $Spiel, '--modelle', $ModellOrdner, '--nicht-fragen', '--log', $Log, '--english')
    if ($Nur -or -not $mitAnimationen) { $genArgs += @('--nur', ($auswahl -join ',')) } else { $genArgs += '--alles' }
    if ($mitGpu) {
        $genArgs += '--gpu'
    } else {
        $genArgs += @('--threads', [string]([Math]::Max(4, [Environment]::ProcessorCount)))
    }
    Info 'Below you see how many images are still to do; the progress line shows the estimated remaining time of this step.'
    Log ("> " + $venvPython + ' ' + ($genArgs -join ' '))
    $env:PYTHONIOENCODING = 'utf-8'
    & $venvPython @genArgs
    $rc = $LASTEXITCODE
    Log "hd_generate.py: exit code $rc"
    if ($rc -eq 130) { throw [System.OperationCanceledException]::new('Abbruch') }
    if ($rc -ne 0) { Fehler "The generator reports errors (exit code $rc). The next start only computes what is still missing." }

    # ------------------------------------------------ 7. Game settings
    Schritt 'Switch the game to HD (AT.json)'
    SpielBeendenLassen 'Changing the settings'
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
    Info 'AT.json: "OptionRenderScale": 4 set. The game now starts in HD.'

    # ------------------------------------------------ 8. Clean up
    Schritt 'Clean up'
    $hdGroesse = OrdnerGroesse (Join-Path $Spiel 'hd')
    Info "HD graphics: $(Join-Path $Spiel 'hd') ($(Groesse $hdGroesse))"
    $arbeitGroesse = OrdnerGroesse $Arbeit
    if ($Behalten) {
        Info 'Temporary files and Python environment are kept (-Behalten).'
    } elseif ($arbeitGroesse -gt 0) {
        Info "Temporary files: $Arbeit ($(Groesse $arbeitGroesse)). The game does not need them."
        if (Frage 'Delete the temporary files?') { Remove-Item -LiteralPath $Arbeit -Recurse -Force; Info 'Deleted.' }
    }
    $venvGroesse = OrdnerGroesse $Venv
    if (-not $Behalten -and $venvGroesse -gt 0) {
        Info "Python environment: $Venv ($(Groesse $venvGroesse)). Only needed for another run, it is set up again then."
        if (Frage 'Delete the Python environment?') { Remove-Item -LiteralPath $Venv -Recurse -Force; Info 'Deleted.' }
    }
    Write-Host ''
    if ($Fehlt.Count -gt 0) {
        Hinweis 'Not included (these parts stay in normal resolution):'
        foreach ($f in $Fehlt) { Hinweis "  - $f" }
        Hinweis "Details are in $Log; please send this file along with any questions."
    }
    Info ("Done after {0:hh\:mm\:ss}. Enjoy Airline Tycoon Deluxe in HD!" -f ((Get-Date) - $Start)) 'Green'
    $fertig = $true
} catch [System.OperationCanceledException] {
    # message already shown
} catch {
    Info "ERROR: $($_.Exception.Message)" 'Red'
    Log ($_ | Out-String)
    Write-Host "Details are in $Log. Please send this file along with any questions." -ForegroundColor Red
} finally {
    if (-not $fertig) {
        Log 'Not finished (error or stopped). The next start resumes.'
        Write-Host 'Starting HD_generate.bat again continues where it stopped.' -ForegroundColor Yellow
    }
    Warten
}
if ($fertig) { exit 0 } else { exit 1 }
