#include "protocoloParoEspera.h"
#include "protocoloDescubrimiento.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ==========================
// Variables globales para errores manuales
// ==========================
int errores_manual = 0; // Contador de errores pendientes F4

// ==========================
// Funciones auxiliares
// ==========================
unsigned char calcularBCE(const unsigned char *datos, int longitud) {
    if (longitud <= 0 || datos == NULL) return 1;
    unsigned char bce = datos[0];
    for (int i = 1; i < longitud; i++) {
        bce ^= datos[i];
    }
    if (bce == 0 || bce == 255) bce = 1;
    return bce;
}

// ==========================
// Construcción de tramas
// ==========================
unsigned char* construirTramaControl(const unsigned char *mac_origen,
                                     const unsigned char *mac_destino,
                                     unsigned char control,
                                     char num_trama) {
    unsigned char payload[3] = {'R', control, (unsigned char)num_trama};
    unsigned char *frame = BuildFrame((unsigned char *)mac_origen,
                                      (unsigned char *)mac_destino,
                                      NULL, payload);
    return frame;
}

unsigned char* construirTramaDatos(const unsigned char *mac_origen,
                                   const unsigned char *mac_destino,
                                   char num_trama,
                                   const unsigned char *datos,
                                   int longitud) {
    if (longitud < 1 || longitud > 254) return NULL;

    // Introducir error manual si hay errores pendientes
    unsigned char *datos_mod = (unsigned char *)malloc(longitud);
    memcpy(datos_mod, datos, longitud);

    if (errores_manual > 0) {
        datos_mod[0] = 135; // Sustituir primer carácter por 'ç'
        errores_manual--;
        printf("INTRODUCIENDO ERROR\n");
    }

    unsigned char bce = calcularBCE(datos_mod, longitud);
    int payload_len = 4 + longitud + 1; // Dir, Ctrl, NumTrama, Long, Datos, BCE
    unsigned char *payload = (unsigned char *)malloc(payload_len);
    payload[0] = 'R';
    payload[1] = 0x02; // STX
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
// Envío y recepción de tramas
// ==========================
int enviarTramaDatos(interface_t iface, const unsigned char *mac_dst,
                     char *num_trama, const unsigned char *datos, int longitud) {
    unsigned char *frame = construirTramaDatos(iface.MACaddr, mac_dst, *num_trama, datos, longitud);
    if (!frame) return 1;

    SendFrame(&iface, frame, longitud + 5);
    free(frame);

    while (1) {
        apacket_t resp = ReceiveFrame(&iface);
        if (resp.packet == NULL) continue;

        if (resp.packet[0] == 'R') {
            if (resp.packet[1] == 0x06 && resp.packet[2] == *num_trama) { // ACK
                *num_trama = (*num_trama == '0') ? '1' : '0';
                return 0;
            }
            if (resp.packet[1] == 0x21 && resp.packet[2] == *num_trama) { // NACK
                frame = construirTramaDatos(iface.MACaddr, mac_dst, *num_trama, datos, longitud);
                SendFrame(&iface, frame, longitud + 5);
                free(frame);
            }
        }
    }
}

int recibirTramaDatos(interface_t iface, const unsigned char *mac_origen,
                      char *num_trama, unsigned char *buffer_datos, int *longitud) {
    apacket_t trama = ReceiveFrame(&iface);
    if (trama.packet == NULL || trama.header.caplen < 14) return 1;

    if (trama.packet[0] != 'R' || trama.packet[1] != 0x02) return 2;
    if (trama.packet[2] != *num_trama) return 3;

    int len = trama.packet[3];
    unsigned char bce_calculado = calcularBCE(trama.packet + 4, len);
    unsigned char bce_recibido = trama.packet[4 + len];

    if (bce_calculado != bce_recibido) {
        unsigned char *nack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, 0x21, *num_trama);
        SendFrame(&iface, nack, 3);
        free(nack);
        return 4;
    }

    if (buffer_datos != NULL) memcpy(buffer_datos, trama.packet + 4, len);
    if (longitud != NULL) *longitud = len;

    unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, 0x06, *num_trama);
    SendFrame(&iface, ack, 3);
    free(ack);

    *num_trama = (*num_trama == '0') ? '1' : '0';
    return 0;
}

// ==========================
// Funciones para enviar archivo completo
// ==========================
int enviarArchivo(interface_t iface, const unsigned char *mac_dst, const char *nombre_archivo) {
    FILE *f = fopen(nombre_archivo, "rb");
    if (!f) {
        printf("No se pudo abrir archivo %s\n", nombre_archivo);
        return 1;
    }

    char num_trama = '0';
    unsigned char buffer[254];
    int leidos;

    // Leer y enviar tramas de 254 bytes máximo
    while ((leidos = fread(buffer, 1, 254, f)) > 0) {
        enviarTramaDatos(iface, mac_dst, &num_trama, buffer, leidos);
    }

    fclose(f);
    return 0;
}

int recibirArchivo(interface_t iface, const unsigned char *mac_origen, const char *nombre_destino) {
    FILE *f = fopen(nombre_destino, "wb");
    if (!f) {
        printf("No se pudo crear archivo %s\n", nombre_destino);
        return 1;
    }

    char num_trama = '0';
    unsigned char buffer[254];
    int longitud;

    while (1) {
        int res = recibirTramaDatos(iface, mac_origen, &num_trama, buffer, &longitud);
        if (res != 0) break; // Por simplicidad, aquí se detiene cuando no hay más tramas
        fwrite(buffer, 1, longitud, f);
    }

    fclose(f);
    return 0;
}

// ==========================
// Función para introducir error manual (F4)
// ==========================
void pulsarF4() {
    errores_manual++;
}