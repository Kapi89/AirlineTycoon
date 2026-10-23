#!/usr/bin/env bash
# HD_generate.sh - H30: generates all HD graphics from your own copy of the game on Linux and macOS.
# Counterpart of HD_generate.bat / HD_generate.ps1 (Windows), same steps and the same log file.
#
# Lives next to the game program AT (in the release; in the repository in tools/). Steps: check the game folder,
# find Python (or offer to install it), own Python environment in hd_tools/venv, download the models (only after
# accepting their licenses), export the room animations with ./AT --hd-export, compute the HD graphics with
# hd_generate.py, set up AT.json, clean up temporary files. Every step detects what is already done: stop with
# Ctrl+C, the next start resumes. Log: hd_generate.log in the game folder.
#
# Usage: ./HD_generate.sh [--yes] [--only LIST] [--cpu] [--keep] [--game FOLDER]
#   --yes          answer every question with yes (for automated tests)
#   --only LIST    only these categories (e.g. schriften,stadtfotos), otherwise everything incl. animations
#   --cpu          do not use the graphics card
#   --keep         keep everything at the end, do not ask
#   --game FOLDER  game folder with AT (default: the folder of this script)
#
# Written for bash 3.2 (macOS) and later. No game data, models or HD graphics belong into Git.

set -u

MIN_VERSION="2.0.3"
CATEGORIES="schriften stadtfotos ui raeume bausteine figuren animationen"
PY_PREFERRED="3.12 3.13 3.11 3.14 3.10"  # order = preference; torch exists for these versions
EXPORT_STARTS=4                          # ./AT --hd-export: further starts after an abort skip the clip it stopped at
CUDA_VARIANTS="13.0:cu130 12.8:cu128 12.6:cu126 11.8:cu118"
# Models: checksums from OpenModelDB or the authors' release. A copy with the same SHA-256 is byte for byte the
# official file. The author distributes 4x-UltraSharp via Mega (not downloadable by a script), hence the Hugging Face
# copies; the checksum guarantees it is exactly the author's file.
M1_FILE="4x-UltraSharp.pth"
M1_NAME="4x-UltraSharp (Kim2091)"
M1_LICENSE="CC BY-NC-SA 4.0: non-commercial use only, share only under the same license"
M1_INFO="https://openmodeldb.info/models/4x-UltraSharp"
M1_USE="rooms, user interface, hall bricks, fonts"
M1_SHA="a5812231fc936b42af08a5edba784195495d303d5b3248c24489ef0c4021fe01"
M1_URLS="https://huggingface.co/Kim2091/UltraSharp/resolve/main/4x-UltraSharp.pth https://huggingface.co/lokCX/4x-Ultrasharp/resolve/main/4x-UltraSharp.pth https://huggingface.co/uwg/upscaler/resolve/main/ESRGAN/4x-UltraSharp.pth"
M2_FILE="RealESRGAN_x4plus.pth"
M2_NAME="RealESRGAN_x4plus (xinntao/Real-ESRGAN, release v0.1.0)"
M2_LICENSE="BSD-3-Clause"
M2_INFO="https://github.com/xinntao/Real-ESRGAN/releases/tag/v0.1.0"
M2_USE="city photos, figures, animations"
M2_SHA="4fa0d38905f75ac06eb49a7951b426670021be3018265fd191d2125df9d682f1"
M2_URLS="https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth"

# ---------------------------------------------------------------- Parameters and folders

YES=0; ONLY=""; CPU=0; KEEP=0; GAME=""
while [ $# -gt 0 ]; do
    case "$1" in
        -y|--yes) YES=1 ;;
        --only) shift; ONLY="${1:-}" ;;
        --cpu) CPU=1 ;;
        --keep) KEEP=1 ;;
        --game) shift; GAME="${1:-}" ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "Unknown parameter: $1 (see --help)"; exit 2 ;;
    esac
    shift
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -z "$GAME" ]; then
    if [ "$(basename "$SCRIPT_DIR")" = "hd_tools" ]; then GAME="$(dirname "$SCRIPT_DIR")"; else GAME="$SCRIPT_DIR"; fi
