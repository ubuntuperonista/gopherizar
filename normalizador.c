/*
 * normalizador.c por ~peron
 * GPLv3
 * ANSI C (C89/C90) Normalizador de Caracteres para Linux y BSD 
 * Detecta UTF-8 vs ISO-8859-1 (Latin-1), reeplaza caracteres destino,
 * verifica salida con fichero termporal, y devuelve resultados a stdout.
 *
 * En BSD: compilar con:
 * cc -std=c89 -pedantic -Wall -Wextra -O2 normalizador.c -o normalizador
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    ENC_ASCII,
    ENC_UTF8,
    ENC_ISO8859
} Encoding;

/* Prototipos */
static Encoding detect_encoding(FILE *fp, long *file_size);
static int process_utf8(FILE *in, FILE *out);
static int process_iso8859(FILE *in, FILE *out);
static int verify_and_output(FILE *tmp, long orig_size);

int main(int argc, char *argv[]) {
    FILE *in = NULL;
    FILE *tmp = NULL;
    Encoding enc;
    long orig_size = 0;
    int status = 0;

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <fichero_entrada>\n", argv[0]);
        return EXIT_FAILURE;
    }

    in = fopen(argv[1], "rb");
    if (!in) {
        perror("Error al abrir fichero de entrada");
        return EXIT_FAILURE;
    }

    /* 1. Detecta encoding */
    enc = detect_encoding(in, &orig_size);
    if (enc == ENC_UTF8) {
        fprintf(stderr, "[INFO] Codificación detectada: UTF-8\n");
    } else if (enc == ENC_ISO8859) {
        fprintf(stderr, "[INFO] Codificación detectada: ISO-8859-1 (Latin-1)\n");
    } else {
        fprintf(stderr, "[INFO] Codificación detectada: ASCII Puro (tratando como UTF-8)\n");
    }

    /* Rebobina fichero de entrada para procesado */
    if (fseek(in, 0L, SEEK_SET) != 0) {
        perror("Error de verificación en fichero de entrada");
        fclose(in);
        return EXIT_FAILURE;
    }

    /* 2. Abre fichero temporal seguro (POSIX/ANSI, auto-borra al cerrarlo) */
    tmp = tmpfile();
    if (!tmp) {
        perror("Error al crear fichero temporal");
        fclose(in);
        return EXIT_FAILURE;
    }

    /* 3. Procesa y escribe un fichero temporal */
    if (enc == ENC_ISO8859) {
        status = process_iso8859(in, tmp);
    } else {
        status = process_utf8(in, tmp);
    }

    fclose(in);

    if (status != 0) {
        fprintf(stderr, "[ERROR] Fallo durante conversión de caracteres.\n");
        fclose(tmp);
        return EXIT_FAILURE;
    }

    /* 4. Verifica fichero temporal y salida a stdout */
    if (verify_and_output(tmp, orig_size) != 0) {
        fclose(tmp);
        return EXIT_FAILURE;
    }

    fclose(tmp);
    return EXIT_SUCCESS;
}

/*
 * Analiza el fichero entero para determinar si es UTF-8 o ISO-8859 válido.
 * También guarda el tamaño del fichero original.
 */
static Encoding detect_encoding(FILE *fp, long *file_size) {
    int c;
    int expected_bytes = 0;
    int has_high_bit = 0;
    int is_valid_utf8 = 1;
    long count = 0;

    while ((c = fgetc(fp)) != EOF) {
        unsigned char b = (unsigned char)c;
        count++;

        if (expected_bytes == 0) {
            if (b <= 0x7F) {
                /* ASCII Estandar */
                continue;
            } else if ((b & 0xE0) == 0xC0) {
                /* secuencia UTF-8 de 2-byte (rechaza 0xC0, 0xC1 largos) */
                if (b < 0xC2) {
                    is_valid_utf8 = 0;
                }
                expected_bytes = 1;
                has_high_bit = 1;
            } else if ((b & 0xF0) == 0xE0) {
                /* secuencia UTF-8 3 bytes */
                expected_bytes = 2;
                has_high_bit = 1;
            } else if ((b & 0xF8) == 0xF0) {
                /* secuencia UTF-8 de 5 bytes */
                if (b > 0xF4) {
                    is_valid_utf8 = 0;
                }
                expected_bytes = 3;
                has_high_bit = 1;
            } else {
                /* Byte de inicio UTF-8 inválido (ej, 0x80-0xBF) */
                is_valid_utf8 = 0;
                has_high_bit = 1;
            }
        } else {
            /* Espera byte de continuacion (10xxxxxx) */
            if ((b & 0xC0) == 0x80) {
                expected_bytes--;
            } else {
                is_valid_utf8 = 0;
                expected_bytes = 0;
            }
        }
    }

    if (expected_bytes != 0) {
        is_valid_utf8 = 0;
    }

    *file_size = count;

    if (!has_high_bit) {
        return ENC_ASCII;
    }
    return is_valid_utf8 ? ENC_UTF8 : ENC_ISO8859;
}

/*
 * Procesa stream de entrada UTF-8 y escribe salida a tmp.
 * Nota: Reemplaza Á, É, Í, Ó, Ú -> A, E, I, O, U; Ü -> ü;
 * «, », “, ” -> "; … -> ...; — -> -
 */
