#include "protocoloParoEspera.h"
#include "protocoloDescubrimiento.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CTRL_ENQ 0x05
#define CTRL_EOT 0x04
#define CTRL_ACK 0x06
#define CTRL_NACK 0x21
#define CTRL_STX 0x02

// ==========================
// Variables globales
// ==========================
int errores_manual = 0;

// ==========================
// BCE
// ==========================
unsigned char calcularBCE(const unsigned char *datos, int longitud) {
    if (longitud <= 0 || datos == NULL) return 1;

    unsigned char bce = datos[0];
    for (int i = 1; i < longitud; i++)
        bce ^= datos[i];

    if (bce == 0 || bce == 255) bce = 1;
    return bce;
}

// ==========================
// Construcción de tramas
// ==========================
unsigned char* construirTramaControl(const unsigned char *mac_origen,
                                     const unsigned char *mac_destino,
                                     unsigned char control,
                                     char num_trama)
{
    unsigned char payload[3] = {'R', control, (unsigned char)num_trama};

    return BuildFrame((unsigned char *)mac_origen,
                      (unsigned char *)mac_destino,
                      NULL, payload);
}

unsigned char* construirTramaDatos(const unsigned char *mac_origen,
                                   const unsigned char *mac_destino,
                                   char num_trama,
                                   const unsigned char *datos,
                                   int longitud)
{
    if (longitud < 1 || longitud > 254) return NULL;

    unsigned char *datos_mod = (unsigned char *)malloc(longitud);
    memcpy(datos_mod, datos, longitud);

    // Error manual
    if (errores_manual > 0) {
        datos_mod[0] = 135;
        errores_manual--;
        printf("INTRODUCIENDO ERROR\n");
    }

    unsigned char bce = calcularBCE(datos_mod, longitud);

    int payload_len = 4 + longitud + 1;
    unsigned char *payload = (unsigned char *)malloc(payload_len);

    payload[0] = 'R';
    payload[1] = CTRL_STX;
    payload[2] = num_trama;
    payload[3] = (unsigned char)longitud;

    memcpy(payload + 4, datos_mod, longitud);
    payload[4 + longitud] = bce;

    free(datos_mod);

    unsigned char *frame = BuildFrame((unsigned char *)mac_origen,
                                      (unsigned char *)mac_destino,
                                      NULL, payload);

    free(payload);
    return frame;
}

// ==========================
// ENVÍO CONTROL
// ==========================
int enviarTramaControl(interface_t iface, const unsigned char *mac_dst,
                       unsigned char control, char num_trama)
{
    unsigned char *frame = construirTramaControl(iface.MACaddr, mac_dst, control, num_trama);
    if (!frame) return 1;

    SendFrame(&iface, frame, 3);
    free(frame);
    return 0;
}

// ==========================
// ENVÍO DATOS
// ==========================
int enviarTramaDatos(interface_t iface, const unsigned char *mac_dst,
                     char *num_trama, const unsigned char *datos, int longitud)
{
    unsigned char *frame = construirTramaDatos(iface.MACaddr, mac_dst, *num_trama, datos, longitud);
    if (!frame) return 1;

    SendFrame(&iface, frame, longitud + 5);
    free(frame);

    while (1) {
        apacket_t resp = ReceiveFrame(&iface);
        if (resp.packet == NULL) continue;

        if (resp.packet[0] == 'R') {

            // ACK
            if (resp.packet[1] == CTRL_ACK && resp.packet[2] == *num_trama) {
                *num_trama = (*num_trama == '0') ? '1' : '0';
                return 0;
            }

            // NACK
            if (resp.packet[1] == CTRL_NACK && resp.packet[2] == *num_trama) {
                frame = construirTramaDatos(iface.MACaddr, mac_dst, *num_trama, datos, longitud);
                SendFrame(&iface, frame, longitud + 5);
                free(frame);
            }
        }
    }
}

// ==========================
// RECEPCIÓN DATOS (genérica)
// ==========================
int recibirTramaDatos(interface_t iface, const unsigned char *mac_origen,
                      char *num_trama, unsigned char *buffer_datos, int *longitud)
{
    apacket_t trama = ReceiveFrame(&iface);

    if (trama.packet == NULL || trama.header.caplen < 5)
        return 1;

    if (trama.packet[0] != 'R' || trama.packet[1] != CTRL_STX)
        return 2;

    if (trama.packet[2] != *num_trama)
        return 3;

    int len = trama.packet[3];
    if (len <= 0 || len > 254) return 5;

    if (trama.header.caplen < 5 + len) return 6;

    unsigned char bce_calculado = calcularBCE(trama.packet + 4, len);
    unsigned char bce_recibido = trama.packet[4 + len];

    if (bce_calculado != bce_recibido) {
        unsigned char *nack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_NACK, *num_trama);
        SendFrame(&iface, nack, 3);
        free(nack);
        return 4;
    }

    memcpy(buffer_datos, trama.packet + 4, len);
    *longitud = len;

    unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_ACK, *num_trama);
    SendFrame(&iface, ack, 3);
    free(ack);

    *num_trama = (*num_trama == '0') ? '1' : '0';
    return 0;
}