fi
GAME="$(cd "$GAME" 2>/dev/null && pwd)" || { echo "Game folder not found."; exit 1; }
if [ -f "$GAME/hd_tools/hd_generate.py" ]; then TOOLS="$GAME/hd_tools"; else TOOLS="$SCRIPT_DIR"; fi
LOG="$GAME/hd_generate.log"
VENV="$GAME/hd_tools/venv"
VPY="$VENV/bin/python"
MODELS="$GAME/models"
WORK="$GAME/hd_assets"
AT="$GAME/AT"
GENERATOR="$TOOLS/hd_generate.py"
REQUIREMENTS="$TOOLS/hdgen/requirements.txt"
OS="$(uname -s)"
ARCH="$(uname -m)"
MISSING=""  # what the result does not contain, for the summary (one entry per line)

# ---------------------------------------------------------------- Output and log

if [ -t 1 ]; then C_CYAN=$'\033[36m'; C_YEL=$'\033[33m'; C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_OFF=$'\033[0m'
else C_CYAN=""; C_YEL=""; C_RED=""; C_GRN=""; C_OFF=""; fi
log() { printf '%s %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$*" >> "$LOG" 2>/dev/null; }
info() { printf '%s\n' "$*"; log "$*"; }
note() { printf '%s%s%s\n' "$C_YEL" "$*" "$C_OFF"; log "$*"; }
good() { printf '%s%s%s\n' "$C_GRN" "$*" "$C_OFF"; log "$*"; }
FINISHED=0
fail() {
    printf '\n%sERROR: %s%s\n' "$C_RED" "$*" "$C_OFF"; log "ERROR: $*"
    printf '%sDetails are in %s. Please send this file along with any questions.%s\n' "$C_RED" "$LOG" "$C_OFF"
    finish 1
}
finish() {
    if [ "$FINISHED" -ne 1 ]; then
        log "Not finished (error or stopped). The next start resumes."
        printf '%sStarting HD_generate.sh again continues where it stopped.%s\n' "$C_YEL" "$C_OFF"
    fi
    exit "$1"
}
trap 'echo; note "Stopped (Ctrl+C)."; finish 130' INT TERM

STEP=0; STEPS=8; START=$(date +%s)
step() {
    STEP=$((STEP + 1))
    local e=$(( $(date +%s) - START ))
    printf '\n'
    info "${C_CYAN}=== Step $STEP of $STEPS: $1   (elapsed $(printf '%02d:%02d:%02d' $((e / 3600)) $((e / 60 % 60)) $((e % 60))))${C_OFF}"
}

ask() {
    if [ "$YES" -eq 1 ]; then info "$1 Y (automatic)"; return 0; fi
    local i a
    for i in 1 2 3 4 5 6 7 8 9 10; do  # limited, so that a missing terminal (no input) does not loop forever
        printf '%s (Y/N) ' "$1"
        read -r a || { echo; log "$1 -> (no input)"; return 1; }
        log "$1 -> $a"
        case "$a" in [YyJj]*) return 0 ;; [Nn]*) return 1 ;; esac
        echo "Please type Y (yes) or N (no)."
    done
    return 1
}

size_of() { du -sk "$1" 2>/dev/null | awk '{ printf "%.1f GB", $1 / 1048576 }'; }
sha256() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | awk '{print $1}'; else shasum -a 256 "$1" | awk '{print $1}'; fi; }
cpu_count() { getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4; }

