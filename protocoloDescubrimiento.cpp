#/*******************************************************************************
# Integrantes:
# - Sebastián Caicedo Sánchez
# - David Martínez Bergantiño
# Grupo: 9
#*******************************************************************************/

#include "protocoloDescubrimiento.h"

#include <stdlib.h>
#include <string.h>

static int FRCGrupoValido(unsigned char grupo)
{
    return (grupo >= FRC_MIN_GRUPO && grupo <= FRC_MAX_GRUPO);
}

static int FRCEsBroadcast(const unsigned char *mac)
{
    static const unsigned char bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return (memcmp(mac, bcast, 6) == 0);
}

static int FRCTramaMinimaValida(const apacket_t *d)
{
    if (d == NULL || d->packet == NULL)
    {
        return 0;
    }

    return (d->header.caplen >= 14);
}

static int FRCTramaTipoOperacion(const apacket_t *d, unsigned char type0, unsigned char op)
{
    if (!FRCTramaMinimaValida(d))
    {
        return 0;
    }

    return (d->packet[12] == type0 && d->packet[13] == op);
}

unsigned char FRCTypeFromGroup(unsigned char grupo)
{
    if (!FRCGrupoValido(grupo))
    {
        return 0x00;
    }

    return (unsigned char)(0x30 + grupo);
}

int FRCDescubrirEsclavo(interface_t iface, unsigned char grupo, unsigned char *mac_esclavo)
{
    if (mac_esclavo == NULL)
    {
        return 1;
    }

    unsigned char type0 = FRCTypeFromGroup(grupo);
    if (type0 == 0x00)
    {
        return 2;
    }

    unsigned char type_discovery[2] = {type0, FRC_OP_DISCOVER};
    unsigned char mac_bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    unsigned char *trama = BuildHeader(iface.MACaddr, mac_bcast, type_discovery);
    if (trama == NULL)
    {
        return 3;
    }

    SendFrame(&iface, trama, 0);
    free(trama);

    while (1)
    {
        apacket_t d = ReceiveFrame(&iface);

        if (!FRCTramaTipoOperacion(&d, type0, FRC_OP_REPLY))
        {
            continue;
        }

        if (memcmp(d.packet, iface.MACaddr, 6) != 0)
        {
            continue;
        }

        memcpy(mac_esclavo, d.packet + 6, 6);
        return 0;
    }
}

int FRCEsperarMaestroYResponder(interface_t iface, unsigned char grupo, unsigned char *mac_maestro)
{
    if (mac_maestro == NULL)
    {
        return 1;
    }

    unsigned char type0 = FRCTypeFromGroup(grupo);
    if (type0 == 0x00)
    {
        return 2;
    }

    unsigned char type_reply[2] = {type0, FRC_OP_REPLY};

    while (1)
    {
        apacket_t d = ReceiveFrame(&iface);

        if (!FRCTramaTipoOperacion(&d, type0, FRC_OP_DISCOVER))
        {
            continue;
        }

        if (!FRCEsBroadcast(d.packet))
        {
            continue;
        }

        memcpy(mac_maestro, d.packet + 6, 6);

        unsigned char *reply = BuildHeader(iface.MACaddr, mac_maestro, type_reply);
        if (reply == NULL)
        {
            return 3;
        }

        SendFrame(&iface, reply, 0);
        free(reply);

        return 0;
    }
}

int FRCEnviarTramaDatos(interface_t iface, const unsigned char *mac_dst, unsigned char grupo,
                        unsigned char subtipo, const unsigned char *datos, int longitud)
{
    if (mac_dst == NULL)
    {
        return 1;
    }

    if (longitud < 0)
    {
        return 2;
    }

    unsigned char type0 = FRCTypeFromGroup(grupo);
    if (type0 == 0x00)
    {
        return 3;
    }

    unsigned char type[2] = {type0, FRC_OP_NORMAL};
    unsigned char *payload = (unsigned char *)malloc((size_t)longitud + 2);
    if (payload == NULL)
    {
        return 4;
    }

    payload[0] = subtipo;
    if (longitud > 0 && datos != NULL)
    {
        memcpy(payload + 1, datos, (size_t)longitud);
    }
    payload[longitud + 1] = '\0';

    unsigned char *frame = BuildFrame(iface.MACaddr, (unsigned char *)mac_dst, type, payload);
    if (frame == NULL)
    {
        free(payload);
        return 5;
    }

    SendFrame(&iface, frame, longitud + 1);

    free(payload);
    free(frame);
    return 0;
}

int FRCRecibirTramaDatos(interface_t iface, unsigned char grupo, const unsigned char *mac_src_esperada,
                         unsigned char *mac_src_real, unsigned char *subtipo,
                         unsigned char *buffer_datos, int *longitud)
{
    if (subtipo == NULL || longitud == NULL)
    {
        return 1;
    }

    unsigned char type0 = FRCTypeFromGroup(grupo);
    if (type0 == 0x00)
    {
        return 2;
    }

    apacket_t d = ReceiveFrame(&iface);
    if (!FRCTramaTipoOperacion(&d, type0, FRC_OP_NORMAL))
    {
        return 3;
    }

    if (mac_src_esperada != NULL && memcmp(d.packet + 6, mac_src_esperada, 6) != 0)
    {
        return 4;
    }

    if (memcmp(d.packet, iface.MACaddr, 6) != 0)
    {
        return 5;
    }

    if (mac_src_real != NULL)
    {
        memcpy(mac_src_real, d.packet + 6, 6);
    }

    if (d.header.caplen <= 14)
    {
        return 6;
    }

    *subtipo = d.packet[14];
    *longitud = d.header.caplen - 15;
    if (*longitud < 0)
    {
        *longitud = 0;
    }

    if (buffer_datos != NULL && *longitud > 0)
    {
        memcpy(buffer_datos, d.packet + 15, (size_t)(*longitud));
    }

    return 0;
}
