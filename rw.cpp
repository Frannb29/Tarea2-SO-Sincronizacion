#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <time.h>
#include <vector>
#include "common.h"
#include "monitor_rw.h"
#include "control_atomico_rw.h"

thread_local int id_hebra = -1;
thread_local FallosAtomico fallos_hebra;

using Reloj = std::chrono::steady_clock;

// Contadores de diagnostico
struct Diagnostico {
    std::atomic<int> lect_dentro{0};
    std::atomic<int> esc_dentro{0};
    std::atomic<int> max_lect{0};
    std::atomic<long> violaciones{0};
};

// Mide cuanto tiempo de espera hay entre pedir y entrar a la seccion critica y cuantas inconsistencias se detectan.
// alignas(64) separa cada elemento en distintas lineas de cache para evitar que los nucleos se invaliden entre si
// al actualizar los contadores de cada hebra
struct alignas(64) Estadisticas {
    long n = 0;
    long long suma_ns = 0, max_ns = 0;
    long inconsistencias = 0;
    FallosAtomico fallos;
};

static void pausa(long us) {
    if (us > 0) std::this_thread::sleep_for(std::chrono::microseconds(us));
}

static void registrar_espera(Estadisticas& st, Reloj::time_point t0, Reloj::time_point t1) {
    long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    st.n++; st.suma_ns += ns; if (ns > st.max_ns) st.max_ns = ns;
}

static void lectora(ControlRW* c, int id, long K, long dur, Registro* reg, Diagnostico* d, Estadisticas* st) {
    id_hebra = id;
    for (long k = 0; k < K; ++k) {
        auto t0 = Reloj::now();
        c->entrar_lectura();
        registrar_espera(*st, t0, Reloj::now());
        int n = d->lect_dentro.fetch_add(1) + 1;
        if (d->esc_dentro.load() != 0) d->violaciones++;
        int m = d->max_lect.load();
        while (n > m && !d->max_lect.compare_exchange_weak(m, n)) {}
        long v = reg->version;
        // Pausa para simular trabajo en la seccion critica, para verificar que no hay violaciones de exclusion mutua
        pausa(dur);
        long dv = reg->doble_version;
        if (dv != 2 * v) st->inconsistencias++;
        d->lect_dentro.fetch_sub(1);
        c->salir_lectura();
    }
    st->fallos = fallos_hebra;
}

static void escritora(ControlRW* c, int id, long K, long dur, Registro* reg, Diagnostico* d, Estadisticas* st) {
    id_hebra = id;
    for (long k = 0; k < K; ++k) {
        auto t0 = Reloj::now();
        c->entrar_escritura();
        registrar_espera(*st, t0, Reloj::now());
        int prev = d->esc_dentro.fetch_add(1);
        if (prev != 0 || d->lect_dentro.load() != 0) d->violaciones++;
        // Modificacion no atomica intencional para verificar que no hay violaciones de exclusion mutua
        reg->version++;
        pausa(dur);
        reg->doble_version = 2 * reg->version;
        d->esc_dentro.fetch_sub(1);
        c->salir_escritura();
    }
    st->fallos = fallos_hebra;
}

static bool parse_long(const char* s, long& out) {
    if (!s || !*s) return false;
    char* fin; errno = 0;
    long v = std::strtol(s, &fin, 10);
    if (errno != 0 || *fin != '\0') return false;
    out = v; return true;
}

