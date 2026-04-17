#ifndef _PROTOCOLO_DESCUBRIMIENTO_H
#define _PROTOCOLO_DESCUBRIMIENTO_H

#include "linkLayer.h"

#define FRC_OP_NORMAL   0x00
#define FRC_OP_DISCOVER 0x01
#define FRC_OP_REPLY    0x02

#define FRC_MIN_GRUPO 1
#define FRC_MAX_GRUPO 10

#define FRC_DATA_INTERACTIVE 'C'
#define FRC_DATA_FILE        'F'
#define FRC_DATA_FILE_END    'E'

#define FRC_MAX_FILE_CHUNK 254

unsigned char FRCTypeFromGroup(unsigned char grupo);

int FRCDescubrirEsclavo(interface_t iface, unsigned char grupo, unsigned char *mac_esclavo);
int FRCEsperarMaestroYResponder(interface_t iface, unsigned char grupo, unsigned char *mac_maestro);

int FRCEnviarTramaDatos(interface_t iface, const unsigned char *mac_dst, unsigned char grupo,
                        unsigned char subtipo, const unsigned char *datos, int longitud);

int FRCRecibirTramaDatos(interface_t iface, unsigned char grupo, const unsigned char *mac_src_esperada,
                         unsigned char *mac_src_real, unsigned char *subtipo,
                         unsigned char *buffer_datos, int *longitud);

#endif