# Version a.b.c >= x.y.z?
version_ge() {
    local IFS=.
    set -- $1 $2
    local a1=${1:-0} a2=${2:-0} a3=${3:-0}
    shift 3 2>/dev/null || set --
    local b1=${1:-0} b2=${2:-0} b3=${3:-0}
    [ "$a1" -gt "$b1" ] && return 0; [ "$a1" -lt "$b1" ] && return 1
    [ "$a2" -gt "$b2" ] && return 0; [ "$a2" -lt "$b2" ] && return 1
    [ "$a3" -ge "$b3" ]
}

# Run a command, output to screen and log; returns its exit code
run_logged() {
    log "> $*"
    "$@" 2>&1 | while IFS= read -r line; do printf '    %s\n' "$line"; log "    $line"; done
    return "${PIPESTATUS[0]}"
}

game_running() { pgrep -f "^$AT( |$)" >/dev/null 2>&1; }
wait_game_closed() {
    while game_running; do
        [ "$YES" -eq 1 ] && fail "The game is still running. $1 only works while the game is closed."
        note "The game is still running. Please close it; $1 only works while the game is closed."
        printf 'Then press Enter '; read -r _ || fail "The game is still running."
    done
}

exit_code_text() {
    case "$1" in
        0) echo "0" ;;
        1) echo "1 (program ended itself, e.g. out of memory in the video decoder)" ;;
        126|127) echo "$1 (AT could not be started)" ;;
        13[0-9]|14[0-9]|15[0-9]) echo "$1 (stopped by signal $(( $1 - 128 )), e.g. 139 = crash)" ;;
        *) echo "$1" ;;
    esac
}

debug_excerpt() {
    if [ -f "$GAME/debug.txt" ]; then
        log "----- last 20 lines of debug.txt -----"
        tail -n 20 "$GAME/debug.txt" | while IFS= read -r line; do log "    $line"; done
        log "----- end of debug.txt -----"
    else
        log "debug.txt not found."
    fi
}

# ---------------------------------------------------------------- Python

py_info() {  # prints "major.minor bits" if usable (incl. venv and ensurepip), else nothing
    [ -x "$1" ] || command -v "$1" >/dev/null 2>&1 || return 1
    "$1" -c 'import sys, struct, venv, ensurepip; print("%d.%d %d" % (sys.version_info[0], sys.version_info[1], struct.calcsize("P") * 8))' 2>/dev/null
}

py_find() {  # prints the path of the preferred usable Python
    local cands="" c v info best="" found=""
    for v in $PY_PREFERRED; do cands="$cands python$v"; done
    cands="$cands python3"
    for v in $PY_PREFERRED; do
        cands="$cands /opt/homebrew/bin/python$v /usr/local/bin/python$v /Library/Frameworks/Python.framework/Versions/$v/bin/python3"
    done
    for v in $PY_PREFERRED; do
        for c in $cands; do
            info="$(py_info "$c")" || continue
            found="$found $c=$info"
            if [ "${info% *}" = "$v" ] && [ "${info#* }" = "64" ]; then best="$(command -v "$c" 2>/dev/null || echo "$c")"; break 2; fi
        done
    done
    [ -n "$found" ] && log "Python found:$found"
    [ -n "$best" ] && echo "$best"
}

py_install() {
    local cmd=""
    if [ "$OS" = "Darwin" ]; then
        if command -v brew >/dev/null 2>&1; then
            cmd="brew install python@3.12"
        else
            fail "Python is not installed. Please install Python 3.12 from https://www.python.org/downloads/macos/ (or Homebrew from https://brew.sh) and start HD_generate.sh again."
        fi
    elif command -v apt-get >/dev/null 2>&1; then cmd="sudo apt-get install -y python3 python3-venv python3-pip"
    elif command -v dnf >/dev/null 2>&1; then cmd="sudo dnf install -y python3 python3-pip"
    elif command -v pacman >/dev/null 2>&1; then cmd="sudo pacman -S --needed --noconfirm python python-pip"
    elif command -v zypper >/dev/null 2>&1; then cmd="sudo zypper install -y python3 python3-pip"
    else
        fail "Python 3.10 to 3.14 with the module venv is not installed. Please install it with your package manager and start HD_generate.sh again."
    fi
    info "Python 3.10 to 3.14 (with venv) is not installed. It can be installed now with:"
    info "  $cmd"
    ask "Install Python now?" || fail "Please install Python yourself ($cmd) and start HD_generate.sh again."
    # shellcheck disable=SC2086
    run_logged $cmd || note "The installation reported an error."
}

