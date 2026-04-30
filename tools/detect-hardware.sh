#!/bin/bash
# SPDX-License-Identifier: 0BSD
set -euo pipefail

buf=""

out() {
  local id="$1" driver="${2:-<empty>}" name="${3:-<empty>}"
  buf+="$(printf "%s\t%s\t%s\n" "${id}" "${driver}" "${name}")"
  buf+=$'\n'
}

sysinfo() {
  local vendor product bios_ver bios_date cpu
  vendor="$(cat /sys/class/dmi/id/sys_vendor 2>/dev/null || true)"
  product="$(cat /sys/class/dmi/id/product_name 2>/dev/null || true)"
  bios_ver="$(cat /sys/devices/virtual/dmi/id/bios_version 2>/dev/null || true)"
  bios_date="$(cat /sys/devices/virtual/dmi/id/bios_date 2>/dev/null || true)"
  cpu="$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2 | xargs || true)"
  out "dmi:vendor" "" "${vendor}"
  out "dmi:product" "" "${product}"
  out "dmi:bios" "" "${bios_ver} ${bios_date}"
  out "cpu:0" "" "${cpu}"
}

pci_devices() {
  local d class slot v p driver name
  for d in /sys/bus/pci/devices/*/; do
    class="$(cat "${d}/class" 2>/dev/null || true)"
    case "${class}" in
      0x060000|0x060100|0x060400|0x0c0500|0x0c8000|0x050000|0x078000|0x0c0300) continue ;;
    esac
    slot="$(basename "${d}")"
    v="$(cat "${d}/vendor" 2>/dev/null || true)"
    v="${v#0x}"
    p="$(cat "${d}/device" 2>/dev/null || true)"
    p="${p#0x}"
    driver="$(basename "$(readlink -f "${d}/driver")" 2>/dev/null || true)"
    name="$(lspci -s "${slot}" 2>/dev/null | cut -d: -f3- | xargs || true)"
    out "pci:${v}:${p}" "${driver}" "${name}"
  done
}

usb_devices() {
  local d vid bcd pid driver name
  for d in /sys/bus/usb/devices/[0-9]*; do
    [[ -f "${d}/idVendor" ]] || continue
    vid="$(cat "${d}/idVendor" 2>/dev/null || true)"
    bcd="$(cat "${d}/bDeviceClass" 2>/dev/null || true)"
    [[ "${vid}" == "1d6b" || "${bcd}" == "09" ]] && continue
    pid="$(cat "${d}/idProduct" 2>/dev/null || true)"
    driver="$(basename "$(readlink -f "${d}/driver")" 2>/dev/null || true)"
    name="$(cat "${d}/product" 2>/dev/null || true)"
    out "usb:${vid}:${pid}" "${driver}" "${name}"
  done
}

i2c_devices() {
  local d hid driver name inp
  for d in /sys/bus/i2c/devices/*/; do
    [[ "$(basename "${d}")" == i2c-[0-9]* ]] && continue
    hid="$(cat "${d}/firmware_node/hid" 2>/dev/null || true)"
    driver="$(basename "$(readlink -f "${d}/driver")" 2>/dev/null || true)"
    name=""
    for inp in "${d}"/*/input/input*/name "${d}"/input/input*/name; do
      [[ -f "${inp}" ]] && { name="$(cat "${inp}" || true)"; break; }
    done
    out "i2c:${hid}" "${driver}" "${name}"
  done
}

acpi_devices() {
  local d hid st driver
  for d in /sys/bus/acpi/devices/*/; do
    hid="$(cat "${d}/hid" 2>/dev/null || true)"
    [[ -z "${hid}" ]] && continue
    st="$(cat "${d}/status" 2>/dev/null || true)"
    [[ "${st}" == "0" ]] && continue
    [[ -z "${st}" && ! -L "${d}/driver" ]] && continue
    [[ "${hid}" == LNX* || "${hid}" == PNP0C0F ]] && continue
    driver=""
    if [[ -L "${d}/driver" ]]; then
      driver="$(basename "$(readlink -f "${d}/driver")" || true)"
    fi
    out "acpi:${hid}" "${driver}" "$(cat "${d}/description" 2>/dev/null || true)"
  done
}

block_devices() {
  local d model raw sz driver
  for d in /sys/block/*/; do
    model="$(cat "${d}/device/model" 2>/dev/null | xargs || true)"
    [[ -z "${model}" ]] && continue
    raw="$(cat "${d}/size" 2>/dev/null || true)"
    sz=$(( ${raw:-0} / 2 / 1024 / 1024 ))
    (( sz > 0 )) || continue
    driver="$(basename "$(readlink -f "${d}/device/driver")" 2>/dev/null || true)"
    out "blk:$(basename "${d}")" "${driver}" "${model} (${sz} GB)"
  done
}

display_devices() {
  local d
  for d in /sys/class/drm/card*-DSI-*/; do
    out "drm:$(basename "${d}")" "" \
      "Internal panel $(head -1 "${d}/modes" 2>/dev/null || true)"
  done
  for d in /sys/class/backlight/*/; do
    out "backlight:$(basename "${d}")" "" \
      "$(cat "${d}/type" 2>/dev/null || true)"
  done
}

hwmon_devices() {
  local d
  for d in /sys/class/hwmon/hwmon*/; do
    out "hwmon:$(basename "${d}")" "" \
      "$(cat "${d}/name" 2>/dev/null || true)"
  done
}

tpm_devices() {
  local d ver
  for d in /sys/class/tpm/*/; do
    ver="$(cat "${d}/tpm_version_major" 2>/dev/null || true)"
    if [[ -n "${ver}" ]]; then
      out "tpm:$(basename "${d}")" "" "v${ver}"
    fi
  done
}

main() {
  out "ID" "DRIVER" "NAME"
  sysinfo
  pci_devices
  usb_devices
  i2c_devices
  acpi_devices
  block_devices
  display_devices
  hwmon_devices
  tpm_devices
  echo "${buf}" | column -t -s $'\t'
}

main "$@"
