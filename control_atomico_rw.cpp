// MOCK funcional de ControlAtomicoRW
#include <thread>
#include "control_atomico_rw.h"

void ControlAtomicoRW::entrar_lectura() {
    for (;;) {
        std::uint64_t s = estado.load();
        if (s & ESC_ACT) { fallos_hebra.ocupado++; std::this_thread::yield(); continue; }
        if (s >> 32)     { fallos_hebra.escritores_esperando++; std::this_thread::yield(); continue; }
        if (estado.compare_exchange_weak(s, s + 1)) return;
        fallos_hebra.cas_fallido++;
    }
}
void ControlAtomicoRW::salir_lectura() { estado.fetch_sub(1); }
void ControlAtomicoRW::entrar_escritura() {
    estado.fetch_add(ESP_UNO);
    for (;;) {
        std::uint64_t s = estado.load();
        if ((s & ESC_ACT) || (s & LECT_MASK)) { fallos_hebra.ocupado++; std::this_thread::yield(); continue; }
        if (estado.compare_exchange_weak(s, s - ESP_UNO + ESC_ACT)) return;
        fallos_hebra.cas_fallido++;
    }
}
void ControlAtomicoRW::salir_escritura() { estado.fetch_sub(ESC_ACT); }