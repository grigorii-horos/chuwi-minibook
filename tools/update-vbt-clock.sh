#!/bin/bash
# SPDX-License-Identifier: 0BSD
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly SCRIPT_DIR

readonly VBT_TOOL="${SCRIPT_DIR}/../vbt_patch/vbt_patch"
readonly SYS_VBT="/sys/kernel/debug/dri/0000:00:02.0/i915_vbt"
readonly FIRMWARE_VBT="/lib/firmware/vbt"
readonly MKINITCPIO_CONF="/etc/mkinitcpio.conf"
readonly LIMINE_CONF="/etc/default/limine"

revert() {
  if [[ -f "${LIMINE_CONF}" ]]; then
    sed -i 's| i915\.vbt_firmware=vbt||g' "${LIMINE_CONF}"
    echo "Removed i915.vbt_firmware=vbt from ${LIMINE_CONF}"
  fi
  echo "Revert complete — reboot to apply."
}

require_root() {
  if (( EUID != 0 )); then
    echo "This script must be run as root" >&2
    exit 1
  fi
}

check_environment() {
  if [[ ! -f "${SYS_VBT}" ]]; then
    echo "${SYS_VBT} not found — is i915 loaded?" >&2
    exit 1
  fi

  if ! command -v mkinitcpio &>/dev/null \
      && ! command -v limine-mkinitcpio &>/dev/null; then
    echo "Neither mkinitcpio nor limine-mkinitcpio found" >&2
    exit 1
  fi

  if [[ ! -f "${LIMINE_CONF}" ]]; then
    echo "${LIMINE_CONF} not found — only Limine is supported" >&2
    exit 1
  fi
}

check_vbt_tool() {
  if [[ ! -x "${VBT_TOOL}" ]]; then
    echo "vbt_patch not found — run 'make' in vbt_patch/ first" >&2
    exit 1
  fi
}

patch_vbt() {
  local framerate="$1"

  if [[ -f "${FIRMWARE_VBT}" ]]; then
    echo "${FIRMWARE_VBT} already exists — remove it or run: $0 --revert" >&2
    exit 1
  fi

  local input_vbt
  input_vbt="$(mktemp)"
  cp "${SYS_VBT}" "${input_vbt}"

  local output_vbt
  output_vbt="$(mktemp)"

  "${VBT_TOOL}" "${input_vbt}" --hz "${framerate}" "${output_vbt}"
  rm -f "${input_vbt}"

  cp "${output_vbt}" "${FIRMWARE_VBT}"
  chmod 644 "${FIRMWARE_VBT}"
  rm -f "${output_vbt}"
  echo "Installed patched VBT to ${FIRMWARE_VBT}"
}

update_mkinitcpio() {
  if grep -q '/lib/firmware/vbt' "${MKINITCPIO_CONF}"; then
    return
  fi

  if grep -qE '^FILES=\(\)' "${MKINITCPIO_CONF}"; then
    sed -i 's|^FILES=()|FILES=(/lib/firmware/vbt)|' "${MKINITCPIO_CONF}"
  elif grep -qE '^FILES=\(' "${MKINITCPIO_CONF}"; then
    sed -i 's|^FILES=(\(.*\))|FILES=(\1 /lib/firmware/vbt)|' "${MKINITCPIO_CONF}"
  else
    echo 'FILES=(/lib/firmware/vbt)' >> "${MKINITCPIO_CONF}"
  fi
  echo "Updated ${MKINITCPIO_CONF}"
}

update_cmdline() {
  if grep -q 'i915.vbt_firmware=vbt' "${LIMINE_CONF}"; then
    return
  fi

  if ! grep -qE '^KERNEL_CMDLINE\[default\]' "${LIMINE_CONF}"; then
    echo "Could not find KERNEL_CMDLINE[default] in ${LIMINE_CONF}" >&2
    exit 1
  fi

  sed -i '/^KERNEL_CMDLINE\[default\]/s|"$| i915.vbt_firmware=vbt"|' "${LIMINE_CONF}"
  echo "Added i915.vbt_firmware=vbt to kernel cmdline"
}

rebuild_initramfs() {
  if command -v limine-mkinitcpio &>/dev/null; then
    limine-mkinitcpio
  else
    mkinitcpio -P
  fi
  echo "Initramfs rebuilt"
}

main() {
  require_root

  if [[ "${1:-}" == "--revert" ]]; then
    revert
    return
  fi

  if [[ $# -ne 1 ]]; then
    echo "Usage: $0 <framerate>"
    echo "       $0 --revert"
    exit 1
  fi

  check_vbt_tool
  check_environment
  patch_vbt "$1"
  update_mkinitcpio
  update_cmdline
  rebuild_initramfs

  echo "Done — reboot to apply. To revert: $0 --revert"
}

main "$@"