static int process_utf8(FILE *in, FILE *out) {
    int c1;

    while ((c1 = fgetc(in)) != EOF) {
        unsigned char b1 = (unsigned char)c1;

        /* secuencias de 2-Byte UTF-8 que empiezan con 0xC3 (mayus acent) */
        if (b1 == 0xC3) {
            int c2 = fgetc(in);
            if (c2 == EOF) return -1;
            switch ((unsigned char)c2) {
                case 0x81: fputc('A', out); break; /* Á (U+00C1) */
                case 0x89: fputc('E', out); break; /* É (U+00C9) */
                case 0x8D: fputc('I', out); break; /* Í (U+00CD) */
                case 0x93: fputc('O', out); break; /* Ó (U+00D3) */
                case 0x9A: fputc('U', out); break; /* Ú (U+00DA) */
                case 0x9C:                         /* Ü (U+00DC) -> ü (U+00FC: C3 BC) */
                    fputc(0xC3, out);
                    fputc(0xBC, out);
                    break;
                default:
                    fputc(b1, out);
                    fputc(c2, out);
                    break;
            }
        }
        /* secuencias UTF-8 2 bits que empiezan con 0xC2 */
        else if (b1 == 0xC2) {
            int c2 = fgetc(in);
            if (c2 == EOF) return -1;
            switch ((unsigned char)c2) {
                case 0xAB: fputc('"', out); break; /* « (U+00AB) */
                case 0xBB: fputc('"', out); break; /* » (U+00BB) */
                default:
                    fputc(b1, out);
                    fputc(c2, out);
                    break;
            }
        }
        /* secuencias de UTF-8 de 3 bits que empiezan con 0xE2 0x80 */
        else if (b1 == 0xE2) {
            int c2 = fgetc(in);
            if (c2 == EOF) return -1;
            if ((unsigned char)c2 == 0x80) {
                int c3 = fgetc(in);
                if (c3 == EOF) return -1;
                switch ((unsigned char)c3) {
                    case 0x94: fputc('-', out); break; /* — Guion largo (U+2014) */
                    case 0x9C: fputc('"', out); break; /* “ abre comillas dobles impresas (U+201C) */
                    case 0x9D: fputc('"', out); break; /* ” comillas impresas cerradas (U+201D) */
                    case 0xA6: fputs("...", out); break; /* … trespuntos (U+2026) */
                    default:
                        fputc(b1, out);
                        fputc(c2, out);
                        fputc(c3, out);
                        break;
                }
            } else {
                fputc(b1, out);
                fputc(c2, out);
            }
        }
        /* Todos los demás bytes pasan sin filtrarse */
        else {
            if (fputc(b1, out) == EOF) return -1;
        }
    }

    return ferror(in) || ferror(out) ? -1 : 0;
}

/*
 * Procesa strems de un byte ISO-8859-1 (y extensiones de puntuación de Windows-1252).
 */
static int process_iso8859(FILE *in, FILE *out) {
    int c;

    while ((c = fgetc(in)) != EOF) {
        unsigned char b = (unsigned char)c;

        switch (b) {
            /* ISO-8859-1 mayusculas acentuadas y dieresis */
            case 0xC1: fputc('A', out); break; /* Á */
            case 0xC9: fputc('E', out); break; /* É */
            case 0xCD: fputc('I', out); break; /* Í */
            case 0xD3: fputc('O', out); break; /* Ó */
            case 0xDA: fputc('U', out); break; /* Ú */
            case 0xDC: fputc(0xFC, out); break; /* Ü -> ü (0xFC in ISO-8859-1) */

            /* ISO-8859-1 comillas francesas */
            case 0xAB: fputc('"', out); break; /* « */
            case 0xBB: fputc('"', out); break; /* » */

            /* Windows-1252 / Latin-1 extend Punctuaciones (0x80-0x9F range) */
            case 0x85: fputs("...", out); break; /* … Ellipsis */
            case 0x93: fputc('"', out); break;   /* “ Left double quote */
            case 0x94: fputc('"', out); break;   /* ” Right double quote */
            case 0x97: fputc('-', out); break;   /* — Em-dash */

            default:
                if (fputc(b, out) == EOF) return -1;
                break;
        }
    }

    return ferror(in) || ferror(out) ? -1 : 0;
}

/*
 * Valida fichero temporal (revisa errores, tamano > 0 si la salida > 0)
 * y escribe contebnido verificado en salida stdout.
 */
static int verify_and_output(FILE *tmp, long orig_size) {
    long tmp_size = 0;
    char buffer[4096];
    size_t n;

    if (fflush(tmp) != 0 || ferror(tmp)) {
        fprintf(stderr, "[ERROR] Fichero temporal reporta un error E/S.\n");
        return -1;
    }

    if (fseek(tmp, 0L, SEEK_END) != 0) {
        perror("[ERROR] Fallo al buscar fichero temporal");
        return -1;
    }

    tmp_size = ftell(tmp);
    if (tmp_size < 0) {
        perror("[ERROR] Fallo al determinal tamaño de fichero temporal");
        return -1;
    }

    /* Revisa que salida no sea 0 bytes cuando entrada no esta vacia */
    if (orig_size > 0 && tmp_size == 0) {
        fprintf(stderr, "[ERROR] Falla verificación: fichero de salida tiene 0 bytes.\n");
        return -1;
    }

    if (fseek(tmp, 0L, SEEK_SET) != 0) {
        perror("[ERROR] Falla rebobinar fichero temporal");
        return -1;
    }

    /* Salida de Stream verificado a stdout */
    while ((n = fread(buffer, 1, sizeof(buffer), tmp)) > 0) {
        if (fwrite(buffer, 1, n, stdout) != n) {
            perror("[ERROR] Fallo al escribir datos verificados a stdout");
            return -1;
        }
    }

    if (ferror(tmp)) {
        fprintf(stderr, "[ERROR] Fallo de lectura en pipe a fichero temporal.\n");
        return -1;
    }

    fflush(stdout);
    return 0;
}