# ---------------------------------------------------------------- Main

: >> "$LOG" 2>/dev/null || { echo "Cannot write into the game folder: $GAME"; exit 1; }
echo
good "Generate HD graphics for Airline Tycoon Deluxe  ($(date '+%Y-%m-%d %H:%M'))"
echo "You can stop at any time with Ctrl+C. Starting HD_generate.sh again continues where it stopped."
log "Game folder: $GAME; parameters: only=$ONLY cpu=$CPU yes=$YES keep=$KEEP; $OS $ARCH; bash $BASH_VERSION"

if [ -n "$ONLY" ]; then
    SELECTION="$(echo "$ONLY" | tr ',; ' '   ')"
    for k in $SELECTION; do
        case " $CATEGORIES " in *" $k "*) ;; *) fail "Unknown category: $k. Allowed: $CATEGORIES" ;; esac
    done
else
    SELECTION="$CATEGORIES"
fi
with_animations() { case " $SELECTION " in *" animationen "*) return 0 ;; esac; return 1; }
drop_animations() { SELECTION="$(echo " $SELECTION " | sed 's/ animationen / /' | xargs)"; }

# ------------------------------------------------ 1. Game folder
step "Check the game folder"
info "Game folder: $GAME"
[ -f "$AT" ] || fail "There is no program AT in the game folder. HD_generate.sh belongs together with the new AT (release 2.0.3 or later) into the game folder that also contains the folders gli, room and misc."
AT_VERSION="$(LC_ALL=C grep -ao 'VERSION [0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*' "$AT" 2>/dev/null | head -n 1 | cut -d' ' -f2)"
info "AT: version ${AT_VERSION:-unknown}"
if [ -z "$AT_VERSION" ] || ! version_ge "$AT_VERSION" "$MIN_VERSION"; then
    fail "AT is too old (${AT_VERSION:-unknown}). Please unpack the current release (at least $MIN_VERSION) into the game folder."
fi
[ -x "$AT" ] || chmod +x "$AT" 2>/dev/null
if [ "$OS" = "Darwin" ] && xattr -p com.apple.quarantine "$AT" >/dev/null 2>&1; then
    note "macOS has marked AT as downloaded from the internet and would block it."
    if ask "Remove this block for AT now (xattr -d com.apple.quarantine)?"; then xattr -d com.apple.quarantine "$AT" 2>/dev/null; fi
fi
[ -f "$GENERATOR" ] || fail "The generator is missing: $GENERATOR. Please unpack the complete release archive."
GLI_DIR="$(find "$GAME" -maxdepth 1 -type d -iname gli | head -n 1)"
MISC_DIR="$(find "$GAME" -maxdepth 1 -type d -iname misc | head -n 1)"
VIDEO_DIR="$(find "$GAME" -maxdepth 1 -type d -iname video | head -n 1)"
N_GLI=0; N_MCF=0; N_CLIPS=0
[ -n "$GLI_DIR" ] && N_GLI=$(find "$GLI_DIR" -maxdepth 1 -type f -iname '*.gli' | wc -l | tr -d ' ')
[ -n "$MISC_DIR" ] && N_MCF=$(find "$MISC_DIR" -maxdepth 1 -type f -iname '*.mcf' | wc -l | tr -d ' ')
[ -n "$VIDEO_DIR" ] && N_CLIPS=$(find "$VIDEO_DIR" -maxdepth 1 -type f \( -iname '*.smk' -o -iname '*.flc' \) | wc -l | tr -d ' ')
[ "$N_GLI" -gt 0 ] || fail "No game data found (folder gli with .gli files). AT and HD_generate.sh belong into the folder of your installed copy of the game that contains gli, room and misc (on the Mac inside the .app bundle, see README)."
info "Game data: $N_GLI graphics archives in gli, $N_MCF fonts in misc"
if with_animations && [ "$N_CLIPS" -eq 0 ]; then
    note "No animations found (folder video); the animations are skipped."
    drop_animations
    MISSING="${MISSING}animations (no folder video in the game folder)
