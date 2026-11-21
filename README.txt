# Tarea2-SO-SincronizacionTAREA 2 - LECTORES Y ESCRITORES

ARCHIVOS
  common.h                   Tipos compartidos: Politica, Tipo, Accion, AnotacionTraza, Registro,
                             ControlRW, FallosAtomico, ResultadoVerif.
  rw.cpp                     main: parseo, hebras, registro, diagnostico, mediciones.
  verificador.cpp            verificar_traza() e indicadores de evidencia.
  monitor_rw.h/.cpp          MonitorRW: modo cond; politicas lectores, escritores, fifo.
  control_atomico_rw.h/.cpp  ControlAtomicoRW.
  Makefile                   make | make tsan | make clean
  run_experimentos.sh        Ejecuta E1 y E2 y genera resultados_e1.txt / resultados_e2.txt.

COMPILAR
  make                   genera ./rw  (g++ -std=c++17 -O2 -Wall -Wextra -pthread)
  make tsan              genera ./rw_tsan con -fsanitize=thread
  make clean

EJECUTAR
  ./rw <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]
     modo:     cond | atomic            (atomic solo admite politica escritores)
     politica: lectores | escritores | fifo
  Ejemplos:
     ./rw cond escritores 6 2 100 200 500
     ./rw atomic escritores 6 2 100 200 500
     ./rw cond escritores 6 2 100 200 500 notraza
  Variable de entorno opcional para el anexo (volcado de la traza completa a un archivo):
     RW_VOLCAR_TRAZA=traza_fifo.txt ./rw cond fifo 6 2 100 200 500
     (formato: idx hebra tipo(L/E) accion turno)
  Codigo de salida: 0 = todo correcto; 1 = error de uso/hebras; 3 = fallo de correcto o politica_ok.

REPRODUCIR RESULTADOS
  make && bash run_experimentos.sh
  (E2 usa K=10000 en la configuracion 8 8 0 0; todos los casos de E2 usan notraza.)

CARRERAS DE DATOS
  make tsan && ./rw_tsan cond escritores 4 2 20 100 100
  TSan reduce la velocidad y solo detecta carreras ocurridas en esa ejecucion:
  la ausencia de alertas NO prueba correccion.

NOTA: ejecutar en un directorio sin noexec, algunos montajes impiden ejecutar binarios.
Repositorio: <https://github.com/Frannb29/Tarea2-SO-Sincronizacion.git>