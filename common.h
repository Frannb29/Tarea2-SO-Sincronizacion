#pragma once
#include <string>
#include <vector>

enum class Politica { Lectores, Escritores, Fifo };
enum class Tipo { Lectora, Escritora };
enum class Accion { Pide, Entra, Sale };

struct AnotacionTraza {
    int hebra; // identificador lógico de la hebra
    Tipo tipo;
    Accion accion;
    long turno; // turno FIFO; -1 en las otras políticas
};

struct Registro {
    long version = 0;
    long doble_version = 0;
};

// Interfaz comun
class ControlRW {
public:
    virtual ~ControlRW() = default;
    virtual void entrar_lectura() = 0;
    virtual void salir_lectura() = 0;
    virtual void entrar_escritura() = 0;
    virtual void salir_escritura() = 0;
};

struct FallosAtomico {
    long ocupado = 0;
    long escritores_esperando = 0;
    long cas_fallido = 0;
};

extern thread_local int id_hebra;
extern thread_local FallosAtomico fallos_hebra;

struct ResultadoVerif {
    bool ok = true;
    long anotacion = -1; // indice de la primera anotacion que rompe una regla
    std::string regla; // regla rota
    long evidencia = 0;
};

ResultadoVerif verificar_traza(const std::vector<AnotacionTraza>& traza, Politica p);

inline const char* nombre_politica(Politica p) {
    switch (p) {
        case Politica::Lectores: return "lectores";
        case Politica::Escritores: return "escritores";
        default: return "fifo";
    }
}