"
fi
PROBE="$GAME/hd_generate.probe"
( : > "$PROBE" ) 2>/dev/null && rm -f "$PROBE" || fail "Cannot write into the game folder. Please make it writable for your user (or copy the game to a folder in your home directory)."
FREE_KB=$(df -Pk "$GAME" 2>/dev/null | awk 'NR == 2 { print $4 }')
if [ -n "$FREE_KB" ]; then
    info "Free space: $(awk -v k="$FREE_KB" 'BEGIN { printf "%.1f GB", k / 1048576 }')"
    if [ "$FREE_KB" -lt 15728640 ]; then
        note "About 15 GB are needed (Python environment, temporary files, HD graphics). Part of it is freed again at the end."
        ask "Continue anyway?" || fail "Not enough free disk space."
    fi
fi

# ------------------------------------------------ 2. Python
step "Find Python"
PYTHON=""
if py_info "$VPY" >/dev/null; then
    info "Own Python environment present: $VENV (Python $(py_info "$VPY" | cut -d' ' -f1))"
else
    PYTHON="$(py_find)"
    if [ -z "$PYTHON" ]; then
        py_install
        PYTHON="$(py_find)"
        [ -n "$PYTHON" ] || fail "Python 3.10 to 3.14 with the module venv was not found. Please install it (on Debian/Ubuntu: sudo apt install python3 python3-venv) and start HD_generate.sh again."
    fi
    info "Python: $PYTHON (version $(py_info "$PYTHON" | cut -d' ' -f1))"
fi

# ------------------------------------------------ 3. Python environment
step "Set up the Python environment (hd_tools/venv)"
VARIANTS=""
GPU_NAME=""
if [ "$OS" = "Darwin" ]; then
    if [ "$ARCH" != "arm64" ]; then
        note "This is a Mac with an Intel processor: current PyTorch versions are not available for it. The installation will probably fail; see the README (manual way, Python up to 3.12 and torch 2.2)."
    fi
    VARIANTS="pypi"  # macOS: normal PyPI package, uses the Apple graphics chip (MPS) on Apple silicon
elif [ "$CPU" -eq 0 ] && command -v nvidia-smi >/dev/null 2>&1; then
    DRIVER_CUDA="$(nvidia-smi 2>/dev/null | sed -n 's/.*CUDA Version: *\([0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' | head -n 1)"
    GPU_NAME="$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -n 1)"
    info "NVIDIA graphics card: $GPU_NAME (driver supports CUDA ${DRIVER_CUDA:-unknown})"
    for pair in $CUDA_VARIANTS; do
        [ -n "$DRIVER_CUDA" ] && version_ge "$DRIVER_CUDA" "${pair%%:*}" && VARIANTS="$VARIANTS ${pair#*:}"
    done
    VARIANTS="$VARIANTS cpu pypi"
else
    [ "$CPU" -eq 1 ] && info "The graphics card is not used (--cpu)." || info "No NVIDIA graphics card found (nvidia-smi missing): computing on the processor. This takes many hours."
    VARIANTS="cpu pypi"  # pypi: fallback if download.pytorch.org is not reachable (larger download)
