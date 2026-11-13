#!/usr/local/bin/bash
## Convierte un fichero de texto a un formato aceptable para Gopher.
## Por Ubuntu Peronista
 
# Revisa si hay un argumento de fichero indicado
if [ $# -eq 0 ]; then
    echo "Uso: $0 <fichero_entrada> [fichero_salida]"
    echo "Si omite FICHERO_SALIDA, el resultado se presenta en la salida estándar."
    exit 1
fi
 
FICHERO_ENTRADA="$1"
FICHERO_SALIDA="$2"
 
# Verifica que exista el fichero de entrada indicado
if [ ! -f "$FICHERO_ENTRADA" ]; then
    echo "Error: No se encontró '$FICHERO_ENTRADA'." >&2
    exit 1
fi
 
# Define la expresión de sed usando citado en ANSI-C ($'...') para operar 
# de forma confiable con codificación UTF-8 los siguientes caracteres poco
# amistosos con gopher:
# \u00ab = «
# \u00bb = »
# \u2026 = …
# \u2014 = —
# \u201c = “
# \u201d = ”
SCRIPT_SED=$'s/\u00ab/"/g; s/\u00bb/"/g; s/\u2026/.../g; s/\u2014/-/g; s/\u201c/"/g; s/\u201d/"/g'
 
## Vuelca el resultado a  un pipe con fold para hacer texto de 72 columnas
## muy amigable con gopher.
if [ -n "$FICHERO_SALIDA" ]; then
    sed "$SCRIPT_SED" "$FICHERO_ENTRADA" | fold -s -w 72 > "$FICHERO_SALIDA"
    echo "El texto fue gopherizado y guardado en '$FICHERO_SALIDA'."
else
    sed "$SCRIPT_SED" "$FICHERO_ENTRADA" | fold -s -w 72
fi
