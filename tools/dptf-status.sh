#!/bin/bash
# SPDX-License-Identifier: 0BSD
set -euo pipefail

readonly PROC_THERMAL_PCI="0000:00:04.0"
readonly INT3400_DRIVER="/sys/bus/platform/drivers/int3400 thermal"
readonly ACPI_DEVICES="/sys/bus/acpi/devices"
readonly PLATFORM_DEVICES="/sys/bus/platform/devices"
readonly THERMAL_CLASS="/sys/class/thermal"
readonly RAPL_MSR="/sys/class/powercap/intel-rapl:0"
readonly RAPL_MMIO="/sys/devices/virtual/powercap/intel-rapl-mmio"
readonly RAPL_MMIO_PKG="${RAPL_MMIO}/intel-rapl-mmio:0"
readonly PROC_THERMAL_SYSFS="/sys/bus/pci/devices/${PROC_THERMAL_PCI}"
readonly MSR_PKG_CST_CONFIG=0xE2
readonly MSR_TEMP_TARGET=0x1A2
readonly TJMAX_FUSED=105
readonly STOCK_TCC_OFFSET=20

readonly -a PARTICIPANTS=(
  IETM SEN1 SEN2 SEN3 SEN4 SEN5 DGPU
  TFN1 TFN2 TFN3 CHRG TPWR TPCH BAT1
)

readonly -a MONITORED_ZONES=(
  B0D4 TCPU SEN3 minibook_soc minibook_charger
)

declare -a WARNINGS=()
declare -A ACPI_MAP=()
declare -A ZONE_MAP=()

has_cmd() { command -v "$1" &>/dev/null; }

module_loaded() { [[ -d "/sys/module/$1" ]]; }

print_field() {
  printf "%-28s%s\n" "$1" "$2"
}

print_warn() {
  printf "%-28s\033[33mWARN\033[0m  %s\n" "$1" "$2"
  WARNINGS+=("${1}: ${2}")
}

print_detail() {
  printf "%-28s%s\n" "" "$1"
}

require_root() {
  if (( EUID != 0 )); then
    echo "This script must be run as root" >&2
    exit 1
  fi
}

read_sysfs() {
  local path="$1"
  if [[ -f "${path}" ]]; then
    cat "${path}" 2>/dev/null || true
  fi
}

read_module_param() {
  local mod="$1" param="$2"
  read_sysfs "/sys/module/${mod}/parameters/${param}"
}

sysfs_driver() {
  local sysfs_path="$1"
  if [[ -L "${sysfs_path}/driver" ]]; then
    basename "$(readlink "${sysfs_path}/driver")"
  fi
}

platform_device_driver() {
  local instance="$1"
  local plat_path="${PLATFORM_DEVICES}/${instance}"
  if [[ -d "${plat_path}" ]]; then
    sysfs_driver "${plat_path}"
  fi
}

format_temp_mc() {
  local mc=$1
  echo "$(( mc / 1000 )).$(( (mc % 1000) / 100 )) C"
}

format_uw() {
  local uw=$1
  printf "%d.%02d W" \
    $(( uw / 1000000 )) $(( (uw % 1000000) / 10000 ))
}

check_dptf_enabler() {
  printf "%-28s" "dptf_enabler"
  if ! module_loaded dptf_enabler; then
    echo "NOT LOADED"
    WARNINGS+=("dptf_enabler: not loaded")
    return
  fi

  local status="loaded"
  if [[ -f /etc/modules-load.d/dptf_enabler.conf ]]; then
    status="${status}, boot"
  fi
  echo "${status}"

  local fans
  fans="$(read_module_param dptf_enabler enable_fans)"
  local sensors
  sensors="$(read_module_param dptf_enabler enable_sensors)"
  if [[ -n "${fans}" ]] || [[ -n "${sensors}" ]]; then
    print_detail "enable_fans=${fans:-?}, enable_sensors=${sensors:-?}"
  fi
}

check_module() {
  local mod="$1"
  printf "%-28s" "${mod}"
  if module_loaded "${mod}"; then
    echo "loaded"
  else
    echo "NOT LOADED"
    WARNINGS+=("${mod}: not loaded")
  fi
}

