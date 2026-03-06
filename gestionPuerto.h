#include <stdio.h>
#include <iostream>
#include "linkLayer.h"
#ifndef _GESTIONPUERTO_H
#define _GESTIONPUERTO_H

using namespace std;

void EnviarCaracter(interface_t iface, char dato,
unsigned char *mac_sic, unsigned char *mac_dst,
unsigned char *type);

char RecibirCaracter(interface_t iface);

#endif