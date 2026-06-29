#/*******************************************************************************
# Integrantes:
# - Sebastián Caicedo Sánchez
# - David Martínez Bergantiño
# Grupo: 9
#*******************************************************************************/

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
// group used to build the 2-byte protocol/type field for ethernet
static unsigned char module_group = 1;

static int leerF4DuranteProtocolo()
{
    if (!kbhit())
        return 0;

    int c1 = getch();

    if (c1 != 27)
        return 0;

    if (!kbhit())
        return 0;

    int c2 = getch();

    if (c2 != 'O')
        return 0;

    if (!kbhit())
        return 0;

    int c3 = getch();

    return c3 == 'S'; // F4
}

// ==========================
// BCE
// ==========================
unsigned char calcularBCE(const unsigned char *datos, int longitud)
{
    if (longitud <= 0 || datos == NULL)
        return 1;

    unsigned char bce = datos[0];
    for (int i = 1; i < longitud; i++)
        bce ^= datos[i];

    if (bce == 0 || bce == 255)
        bce = 1;
    return bce;
}

// ==========================
// Construcción de tramas
// ==========================
unsigned char *construirTramaControl(const unsigned char *mac_origen,
                                     const unsigned char *mac_destino,
                                     unsigned char control,
                                     char num_trama)
{
    unsigned char payload[3] = {'R', control, (unsigned char)num_trama};

    unsigned char type[2] = {FRCTypeFromGroup(module_group), FRC_OP_NORMAL};
    return BuildFrame((unsigned char *)mac_origen,
                      (unsigned char *)mac_destino,
                      type, payload);
}

