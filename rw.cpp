#include <iostream>
#include "common.h"
#include "monitor_rw.h"
#include "control_atomico_rw.h"

thread_local int id_hebra = -1;
thread_local FallosAtomico fallos_hebra{};

int main() {
    MonitorRW mon(Politica::Escritores);
    ControlAtomicoRW atom;
    std::cout << "Compilacion base OK\n";
    return 0;
}