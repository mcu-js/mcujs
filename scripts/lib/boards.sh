#!/usr/bin/env bash

# Supported release boards in deterministic build and package order.
MCUJS_BOARDS=(
    pico
    pico2
    pico2_w
    waveshare_rp2040_zero
    waveshare_rp2040_pizero
    waveshare_rp2040_touch_lcd_1.28
    waveshare_rp2350_lcd_1.47_a
    waveshare_rp2350_touch_lcd_1.69
    adafruit_feather_rp2040
)

mcujs_list_boards() {
    printf '%s\n' "${MCUJS_BOARDS[@]}"
}

mcujs_is_board() {
    local candidate="${1:-}"
    local board
    for board in "${MCUJS_BOARDS[@]}"; do
        if [[ "${candidate}" == "${board}" ]]; then
            return 0
        fi
    done
    return 1
}

mcujs_board_label() {
    case "${1:-}" in
        pico) printf 'Raspberry Pi Pico' ;;
        pico2) printf 'Raspberry Pi Pico 2' ;;
        pico2_w) printf 'Raspberry Pi Pico 2 W' ;;
        waveshare_rp2040_zero) printf 'Waveshare RP2040-Zero' ;;
        waveshare_rp2040_pizero) printf 'Waveshare RP2040-PiZero' ;;
        waveshare_rp2040_touch_lcd_1.28) printf 'Waveshare RP2040 Touch LCD 1.28' ;;
        waveshare_rp2350_lcd_1.47_a) printf 'Waveshare RP2350-LCD-1.47-A' ;;
        waveshare_rp2350_touch_lcd_1.69) printf 'Waveshare RP2350-Touch-LCD-1.69' ;;
        adafruit_feather_rp2040) printf 'Adafruit Feather RP2040' ;;
        *) return 1 ;;
    esac
}

mcujs_board_chip() {
    case "${1:-}" in
        pico|waveshare_rp2040_zero|waveshare_rp2040_pizero|waveshare_rp2040_touch_lcd_1.28|adafruit_feather_rp2040)
            printf 'RP2040'
            ;;
        pico2|pico2_w|waveshare_rp2350_lcd_1.47_a|waveshare_rp2350_touch_lcd_1.69)
            printf 'RP2350'
            ;;
        *)
            return 1
            ;;
    esac
}

mcujs_board_flash() {
    case "${1:-}" in
        pico|waveshare_rp2040_zero) printf '2MB' ;;
        pico2|pico2_w|waveshare_rp2040_touch_lcd_1.28) printf '4MB' ;;
        adafruit_feather_rp2040) printf '8MB' ;;
        waveshare_rp2040_pizero|waveshare_rp2350_lcd_1.47_a|waveshare_rp2350_touch_lcd_1.69) printf '16MB' ;;
        *) return 1 ;;
    esac
}

mcujs_board_features() {
    case "${1:-}" in
        pico) printf 'Onboard LED' ;;
        pico2) printf 'Onboard LED' ;;
        pico2_w) printf 'CYW43 LED support' ;;
        waveshare_rp2040_zero) printf 'Onboard NeoPixel' ;;
        waveshare_rp2040_pizero) printf 'DVI/HDMI output' ;;
        waveshare_rp2040_touch_lcd_1.28) printf 'Round LCD, touch, IMU' ;;
        waveshare_rp2350_lcd_1.47_a) printf 'LCD, NeoPixel' ;;
        waveshare_rp2350_touch_lcd_1.69) printf 'LCD, touch, IMU, buzzer' ;;
        adafruit_feather_rp2040) printf 'NeoPixel, STEMMA QT' ;;
        *) return 1 ;;
    esac
}

mcujs_print_board_list() {
    local board
    for board in "${MCUJS_BOARDS[@]}"; do
        printf '  %-36s %s\n' "${board}" "$(mcujs_board_label "${board}")"
    done
}

mcujs_print_board_markdown_table() {
    local board
    printf '| Board ID | Board | Chip | Flash | Notes |\n'
    printf '| --- | --- | --- | --- | --- |\n'
    for board in "${MCUJS_BOARDS[@]}"; do
        printf '| `%s` | %s | %s | %s | %s |\n' \
            "${board}" \
            "$(mcujs_board_label "${board}")" \
            "$(mcujs_board_chip "${board}")" \
            "$(mcujs_board_flash "${board}")" \
            "$(mcujs_board_features "${board}")"
    done
}
