#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-}"
case "$MODE" in
  instrumentation)
    adb wait-for-device
    booted=""
    for attempt in $(seq 1 60); do
      booted="$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' || true)"
      if [[ "$booted" == "1" ]]; then
        break
      fi
      sleep 2
    done
    [[ "$booted" == "1" ]] || {
      echo "Android emulator did not finish booting." >&2
      adb devices -l || true
      exit 1
    }
    gradle -p mobile/android connectedDebugAndroidTest
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