unsigned char *construirTramaDatos(const unsigned char *mac_origen,
                                   const unsigned char *mac_destino,
                                   char num_trama,
                                   const unsigned char *datos,
                                   int longitud,
                                   int introducir_error)
{
    if (longitud < 1 || longitud > 254)
        return NULL;

    unsigned char bce = calcularBCE(datos, longitud);

    int payload_len = 4 + longitud + 1;
    unsigned char *payload = (unsigned char *)malloc(payload_len + 1);

    payload[0] = 'R';
    payload[1] = CTRL_STX;
    payload[2] = num_trama;
    payload[3] = (unsigned char)longitud;

    memcpy(payload + 4, datos, longitud);
    payload[4 + longitud] = bce;
    payload[payload_len] = '\0';

    if (introducir_error)
    {
        payload[4] = 135;
    }

    unsigned char type[2] = {FRCTypeFromGroup(module_group), FRC_OP_NORMAL};

    unsigned char *frame = BuildFrame((unsigned char *)mac_origen,
                                      (unsigned char *)mac_destino,
                                      type,
                                      payload);

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
    if (!frame)
        return 1;

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
    int introducir_error = 0;

    if (errores_manual > 0)
    {
        introducir_error = 1;
        errores_manual--;
    }

    unsigned char *frame = construirTramaDatos(iface.MACaddr, mac_dst,
                                               *num_trama, datos, longitud,
                                               introducir_error);
    if (!frame)
        return 1;

    SendFrame(&iface, frame, longitud + 5);
    free(frame);

    while (1)
    {
        apacket_t resp = ReceiveFrame(&iface);
        if (resp.packet == NULL)
            continue;

        if (resp.header.caplen < 14 + 3)
            continue;

        const unsigned char *payload = resp.packet + 14;

        if (payload[0] != 'R')
            continue;

        if (payload[1] == CTRL_ACK && payload[2] == *num_trama)
        {
            printf("R   R   ACK   %c\n", *num_trama);
            *num_trama = (*num_trama == '0') ? '1' : '0';
            return 0;
        }

        if (payload[1] == CTRL_NACK && payload[2] == *num_trama)
        {
            printf("R   R   NACK  %c\n", *num_trama);

            frame = construirTramaDatos(iface.MACaddr, mac_dst,
                                        *num_trama, datos, longitud,
                                        0); // retransmisión SIN error

            if (!frame)
                return 1;

            printf("E   R   STX   %c   %d\n", *num_trama, (int)calcularBCE(datos, longitud));

            SendFrame(&iface, frame, longitud + 5);
            free(frame);
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

    if (trama.packet == NULL)
        return 1;

    // Ensure we have at least Ethernet header + minimum control header
    if (trama.header.caplen < 14 + 5)
        return 1;

    const unsigned char *payload = trama.packet + 14; // skip Ethernet header

    if (payload[0] != 'R' || payload[1] != CTRL_STX)
        return 2;

    if (payload[2] != *num_trama)
        return 3;

    int len = payload[3];
    if (len <= 0 || len > 254)
        return 5;

    if (trama.header.caplen < 14 + 5 + len)
        return 6;

    unsigned char bce_calculado = calcularBCE(payload + 4, len);
    unsigned char bce_recibido = payload[4 + len];

    if (bce_calculado != bce_recibido)
    {
        unsigned char *nack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_NACK, *num_trama);
        SendFrame(&iface, nack, 3);
        free(nack);
        return 4;
    }

    memcpy(buffer_datos, payload + 4, len);
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
    while (1)
    {
        apacket_t resp = ReceiveFrame(&iface);
        if (resp.packet == NULL)
            continue;

        if (resp.header.caplen < 14 + 3)
            continue;

        const unsigned char *payload = resp.packet + 14;
        if (payload[0] == 'R' &&
            payload[1] == CTRL_ACK &&
            payload[2] == num_trama)
        {
            return 0;
        }
    }
}

// ==========================
// ENVÍO ARCHIVO
// ==========================
int enviarArchivo(interface_t iface, const unsigned char *mac_dst, unsigned char grupo, const char *nombre_archivo)
{
    module_group = grupo;
    FILE *f = fopen(nombre_archivo, "rb");
    if (!f)
    {
        printf("No se pudo abrir archivo %s\n", nombre_archivo);
        return 1;
    }

    char num_trama = '0';
    unsigned char buffer[254];
    int leidos;

    printf("PROTOCOLO PARO Y ESPERA (MAESTRO)\n");

    // ENQ
    printf("E   R   ENQ   %c\n", num_trama);
    enviarTramaControl(iface, mac_dst, CTRL_ENQ, num_trama);

    esperarACK(iface, mac_dst, num_trama);
    printf("R   R   ACK   %c\n", num_trama);

    printf("\n");

    // DATOS
    while ((leidos = fread(buffer, 1, 254, f)) > 0)
    {

        if (leerF4DuranteProtocolo())
        {
            pulsarF4();
        }

        if (errores_manual > 0) {
            printf("INTRODUCIENDO ERROR\n");
        }
        unsigned char bce = calcularBCE(buffer, leidos);
        printf("E   R   STX   %c   %d\n", num_trama, (int)bce);
        enviarTramaDatos(iface, mac_dst, &num_trama, buffer, leidos);

    }
    printf("\n");

    // EOT
    num_trama = '0';
    printf("E   R   EOT   %c\n", num_trama);
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
int recibirArchivo(interface_t iface, const unsigned char *mac_origen, unsigned char grupo, const char *nombre_destino)
{
    module_group = grupo;
    FILE *f = fopen(nombre_destino, "wb");
    if (!f)
    {
        printf("No se pudo crear archivo %s\n", nombre_destino);
        return 1;
    }

    char num_trama = '0';
    unsigned char buffer[254];
    int longitud;

    printf("PROTOCOLO PARO Y ESPERA (ESCLAVO)\n");

    while (1)
    {
        apacket_t trama = ReceiveFrame(&iface);
        if (trama.packet == NULL)
            continue;
        if (trama.header.caplen < 14 + 1)
            continue;

        const unsigned char *payload = trama.packet + 14;

        // ENQ
        if (payload[0] == 'R' && payload[1] == CTRL_ENQ)
        {

            printf("R   R   ENQ   %c\n", payload[2]);

            unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_ACK, payload[2]);
            SendFrame(&iface, ack, 3);
            free(ack);

            printf("E   R   ACK   %c\n", payload[2]);
            printf("\n");
            continue;
        }

        // EOT
        if (payload[0] == 'R' && payload[1] == CTRL_EOT)
        {
            printf("\n");
            printf("R   R   EOT   %c\n", payload[2]);

            unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_ACK, payload[2]);
            SendFrame(&iface, ack, 3);
            free(ack);

            printf("E   R   ACK   %c\n", payload[2]);
            break;
        }

        // DATOS
        if (payload[0] == 'R' && payload[1] == CTRL_STX)
        {
            int len = payload[3];
            unsigned char bce_recibido = 0;
            unsigned char bce_calculado = 0;

            // Make sure the packet is large enough before indexing
            if (len > 0 && trama.header.caplen >= 14 + 5 + len)
            {
                bce_recibido = payload[4 + len];
                bce_calculado = calcularBCE(payload + 4, len);
                // Print both received and calculated BCE for debugging
                printf("R   R   STX   %c  %d %d\n", payload[2], (int)bce_recibido, (int)bce_calculado);
            }
            else
            {
                // Fallback: packet too small or invalid length
                printf("R   R   STX   %c   len=%d (incomplete)\n", payload[2], len);
            }

            int res = procesarTramaDatos(&trama, iface, mac_origen, &num_trama, buffer, &longitud);

            if (res == 0)
            {
                fwrite(buffer, 1, longitud, f);
                printf("E   R   ACK   %c\n", (num_trama == '0') ? '1' : '0');
            }
            else if (res == 4)
            {
                printf("E   R   NACK  %c\n", num_trama);
            }
        }
    }

    fclose(f);
    printf("Fin de Seleccion por parte del Esclavo\n");
    return 0;
}

// ==========================
void pulsarF4()
{
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
    if (trama->packet == NULL)
        return 1;

    if (trama->header.caplen < 14 + 5)
        return 1;

    const unsigned char *payload = trama->packet + 14;

    if (payload[0] != 'R' || payload[1] != CTRL_STX)
        return 2;

    if (payload[2] != *num_trama)
        return 3;

    int len = payload[3];
    if (len <= 0 || len > 254)
        return 5;

    if (trama->header.caplen < 14 + 5 + len)
        return 6;

    unsigned char bce_calculado = calcularBCE(payload + 4, len);
    unsigned char bce_recibido = payload[4 + len];

    if (bce_calculado != bce_recibido)
    {
        unsigned char *nack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_NACK, *num_trama);
        SendFrame(&iface, nack, 3);
        free(nack);
        return 4;
    }

    memcpy(buffer_datos, payload + 4, len);
    *longitud = len;

    unsigned char *ack = construirTramaControl(iface.MACaddr, (unsigned char *)mac_origen, CTRL_ACK, *num_trama);
    SendFrame(&iface, ack, 3);
    free(ack);

    *num_trama = (*num_trama == '0') ? '1' : '0';

    return 0;
}