static int uso(const char* msg) {
    if (msg) std::fprintf(stderr, "Error: %s\n", msg);
    std::fprintf(stderr, "Uso: ./rw <cond|atomic> <lectores|escritores|fifo> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]\n");
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 8 && argc != 9) return uso("numero de argumentos incorrecto");
    std::string modo = argv[1], pol = argv[2];
    bool traza_on = true;
    if (argc == 9) {
        if (std::string(argv[8]) != "notraza") return uso("el argumento opcional solo puede ser 'notraza'");
        traza_on = false;
    }
    if (modo != "cond" && modo != "atomic") return uso("modo invalido (cond|atomic)");
    Politica p;
    if (pol == "lectores") p = Politica::Lectores;
    else if (pol == "escritores") p = Politica::Escritores;
    else if (pol == "fifo") p = Politica::Fifo;
    else return uso("politica invalida (lectores|escritores|fifo)");
    if (modo == "atomic" && p != Politica::Escritores)
        return uso("el modo atomic solo implementa la politica 'escritores'");

    long L, E, K, dl, de;
    if (!parse_long(argv[3], L) || !parse_long(argv[4], E) || !parse_long(argv[5], K) ||
        !parse_long(argv[6], dl) || !parse_long(argv[7], de))
        return uso("los parametros numericos deben ser enteros");
    if (L < 1 || E < 1 || K < 1) return uso("se exige L>=1, E>=1, K>=1");
    if (L + E > 1000) return uso("se exige L+E<=1000");
    if (dl < 0 || de < 0) return uso("las duraciones deben ser >= 0");
    long tmp, ek;
    if (__builtin_mul_overflow(E, K, &ek) || __builtin_mul_overflow(2L, ek, &tmp) ||
        __builtin_mul_overflow(L + E, K, &tmp))
        return uso("2*E*K o (L+E)*K no caben en un long");

    Registro reg;
    Diagnostico diag;
    std::unique_ptr<MonitorRW> mon;
    std::unique_ptr<ControlAtomicoRW> atom;
    ControlRW* ctl;
    if (modo == "cond") { mon.reset(new MonitorRW(p)); mon->activar_traza(traza_on); ctl = mon.get(); }
    else { atom.reset(new ControlAtomicoRW()); ctl = atom.get(); }

    const int N = (int)(L + E);
    std::vector<Estadisticas> st(N);
    std::vector<std::thread> hebras;
    hebras.reserve(N);

    // Region de medicion, antes de crear hebras hasta completar todos los join()
    timespec c0, c1;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c0);
    auto w0 = Reloj::now();
    bool error_creacion = false;
    for (int i = 0; i < N; ++i) {
        try {
            if (i < L) hebras.emplace_back(lectora, ctl, i, K, dl, &reg, &diag, &st[i]);
            else       hebras.emplace_back(escritora, ctl, i, K, de, &reg, &diag, &st[i]);
        } catch (const std::system_error& e) {
            std::fprintf(stderr, "Error: no se pudo crear la hebra %d (%s)\n", i, e.what());
            error_creacion = true; break;
        }
    }
    for (auto& h : hebras) h.join();
    auto w1 = Reloj::now();
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c1);
    if (error_creacion) return 1;

    double pared = std::chrono::duration<double>(w1 - w0).count();
    double cpu = (c1.tv_sec - c0.tv_sec) + (c1.tv_nsec - c0.tv_nsec) / 1e9;

    long incons = 0, nl = 0, ne = 0;
    long long sl = 0, se = 0, ml = 0, me = 0;
    FallosAtomico f;
    for (int i = 0; i < N; ++i) {
        incons += st[i].inconsistencias;
        f.ocupado += st[i].fallos.ocupado;
        f.escritores_esperando += st[i].fallos.escritores_esperando;
        f.cas_fallido += st[i].fallos.cas_fallido;
        if (i < L) { nl += st[i].n; sl += st[i].suma_ns; if (st[i].max_ns > ml) ml = st[i].max_ns; }
        else       { ne += st[i].n; se += st[i].suma_ns; if (st[i].max_ns > me) me = st[i].max_ns; }
    }
    double lec_prom = nl ? sl / 1e3 / nl : 0, esc_prom = ne ? se / 1e3 / ne : 0;
    long esperado_v = E * K, esperado_d = 2 * E * K;
    bool correcto = reg.version == esperado_v && reg.doble_version == esperado_d &&
                    incons == 0 && diag.violaciones.load() == 0;

    ResultadoVerif vr;
    bool verifica = (modo == "cond" && traza_on);
    if (verifica) {
        std::vector<AnotacionTraza> tr = mon->obtener_traza();
        vr = verificar_traza(tr, p);
        if (const char* arch = std::getenv("RW_VOLCAR_TRAZA")) {
            if (FILE* fp = std::fopen(arch, "w")) {
                std::fprintf(fp, "# idx hebra tipo accion turno\n");
                for (size_t i = 0; i < tr.size(); ++i)
                    std::fprintf(fp, "%zu %d %c %s %ld\n", i, tr[i].hebra,
                        tr[i].tipo == Tipo::Lectora ? 'L' : 'E',
                        tr[i].accion == Accion::Pide ? "Pide" : tr[i].accion == Accion::Entra ? "Entra" : "Sale",
                        tr[i].turno);
                std::fclose(fp);
            }
        }
    }

    std::printf("\n============================================================\n");
    std::printf("  RESUMEN DE EJECUCION: LECTORES Y ESCRITORES\n");
    std::printf("============================================================\n");
    std::printf("  Modo / Politica   : %s / %s (Traza: %s)\n",
                modo.c_str(), pol.c_str(), traza_on ? "SI" : "NO");
    std::printf("  Hebras (L / E)    : %ld lectoras, %ld escritoras (K = %ld)\n", L, E, K);
    std::printf("  Duracion trabajo  : Lectura = %ld us | Escritura = %ld us\n", dl, de);
    std::printf("  Total operaciones : %ld (%ld lecturas, %ld escrituras)\n",
                (L + E) * K, L * K, E * K);

    std::printf("------------------------------------------------------------\n");
    std::printf("  INTEGRIDAD Y EXCLUSION MUTUA\n");
    std::printf("------------------------------------------------------------\n");
    std::printf("  version_final             : %ld (esperado: %ld)\n", reg.version, esperado_v);
    std::printf("  doble_version_final       : %ld (esperado: %ld)\n", reg.doble_version, esperado_d);
    std::printf("  inconsistencias           : %ld\n", incons);
    std::printf("  violaciones_exclusion     : %ld\n", diag.violaciones.load());
    std::printf("  lectores_simultaneos_max  : %d\n", diag.max_lect.load());

    std::printf("------------------------------------------------------------\n");
    std::printf("  RENDIMIENTO Y TIEMPOS DE ESPERA\n");
    std::printf("------------------------------------------------------------\n");
    std::printf("  Tiempo pared / CPU        : %.6f s / %.6f s\n", pared, cpu);
    std::printf("  Espera lectoras (us)      : prom = %8.2f | max = %8.2f\n", lec_prom, ml / 1e3);
    std::printf("  Espera escritoras (us)    : prom = %8.2f | max = %8.2f\n", esc_prom, me / 1e3);
    if (modo == "atomic") {
        std::printf("  Fallos atomicos           : ocupado=%ld | esc_esp=%ld | cas=%ld\n",
                    f.ocupado, f.escritores_esperando, f.cas_fallido);
    }

    std::printf("============================================================\n");
    std::printf("  RESULTADO FINAL: correcto=%s", correcto ? "SI" : "NO");
    if (verifica) {
        std::printf(" | politica_ok=%s (evidencia=%ld)", vr.ok ? "SI" : "NO", vr.evidencia);
    }
    std::printf("\n============================================================\n");
    if (verifica && !vr.ok) {
        std::printf("  [!] Violacion en anotacion %ld: %s\n", vr.anotacion, vr.regla.c_str());
    }

    // Linea para scripts: modo,politica,L,E,K,dl,de,pared,cpu,lecprom,lecmax,escprom,escmax,lecsim,fallos,correcto,politica_ok,evidencia
    std::printf("CSV,%s,%s,%ld,%ld,%ld,%ld,%ld,%.6f,%.6f,%.2f,%.2f,%.2f,%.2f,%d,%ld,%s,%s,%ld\n",
                modo.c_str(), pol.c_str(), L, E, K, dl, de, pared, cpu, lec_prom, ml / 1e3, esc_prom, me / 1e3,
                diag.max_lect.load(), f.ocupado + f.escritores_esperando + f.cas_fallido,
                correcto ? "SI" : "NO", verifica ? (vr.ok ? "SI" : "NO") : "NA", verifica ? vr.evidencia : 0L);

    return (correcto && (!verifica || vr.ok)) ? 0 : 3;
}