#include "gestionPuerto.h"

void EnviarCaracter(interface_t iface, char dato,
unsigned char *mac_src, unsigned char *mac_dst,
unsigned char *type)
{
    unsigned char *payload = (unsigned char *)(malloc(sizeof(char)));
    payload[0] = dato;

    unsigned char *p;

    p = BuildFrame(mac_src, mac_dst, type, payload);
    SendFrame(&iface, p, sizeof(payload));

    free(payload);
}

char RecibirCaracter(interface_t iface){
    apacket_t d = ReceiveFrame(&iface);
    const unsigned char * p = d.packet;
    if(p != NULL){
        const unsigned char * payload = p + 14;
        return payload[0];
    }
    return 0;
}