fi
set -- $VARIANTS; FIRST_VARIANT="$1"
REQ_HASH="$(sha256 "$REQUIREMENTS" | cut -c1-16 | tr 'a-f' 'A-F')"
MARKER="$VENV/hd_pakete.txt"
WANT="$FIRST_VARIANT|$REQ_HASH"
TEST='import numpy, PIL, torch, spandrel; mps = getattr(torch.backends, "mps", None); print(torch.__version__, 1 if torch.cuda.is_available() else (2 if mps is not None and mps.is_available() else 0))'
TORCH_INFO=""
if [ -f "$MARKER" ] && [ "$(cat "$MARKER" 2>/dev/null)" = "$WANT" ]; then TORCH_INFO="$("$VPY" -c "$TEST" 2>/dev/null)"; fi
if [ -n "$TORCH_INFO" ]; then
    info "Packages already installed (torch $TORCH_INFO)."
else
    if ! py_info "$VPY" >/dev/null; then
        info "Creating the Python environment: $VENV"
        run_logged "$PYTHON" -m venv "$VENV"
        py_info "$VPY" >/dev/null || fail "The Python environment could not be created (on Debian/Ubuntu: sudo apt install python3-venv)."
    fi
    info "Installing the packages (numpy, pillow, torch, spandrel). With a graphics card this is several GB, please be patient."
    "$VPY" -m pip install --upgrade pip --disable-pip-version-check -q >> "$LOG" 2>&1
    INSTALLED=""
    for v in $VARIANTS; do
        info "torch for $v ..."
        if [ "$v" = "pypi" ]; then
            run_logged "$VPY" -m pip install --disable-pip-version-check --progress-bar off torch torchvision
        else
            run_logged "$VPY" -m pip install --disable-pip-version-check --progress-bar off torch torchvision --index-url "https://download.pytorch.org/whl/$v"
        fi
        rc=$?
        if [ $rc -eq 0 ]; then INSTALLED="$v"; break; fi
        note "torch for $v could not be installed (exit code $rc)."
    done
    [ -n "$INSTALLED" ] || fail "torch could not be installed (internet connection? On a Mac with an Intel processor current versions do not exist)."
    run_logged "$VPY" -m pip install --disable-pip-version-check --progress-bar off -r "$REQUIREMENTS" || fail "The packages from requirements.txt could not be installed."
    TORCH_INFO="$("$VPY" -c "$TEST" 2>/dev/null)"
    [ -n "$TORCH_INFO" ] || fail "The installed packages cannot be loaded."
    printf '%s' "$WANT" > "$MARKER"
    info "Packages installed (torch $TORCH_INFO)."
fi
WITH_GPU=0
case "$TORCH_INFO" in
    *" 1") [ "$CPU" -eq 0 ] && WITH_GPU=1 && good "Computing on the graphics card ($GPU_NAME)." ;;
    *" 2") [ "$CPU" -eq 0 ] && WITH_GPU=1 && good "Computing on the Apple graphics chip (MPS)." ;;
    *) [ -n "$GPU_NAME" ] && note "torch does not see the graphics card (driver too old?). Computing on the processor." ;;
esac

# ------------------------------------------------ 4. Models
step "Models"
mkdir -p "$MODELS"
NEEDED=""
for i in 1 2; do
    eval "f=\$M${i}_FILE; s=\$M${i}_SHA"
    if [ -f "$MODELS/$f" ] && [ "$(sha256 "$MODELS/$f")" = "$s" ]; then
        info "$f: present, checksum correct."
    else
        [ -f "$MODELS/$f" ] && note "$f: wrong checksum, downloading again."
        NEEDED="$NEEDED $i"
    fi
