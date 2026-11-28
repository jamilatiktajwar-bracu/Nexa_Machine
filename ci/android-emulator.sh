#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"
case "$MODE" in
  instrumentation)
    adb wait-for-device
    booted=""
    for attempt in $(seq 1 180); do
      device_state="$(adb get-state 2>/dev/null || true)"
      if [[ "$device_state" == "device" ]]; then
        booted="$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' || true)"
        if [[ "$booted" == "1" ]]; then
          break
        fi
      fi
      sleep 2
    done
    [[ "$booted" == "1" ]] || {
      echo "Android emulator did not become online and finish booting." >&2
      adb devices -l || true
      adb logcat -d -t 500 || true
      exit 1
    }
    adb shell input keyevent KEYCODE_WAKEUP || true
    adb shell wm dismiss-keyguard || true
    gradle -p mobile/android connectedDebugAndroidTest

    # The OS-level force-stop is host-driven after instrumentation exits.
    # Running am force-stop from inside an instrumentation test can kill the
    # instrumentation process because it belongs to the target package.
    diagnostics="mobile/android/app/build/diagnostics"
    mkdir -p "$diagnostics"
    verify_workspace_recovery() {
      local attempt="$1"
      local ui_dump="${RUNNER_TEMP:-/tmp}/nexa-android-ui-$attempt.xml"
      mkdir -p "$(dirname "$ui_dump")"
      for ui_attempt in $(seq 1 20); do
        adb shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
        adb shell wm dismiss-keyguard >/dev/null 2>&1 || true
        adb shell uiautomator dump /sdcard/nexa-window.xml >/dev/null 2>&1 || true
        adb shell cat /sdcard/nexa-window.xml > "$ui_dump" 2>/dev/null || true
        if grep -Fq "Workspace restored" "$ui_dump" &&
           grep -Fq "Recovery: SUCCESS" "$ui_dump"; then
          echo "Android workspace recovery UI verified after host force-stop (launch $attempt)."
          return 0
        fi
        sleep 2
      done
      cp "$ui_dump" "$diagnostics/ui-after-launch-$attempt.xml" 2>/dev/null || true
      adb logcat -d -t 1500 > "$diagnostics/logcat-after-launch-$attempt.txt" 2>&1 || true
      adb shell dumpsys activity activities > "$diagnostics/activities-after-launch-$attempt.txt" 2>&1 || true
      echo "Android workspace recovery UI was not verified after force-stop launch $attempt." >&2
      return 1
    }

    for launch_attempt in 1 2; do
      adb shell am force-stop com.nexa.machine
      adb shell am start -W -n com.nexa.machine/.MainActivity
      verify_workspace_recovery "$launch_attempt"
    done
    echo "Android OS force-stop and repeated workspace recovery checks passed."
    ;;
  release-smoke)
    apk="$(find mobile/android/app/build/outputs/apk/release -type f -name '*.apk' -print -quit 2>/dev/null || true)"
    if [[ -z "$apk" ]]; then
      apk="mobile/android/app/build/outputs/apk/debug/app-debug.apk"
    fi
    [[ -f "$apk" ]] || {
      echo "No Android release APK or debug preview APK was produced." >&2
      exit 1
    }
    adb wait-for-device
    adb install "$apk"
    adb shell am start -W -n com.nexa.machine/.MainActivity
    sleep 8
    first_pid="$(adb shell pidof com.nexa.machine | tr -d '\r' | tr -d '[:space:]')"
    [[ -n "$first_pid" ]] || {
      echo "Android app process did not start after installation." >&2
      adb logcat -d -t 250 || true
      exit 1
    }
    adb shell am force-stop com.nexa.machine
    adb shell am start -W -n com.nexa.machine/.MainActivity
    sleep 8
    second_pid="$(adb shell pidof com.nexa.machine | tr -d '\r' | tr -d '[:space:]')"
    [[ -n "$second_pid" ]] || {
      echo "Android app process did not restart after force-stop." >&2
      adb logcat -d -t 250 || true
      exit 1
    }
    printf 'Android app relaunched successfully (pid %s -> %s)\n' "$first_pid" "$second_pid"
    ;;
  *)
    echo "Usage: $0 {instrumentation|release-smoke}" >&2
    exit 2
    ;;
esac
