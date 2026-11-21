#include "common.h"

ResultadoVerif verificar_traza(const std::vector<AnotacionTraza>& t, Politica p) {
    ResultadoVerif r;
    // lectoras y escritoras, activas y pendientes
    long lact = 0, eact = 0, lpend = 0, epend = 0;
    long k_pide = 0, k_entra = 0;
    auto viola = [&](size_t i, const char* regla) {
        r.ok = false; r.anotacion = (long)i; r.regla = regla; return r;
    };
    for (size_t i = 0; i < t.size(); ++i) {
        const AnotacionTraza& a = t[i];
        const bool L = (a.tipo == Tipo::Lectora);
        switch (a.accion) {
        case Accion::Pide:
            if (p == Politica::Fifo && a.turno != k_pide)
                return viola(i, "FIFO: los turnos de Pide no son consecutivos desde 0");
            k_pide++;
            if (L && p == Politica::Escritores && epend > 0) r.evidencia++;
            if (L && p == Politica::Lectores && eact == 0) {
                // si una lectora pide sin escritora activa, debe entrar altiro.
                // Por eso se revisa si la siguiente anotacion en la traza es Entra de la misma hebra.
                if (i + 1 >= t.size() || t[i + 1].hebra != a.hebra ||
                    t[i + 1].tipo != Tipo::Lectora || t[i + 1].accion != Accion::Entra)
                    return viola(i, "Lectores: lectora pide sin escritora activa y no entra de inmediato");
            }
            (L ? lpend : epend)++;
            break;
        case Accion::Entra:
            if ((L ? lpend : epend) == 0) return viola(i, "Entra sin Pide previo");
            (L ? lpend : epend)--; // se descuenta a si misma de las pendientes
            if (L) {
                if (eact > 0) return viola(i, "Todas: lectora entra con escritora activa");
                if (p == Politica::Escritores && epend > 0)
                    return viola(i, "Escritores: lectora entra con escritoras pendientes");
                if (p == Politica::Lectores && epend > 0) r.evidencia++;
                if (p == Politica::Fifo && lact > 0) r.evidencia++;
            } else {
                if (lact > 0 || eact > 0) return viola(i, "Todas: escritora entra con lectoras o escritora activas");
                if (p == Politica::Lectores && lpend > 0)
                    return viola(i, "Lectores: escritora entra con lectoras pendientes");
            }
            if (p == Politica::Fifo && a.turno != k_entra)
                return viola(i, "FIFO: el k-esimo Entra no corresponde al turno k");
            k_entra++;
            (L ? lact : eact)++;
            break;
        case Accion::Sale:
            if ((L ? lact : eact) == 0) return viola(i, "Sale sin Entra previo");
            (L ? lact : eact)--;
            break;
        }
    }
    if (lact || eact || lpend || epend)
        return viola(t.size(), "Traza incompleta: quedan solicitudes activas o pendientes al final");
    return r;
}