check_modules() {
  echo "--- modules ---"
  echo ""
  check_dptf_enabler
  check_module int3400_thermal
  check_module int340x_thermal_zone
  check_module intel_rapl_common
}

find_int3400_device() {
  [[ -d "${INT3400_DRIVER}" ]] || return 1
  local entry
  for entry in "${INT3400_DRIVER}"/INT* \
               "${INT3400_DRIVER}"/INTC*; do
    if [[ -d "${entry}" ]]; then
      basename "${entry}"
      return 0
    fi
  done
  return 1
}

check_data_vault() {
  local dev_path="$1"
  local vault="${dev_path}/data_vault"
  if [[ ! -f "${vault}" ]]; then
    print_warn "IETM" "data_vault not found"
    return
  fi
  local size
  size="$(stat -c %s "${vault}" 2>/dev/null || echo 0)"
  if (( size > 0 )); then
    print_detail "data_vault: ${size} bytes"
  else
    print_warn "IETM" "data_vault empty"
  fi
}

check_current_policy() {
  local dev_path="$1"
  local current
  current="$(read_sysfs "${dev_path}/uuids/current_uuid")"
  [[ -n "${current}" ]] || return 0
  local policy
  case "${current}" in
    63BE270F*|63be270f*) policy="adaptive_performance" ;;
    42A441D6*|42a441d6*) policy="passive" ;;
    9E04115A*|9e04115a*) policy="passive_2" ;;
    *)                   policy="${current}" ;;
  esac
  print_detail "policy: ${policy}"
}

check_dptf_manager() {
  echo ""
  echo "--- dptf manager ---"
  echo ""

  local dev_name
  if ! dev_name="$(find_int3400_device)"; then
    print_field "IETM" "not found"
    if module_loaded dptf_enabler; then
      print_warn "IETM" "dptf_enabler loaded but IETM missing"
    fi
    return
  fi

  print_field "IETM" "${dev_name}, int3400_thermal"
  local dev_path="/sys/bus/platform/devices/${dev_name}"
  check_data_vault "${dev_path}"
  check_current_policy "${dev_path}"
}

acpi_path_name() {
  local sysfs_path="$1"
  local acpi_path
  acpi_path="$(read_sysfs "${sysfs_path}/path")"
  if [[ -z "${acpi_path}" ]]; then
    return
  fi
  local name="${acpi_path##*.}"
  echo "${name%%_*}"
}

scan_acpi_participants() {
  local entry
  for entry in "${ACPI_DEVICES}"/INTC* \
               "${ACPI_DEVICES}"/INT3*; do
    [[ -d "${entry}" ]] || continue
    local name
    name="$(acpi_path_name "${entry}")"
    if [[ -n "${name}" ]]; then
      ACPI_MAP["${name}"]="${entry}"
    fi
  done
}

participant_status() {
  local sysfs_path="$1"
  local status
  status="$(read_sysfs "${sysfs_path}/status")"
  [[ -n "${status}" ]] && (( status > 0 ))
}

check_one_participant() {
  local name="$1"
  local sysfs="${ACPI_MAP[${name}]:-}"
  if [[ -z "${sysfs}" ]]; then
    print_field "${name}" "not present"
    return
  fi

  local instance
  instance="$(basename "${sysfs}")"

  if ! participant_status "${sysfs}"; then
    print_field "${name}" "${instance}, inactive"
    return
  fi

  local driver
  driver="$(platform_device_driver "${instance}")"
  if [[ -n "${driver}" ]]; then
    print_field "${name}" "${instance}, ${driver}"
  else
    print_field "${name}" "${instance}, no driver"
  fi
}

check_proc_thermal() {
  printf "%-28s" "TCPU"
  if [[ ! -d "${PROC_THERMAL_SYSFS}" ]]; then
    echo "not present"
    return
  fi
  local driver
  driver="$(sysfs_driver "${PROC_THERMAL_SYSFS}")"
  if [[ -n "${driver}" ]]; then
    echo "${driver} (PCI ${PROC_THERMAL_PCI})"
  else
    echo "no driver (PCI ${PROC_THERMAL_PCI})"
  fi
}

