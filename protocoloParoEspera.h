#ifndef PROTOCOLO_PARO_ESPERA_H
#define PROTOCOLO_PARO_ESPERA_H
#include "linkLayer.h"

unsigned char calcularBCE(const unsigned char *datos, int longitud);

unsigned char* construirTramaControl(const unsigned char *mac_origen, const unsigned char *mac_destino,
                                     unsigned char control, char num_trama);

unsigned char* construirTramaDatos(const unsigned char *mac_origen, const unsigned char *mac_destino,
                                   char num_trama, const unsigned char *datos, int longitud);
                                   
int enviarTramaControl(interface_t iface, const unsigned char *mac_dst, unsigned char control, char num_trama);

int enviarTramaDatos(interface_t iface, const unsigned char *mac_dst, char *num_trama, const unsigned char *datos, int longitud);

int recibirTramaDatos(interface_t iface, const unsigned char *mac_origen, char *num_trama, unsigned char *buffer_datos, int *longitud);

int enviarArchivo(interface_t iface, const unsigned char *mac_dst, const char *nombre_archivo);

int recibirArchivo(interface_t iface, const unsigned char *mac_origen, const char *nombre_destino);

void pulsarF4();

int esperarACK(interface_t iface, const unsigned char *mac_origen, char num_trama);
#endif // PROTOCOLO_PARO_ESPERA_H