done
if [ -n "$NEEDED" ]; then
    echo
    info "Upscaling needs two AI models. They are not made by us and have their own licenses:"
    for i in $NEEDED; do
        eval "n=\$M${i}_NAME; l=\$M${i}_LICENSE; d=\$M${i}_INFO; u=\$M${i}_USE"
        info "  $n"; info "    License: $l"; info "    Details: $d"; info "    Used for: $u"
    done
    info "The HD graphics are for private use only and must not be passed on."
    ask "Have you read and do you accept the licenses? Download the models now?" || fail "The models are required. Alternatively download the files yourself into $MODELS and start again."
    for i in $NEEDED; do
        eval "f=\$M${i}_FILE; s=\$M${i}_SHA; d=\$M${i}_INFO; urls=\$M${i}_URLS"
        ok=0
        for u in $urls; do
            info "Downloading $f from $u"
            if command -v curl >/dev/null 2>&1; then curl -fL --retry 2 -# -o "$MODELS/$f.download" "$u"; else wget -q --show-progress -O "$MODELS/$f.download" "$u"; fi
            if [ $? -ne 0 ]; then note "  not reachable, trying the next source."; log "  $u failed"; continue; fi
            h="$(sha256 "$MODELS/$f.download")"
            if [ "$h" != "$s" ]; then note "  wrong checksum ($h), file discarded."; rm -f "$MODELS/$f.download"; continue; fi
            mv -f "$MODELS/$f.download" "$MODELS/$f"
            info "  checksum correct (SHA-256 $h)."
            ok=1; break
        done
        [ $ok -eq 1 ] || fail "$f could not be downloaded. Please download it yourself from $d into $MODELS (SHA-256 $s) and start again."
    done
fi

# ------------------------------------------------ 5. Export the animations
step "Export the animations from the game"
EXPORT_DIR="$WORK/original/video"
DONE_MARK="$EXPORT_DIR/export_fertig.txt"
RUNNING_MARK="$EXPORT_DIR/export_laeuft.txt"
if ! with_animations; then
    info "Not selected, skipped."
elif [ -f "$DONE_MARK" ] && grep -q '^auswahl;alle' "$DONE_MARK"; then
    info "Already exported."
else
    LC_ALL=C grep -aq -- '--hd-export' "$AT" || fail "This AT does not know the start parameter --hd-export yet. Please unpack the current release."
    wait_game_closed "The export"
    attempt=1
    while [ $attempt -le $EXPORT_STARTS ]; do
        info "Starting ./AT --hd-export alle (no window; takes a few minutes) ..."
        ( cd "$GAME" && exec "$AT" --hd-export alle ) > /dev/null 2>&1 &
        pid=$!
        t0=$(date +%s)
        while kill -0 "$pid" 2>/dev/null; do
            sleep 2
            n=$(find "$EXPORT_DIR" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | wc -l | tr -d ' ')
            [ -t 1 ] && printf '\r    %s clips exported, %ss elapsed   ' "$n" "$(( $(date +%s) - t0 ))"
        done
        wait "$pid"; code=$?
        [ -t 1 ] && echo
        log "AT --hd-export: exit code $(exit_code_text $code)"
        [ -f "$DONE_MARK" ] && break
        note "AT stopped during the export (exit code $(exit_code_text $code))."
        debug_excerpt
        clip=""
        [ -f "$RUNNING_MARK" ] && clip="$(head -n 1 "$RUNNING_MARK" | tr -d '\r')"
        [ -n "$clip" ] || break  # no clip to blame: another start would end the same way
        [ $attempt -lt $EXPORT_STARTS ] && note "It stopped at clip '$clip'. Starting again, this clip is skipped."
        attempt=$((attempt + 1))
    done
    if [ -f "$DONE_MARK" ]; then
        info "Export done: $(sed -n 's/^clips;//p' "$DONE_MARK") clips."
        absent="$(sed -n 's/^fehlen;//p' "$DONE_MARK")"
        [ "${absent:-0}" -gt 0 ] && log "$absent clips from the list do not exist in this version of the game."
        broken="$(sed -n 's/^fehler_clip;//p' "$DONE_MARK" | tr '\n' ' ' | sed 's/ *$//; s/ /, /g')"
        if [ -n "$broken" ]; then
            nb=$(sed -n 's/^fehler_clip;//p' "$DONE_MARK" | wc -l | tr -d ' ')
            note "$nb clip(s) could not be exported and stay in normal resolution: $broken"
            MISSING="${MISSING}$nb animation clip(s), export failed: $broken
