#!/bin/bash
# Uso: bash run_experimentos.sh, requiere ./rw compilado con make
# E1: 5 ejecuciones por caso con traza -> resultados_e1.txt
# E2: politica escritores cond vs atomic con notraza, promedio de 5 -> resultados_e2.txt
CASOS=("1 1 20 500 500" "6 2 100 200 500" "2 6 100 500 200" "8 8 100 0 0" "8 1 200 1000 100")
REP=5
: > resultados_e1.txt; : > resultados_e2.txt

for c in "${CASOS[@]}"; do
  for pol in lectores escritores fifo; do
    for i in $(seq $REP); do
      echo "### cond $pol $c (rep $i)" >> resultados_e1.txt
      ./rw cond $pol $c >> resultados_e1.txt
    done
  done
  for i in $(seq $REP); do
    echo "### atomic escritores $c (rep $i)" >> resultados_e1.txt
    ./rw atomic escritores $c >> resultados_e1.txt
  done
done

# columnas CSV: 2 modo,3 pol,4 L,5 E,6 K,7 dl,8 de,9 pared,10 cpu,11 lecprom,12 lecmax,13 escprom,14 escmax,15 lecsim,16 fallos
echo "modo L E K dl de pared_s cpu_s lec_prom_us lec_max_us esc_prom_us esc_max_us lec_sim fallos" >> resultados_e2.txt
for c in "${CASOS[@]}"; do
  set -- $c; L=$1; E=$2; K=$3; DL=$4; DE=$5
  if [ "$L" = 8 ] && [ "$E" = 8 ] && [ "$DL" = 0 ]; then K=10000; fi   # regla de E2
  for modo in cond atomic; do
    for i in $(seq $REP); do ./rw $modo escritores $L $E $K $DL $DE notraza | grep '^CSV'; done |
    awk -F, -v m=$modo -v L=$L -v E=$E -v K=$K -v dl=$DL -v de=$DE '
      {for(i=9;i<=16;i++) s[i]+=$i; n++}
      END{printf "%s %s %s %s %s %s", m,L,E,K,dl,de; for(i=9;i<=16;i++) printf " %.4f", s[i]/n; print ""}' >> resultados_e2.txt
  done
done
echo "Listo: resultados_e1.txt y resultados_e2.txt"