check_participants() {
  echo ""
  echo "--- participants ---"
  echo ""

  scan_acpi_participants
  check_proc_thermal
  local name
  for name in "${PARTICIPANTS[@]}"; do
    if [[ "${name}" == "IETM" ]]; then
      continue
    fi
    check_one_participant "${name}"
  done
}

scan_thermal_zones() {
  local zone_dir
  for zone_dir in "${THERMAL_CLASS}"/thermal_zone*; do
    [[ -d "${zone_dir}" ]] || continue
    local zone_type
    zone_type="$(read_sysfs "${zone_dir}/type")"
    if [[ -n "${zone_type}" ]]; then
      ZONE_MAP["${zone_type}"]="${zone_dir}"
    fi
  done
}

zone_trip_temp() {
  local zone_dir="$1" trip_type="$2"
  local i=0
  while [[ -f "${zone_dir}/trip_point_${i}_type" ]]; do
    local ttype
    ttype="$(read_sysfs "${zone_dir}/trip_point_${i}_type")"
    if [[ "${ttype}" == "${trip_type}" ]]; then
      local raw
      raw="$(read_sysfs "${zone_dir}/trip_point_${i}_temp")"
      if [[ -n "${raw}" ]] && (( raw > -273000 )); then
        echo "${raw}"
      fi
      return
    fi
    (( i++ )) || true
  done
}

zone_policy() {
  local zone_dir="$1"
  read_sysfs "${zone_dir}/policy"
}

format_zone_info() {
  local zone_dir="$1"
  local raw
  raw="$(read_sysfs "${zone_dir}/temp")"
  if [[ -z "${raw}" ]] || (( raw <= 0 )); then
    echo "no reading"
    return
  fi

  local info
  info="$(format_temp_mc "${raw}")"
  local trips=""

  local passive
  passive="$(zone_trip_temp "${zone_dir}" passive)"
  if [[ -n "${passive}" ]]; then
    trips="passive $(( passive / 1000 )) C"
  fi

  local critical
  critical="$(zone_trip_temp "${zone_dir}" critical)"
  if [[ -n "${critical}" ]]; then
    trips="${trips:+${trips}, }critical $(( critical / 1000 )) C"
  fi

  if [[ -n "${trips}" ]]; then
    info="${info}  (${trips})"
  fi

  local policy
  policy="$(zone_policy "${zone_dir}")"
  if [[ "${policy}" == "user_space" ]]; then
    info="${info}  [thermald]"
  fi

  echo "${info}"
}

check_thermal_zones() {
  echo ""
  echo "--- thermal zones ---"
  echo ""

  scan_thermal_zones
  local shown_proc=0
  local zone_name
  for zone_name in "${MONITORED_ZONES[@]}"; do
    if [[ "${zone_name}" == "TCPU" ]] && (( shown_proc )); then
      continue
    fi
    local zone_dir="${ZONE_MAP[${zone_name}]:-}"
    if [[ -z "${zone_dir}" ]]; then
      if [[ "${zone_name}" == "B0D4" ]]; then
        continue
      fi
      print_field "${zone_name}" "not found"
      continue
    fi
    if [[ "${zone_name}" == "B0D4" ]] || [[ "${zone_name}" == "TCPU" ]]; then
      shown_proc=1
    fi
    print_field "${zone_name}" "$(format_zone_info "${zone_dir}")"
  done
}

check_rapl_pl1() {
  local label="$1" base="$2"
  if [[ ! -d "${base}" ]]; then
    print_field "${label}" "not available"
    return
  fi
  local pl1_uw
  pl1_uw="$(read_sysfs "${base}/constraint_0_power_limit_uw")"
  if [[ -z "${pl1_uw}" ]] || (( pl1_uw == 0 )); then
    print_field "${label}" "present, no reading"
    return
  fi
  print_field "${label}" "$(format_uw "${pl1_uw}")"
}