"
            [ -f "$EXPORT_DIR/export_fehler.txt" ] && while IFS= read -r line; do log "    export error: $line"; done < "$EXPORT_DIR/export_fehler.txt"
        fi
    else
        note "The animations could not be exported. Continuing with everything else; the animations stay in normal resolution."
        MISSING="${MISSING}animations (export with AT failed, see $LOG)
"
        drop_animations
    fi
fi

# ------------------------------------------------ 6. Compute the HD graphics
step "Compute the HD graphics"
set -- "$GENERATOR" "$GAME" --modelle "$MODELS" --nicht-fragen --log "$LOG" --english
if [ -n "$ONLY" ] || ! with_animations; then set -- "$@" --nur "$(echo $SELECTION | tr ' ' ',')"; else set -- "$@" --alles; fi
if [ $WITH_GPU -eq 1 ]; then set -- "$@" --gpu; else
    threads=$(cpu_count); [ "$threads" -lt 4 ] && threads=4
    set -- "$@" --threads "$threads"
fi
info "Below you see how many images are still to do; the progress line shows the estimated remaining time of this step."
log "> $VPY $*"
PYTHONIOENCODING=utf-8 "$VPY" "$@"
rc=$?
log "hd_generate.py: exit code $rc"
[ $rc -eq 130 ] && { note "Stopped."; finish 130; }
[ $rc -eq 0 ] || fail "The generator reports errors (exit code $rc). The next start only computes what is still missing."

# ------------------------------------------------ 7. Game settings
step "Switch the game to HD (AT.json)"
wait_game_closed "Changing the settings"
"$VPY" - "$GAME/AT.json" <<'PYEOF' || fail "AT.json could not be changed."
import os, re, sys
path = sys.argv[1]
text = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
if re.search(r'"OptionRenderScale"\s*:\s*-?\d+', text):
    text = re.sub(r'"OptionRenderScale"\s*:\s*-?\d+', '"OptionRenderScale": 4', text)
elif re.match(r'\s*\{\s*"', text):
    text = re.sub(r'^\s*\{', '{\n   "OptionRenderScale": 4,', text, count=1)
else:
    text = '{\n   "OptionRenderScale": 4\n}\n'
open(path, "w", encoding="utf-8").write(text)
PYEOF
info 'AT.json: "OptionRenderScale": 4 set. The game now starts in HD.'

# ------------------------------------------------ 8. Clean up
step "Clean up"
info "HD graphics: $GAME/hd ($(size_of "$GAME/hd"))"
if [ $KEEP -eq 1 ]; then
    info "Temporary files and Python environment are kept (--keep)."
else
    if [ -d "$WORK" ]; then
        info "Temporary files: $WORK ($(size_of "$WORK")). The game does not need them."
        if ask "Delete the temporary files?"; then rm -rf "$WORK"; info "Deleted."; fi
    fi
    if [ -d "$VENV" ]; then
        info "Python environment: $VENV ($(size_of "$VENV")). Only needed for another run, it is set up again then."
        if ask "Delete the Python environment?"; then rm -rf "$VENV"; info "Deleted."; fi
    fi
fi
echo
if [ -n "$MISSING" ]; then
    note "Not included (these parts stay in normal resolution):"
    printf '%s' "$MISSING" | while IFS= read -r line; do note "  - $line"; done
    note "Details are in $LOG; please send this file along with any questions."
fi
e=$(( $(date +%s) - START ))
good "Done after $(printf '%02d:%02d:%02d' $((e / 3600)) $((e / 60 % 60)) $((e % 60))). Enjoy Airline Tycoon Deluxe in HD!"
FINISHED=1
exit 0
