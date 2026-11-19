#pragma once
#include <atomic>
#include <cstdint>
#include "common.h"

class ControlAtomicoRW : public ControlRW {
public:
    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;
private:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free, "atomic<uint64_t> debe ser lock-free");
    // MOCK
    static constexpr std::uint64_t LECT_MASK = 0xFFFFull, ESC_ACT = 1ull << 16, ESP_UNO = 1ull << 32;
    std::atomic<std::uint64_t> estado{0};
};