check_ppcc() {
  local limits="${PROC_THERMAL_SYSFS}/power_limits"
  [[ -d "${limits}" ]] || return 0

  local min
  min="$(read_sysfs "${limits}/power_limit_0_min_uw")"
  local max
  max="$(read_sysfs "${limits}/power_limit_0_max_uw")"
  local step
  step="$(read_sysfs "${limits}/power_limit_0_step_uw")"
  [[ -n "${min}" ]] && [[ -n "${max}" ]] || return 0

  if [[ "${min}" == "${max}" ]]; then
    print_warn "PPCC" \
      "$(format_uw "${min}") (locked, min == max)"
    return
  fi

  local info
  info="$(format_uw "${min}") - $(format_uw "${max}")"
  if [[ -n "${step}" ]] && (( step > 0 )); then
    info="${info}, step $(format_uw "${step}")"
  fi
  print_field "PPCC" "${info}"
}

check_rapl() {
  echo ""
  echo "--- rapl ---"
  echo ""
  check_rapl_pl1 "PL1 (MMIO)" "${RAPL_MMIO_PKG}"
  check_rapl_pl1 "PL1 (MSR)" "${RAPL_MSR}"
  check_ppcc
}

check_cfg_lock() {
  local lock_val
  if ! lock_val="$(rdmsr -f 15:15 ${MSR_PKG_CST_CONFIG} 2>&1)"; then
    print_warn "cfg lock" \
      "cannot read MSR 0xE2: ${lock_val}"
    return
  fi

  if [[ "${lock_val}" == "0" ]]; then
    print_field "cfg lock" "disabled"
    check_rapl_writable
  else
    print_field "cfg lock" "ENABLED"
    WARNINGS+=("cfg lock: RAPL writes blocked -- see GUIDE.md")
  fi
}

check_rapl_writable() {
  local pl1_path="${RAPL_MSR}/constraint_0_power_limit_uw"
  [[ -f "${pl1_path}" ]] || return 0

  local current
  current="$(cat "${pl1_path}")"
  if echo "${current}" > "${pl1_path}" 2>/dev/null; then
    print_detail \
      "RAPL PL1 writable ($(format_uw "${current}"))"
  else
    print_warn "rapl" "PL1 write failed despite CFG Lock off"
  fi
}

check_tcc_offset() {
  local offset_hex
  if ! offset_hex="$(rdmsr -f 29:24 ${MSR_TEMP_TARGET} 2>&1)"; then
    print_warn "tcc offset" \
      "cannot read MSR 0x1A2: ${offset_hex}"
    return
  fi

  local offset_dec=$(( 16#${offset_hex} ))
  local throttle_at=$(( TJMAX_FUSED - offset_dec ))

  if (( offset_dec == STOCK_TCC_OFFSET )); then
    print_field "tcc offset" \
      "stock (${offset_dec} C, throttle at ${throttle_at} C)"
    WARNINGS+=("tcc offset: stock value -- see GUIDE.md")
  elif (( offset_dec < STOCK_TCC_OFFSET )); then
    print_field "tcc offset" \
      "${offset_dec} C, throttle at ${throttle_at} C (improved)"
  else
    print_warn "tcc offset" \
      "${offset_dec} C, throttle at ${throttle_at} C (worse than stock)"
  fi
}

check_bios_settings() {
  echo ""
  echo "--- bios settings ---"
  echo ""

  if ! has_cmd rdmsr; then
    echo "skipped (install msr-tools)"
    WARNINGS+=("install msr-tools to check BIOS settings")
    return
  fi

  modprobe msr 2>/dev/null || true
  check_cfg_lock
  check_tcc_offset
}

print_warnings() {
  if (( ${#WARNINGS[@]} == 0 )); then
    return
  fi

  echo ""
  echo "--- warnings ---"
  echo ""
  local w
  for w in "${WARNINGS[@]}"; do
    echo "${w}"
  done
}

main() {
  require_root
  check_modules
  check_dptf_manager
  check_participants
  check_thermal_zones
  check_rapl
  check_bios_settings
  print_warnings
}

main "$@"