// ==========================
// ESPERAR ACK
// ==========================
int esperarACK(interface_t iface, const unsigned char *mac_origen, char num_trama)
{
    while (1) {
        apacket_t resp = ReceiveFrame(&iface);

        if (resp.packet == NULL) continue;

        if (resp.packet[0] == 'R' &&
            resp.packet[1] == CTRL_ACK &&
            resp.packet[2] == num_trama)
        {
            return 0;
        }
    }
}

// ==========================
// ENVÍO ARCHIVO
// ==========================
int enviarArchivo(interface_t iface, const unsigned char *mac_dst, const char *nombre_archivo)
{
    FILE *f = fopen(nombre_archivo, "rb");
    if (!f) {
        printf("No se pudo abrir archivo %s\n", nombre_archivo);
        return 1;
    }

    char num_trama = '0';
    unsigned char buffer[254];
    int leidos;

    printf("PROTOCOLO PARO Y ESPERA (MAESTRO)\n");

    // ENQ
    printf("E   E   ENQ   %c\n", num_trama);
    enviarTramaControl(iface, mac_dst, CTRL_ENQ, num_trama);

    esperarACK(iface, mac_dst, num_trama);
    printf("R   R   ACK   %c\n", num_trama);

    // DATOS
    while ((leidos = fread(buffer, 1, 254, f)) > 0) {

        printf("E   E   STX   %c   %d\n", num_trama, leidos);

        enviarTramaDatos(iface, mac_dst, &num_trama, buffer, leidos);

        printf("R   R   ACK   %c\n", (num_trama == '0') ? '1' : '0');
    }

    // EOT
    printf("E   E   EOT   %c\n", num_trama);
    enviarTramaControl(iface, mac_dst, CTRL_EOT, num_trama);

    esperarACK(iface, mac_dst, num_trama);
    printf("R   R   ACK   %c\n", num_trama);

    fclose(f);
    printf("Fin de Seleccion por parte del Maestro\n");

    return 0;
}

// ==========================
// RECEPCIÓN ARCHIVO
// ==========================
int recibirArchivo(interface_t iface, const unsigned char *mac_origen, const char *nombre_destino)
{
    FILE *f = fopen(nombre_destino, "wb");
    if (!f) {
        printf("No se pudo crear archivo %s\n", nombre_destino);
        return 1;
    }

    char num_trama = '0';
    unsigned char buffer[254];
    int longitud;

    printf("PROTOCOLO PARO Y ESPERA (ESCLAVO)\n");

    while (1) {
        apacket_t trama = ReceiveFrame(&iface);
        if (trama.packet == NULL) continue;

        if (trama.header.caplen < 5) continue;

        // ENQ
        if (trama.packet[0] == 'R' && trama.packet[1] == CTRL_ENQ) {

            printf("R   R   ENQ   %c\n", trama.packet[2]);

            unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_ACK, trama.packet[2]);
            SendFrame(&iface, ack, 3);
            free(ack);

            printf("E   E   ACK   %c\n", trama.packet[2]);
            continue;
        }

        // EOT
        if (trama.packet[0] == 'R' && trama.packet[1] == CTRL_EOT) {

            printf("R   R   EOT   %c\n", trama.packet[2]);

            unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_ACK, trama.packet[2]);
            SendFrame(&iface, ack, 3);
            free(ack);

            printf("E   E   ACK   %c\n", trama.packet[2]);
            break;
        }

        // DATOS
        if (trama.packet[0] == 'R' && trama.packet[1] == CTRL_STX) {

            printf("R   R   STX   %c   %d\n", trama.packet[2], trama.packet[3]);

            int res = procesarTramaDatos(&trama, iface, mac_origen, &num_trama, buffer, &longitud);

            if (res == 0) {
                fwrite(buffer, 1, longitud, f);
                printf("E   E   ACK   %c\n", (num_trama == '0') ? '1' : '0');
            }
            else if (res == 4) {
                printf("E   E   NACK  %c\n", num_trama);
            }
        }
    }

    fclose(f);
    printf("Fin de Seleccion por parte del Esclavo\n");
    return 0;
}

// ==========================
void pulsarF4() {
    errores_manual++;
}

// ==========================
int procesarTramaDatos(apacket_t *trama,
                       interface_t iface,
                       const unsigned char *mac_origen,
                       char *num_trama,
                       unsigned char *buffer_datos,
                       int *longitud)
{
    if (trama->packet == NULL || trama->header.caplen < 5)
        return 1;

    if (trama->packet[0] != 'R' || trama->packet[1] != CTRL_STX)
        return 2;

    if (trama->packet[2] != *num_trama)
        return 3;

    int len = trama->packet[3];
    if (len <= 0 || len > 254) return 5;

    if (trama->header.caplen < 5 + len) return 6;

    unsigned char bce_calculado = calcularBCE(trama->packet + 4, len);
    unsigned char bce_recibido = trama->packet[4 + len];

    if (bce_calculado != bce_recibido) {
        unsigned char *nack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_NACK, *num_trama);
        SendFrame(&iface, nack, 3);
        free(nack);
        return 4;
    }

    memcpy(buffer_datos, trama->packet + 4, len);
    *longitud = len;

    unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_ACK, *num_trama);
    SendFrame(&iface, ack, 3);
    free(ack);

    *num_trama = (*num_trama == '0') ? '1' : '0';

    return 0;
}