#pragma once
#include <condition_variable>
#include <mutex>
#include "common.h"

class MonitorRW : public ControlRW {
public:
    explicit MonitorRW(Politica p);
    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;
    std::vector<AnotacionTraza> obtener_traza();
    void activar_traza(bool b);

private:
    Politica politica;
    std::mutex m;
    std::vector<AnotacionTraza> traza;
    bool traza_activa = true;
    void anotar(Tipo t, Accion a, long turno = -1);
    // MOCK
    std::condition_variable cv;
    int lect_act = 0;
    bool esc_act = false;
    int esc_esp = 0;    
};