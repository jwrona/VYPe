#!/bin/bash
# Ukazka zpracovani jazyku symbolickych adres pro MIPS32 vcetne simulace
# na Linuxovem OS (32-bitovem)
# Pouziti:
#   ./link_and_run.sh jmeno_asm_bez_pripony

# preklad asembleru do objektoveho souboru (prida 0 za koncovku obj)
./assembler2 -i $1.asm -o $1.obj

# spojeni objektoveho souboru do binarniho souboru
./linker $1.obj -o $1.xexe

# simulace binarnho souboru s programem nad zadanym vstupem mips.input
./intersim2 -i $1.xexe -x $1.xml -n mips < $1.in
