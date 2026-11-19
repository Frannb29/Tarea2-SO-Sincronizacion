// MOCK de MonitorRW
// exclusion mutua correcta, pero solo prioridad de escritores, ignora 'politica'
#include "monitor_rw.h"

MonitorRW::MonitorRW(Politica p) : politica(p) {}

void MonitorRW::anotar(Tipo t, Accion a, long turno) {
    if (traza_activa) traza.push_back({id_hebra, t, a, turno});
}
void MonitorRW::activar_traza(bool b) { std::lock_guard<std::mutex> g(m); traza_activa = b; }
std::vector<AnotacionTraza> MonitorRW::obtener_traza() { std::lock_guard<std::mutex> g(m); return traza; }

void MonitorRW::entrar_lectura() {
    std::unique_lock<std::mutex> lk(m);
    anotar(Tipo::Lectora, Accion::Pide);
    while (esc_act || esc_esp > 0) cv.wait(lk);
    lect_act++;
    anotar(Tipo::Lectora, Accion::Entra);
}
void MonitorRW::salir_lectura() {
    std::unique_lock<std::mutex> lk(m);
    lect_act--;
    anotar(Tipo::Lectora, Accion::Sale);
    cv.notify_all();
}
void MonitorRW::entrar_escritura() {
    std::unique_lock<std::mutex> lk(m);
    anotar(Tipo::Escritora, Accion::Pide);
    esc_esp++;
    while (esc_act || lect_act > 0) cv.wait(lk);
    esc_esp--; esc_act = true;
    anotar(Tipo::Escritora, Accion::Entra);
}
void MonitorRW::salir_escritura() {
    std::unique_lock<std::mutex> lk(m);
    esc_act = false;
    anotar(Tipo::Escritora, Accion::Sale);
    cv.notify_all();
}