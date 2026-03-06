//============================================================================
// ----------- PRACTICAS DE FUNDAMENTOS DE REDES DE COMUNICACIONES -----------
// ---------------------------- CURSO 2025/26 --------------------------------
// ----------------------------- SESION2.CPP ---------------------------------
//============================================================================
#include <stdio.h>
#include <stdio_ext.h>
#include <iostream>
#include <string>
#include <vector>
#include "linkLayer.h"
#include "gestionPuerto.h"

using namespace std;

int main()
{
  char car;
  unsigned char mac_src[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  unsigned char mac_dst[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  unsigned char type[2] = {0x30, 0x00};

  interface_t iface;
  pcap_if_t *avail_ifaces = NULL;

  printf("\n----------------------------\n");
  printf("------ SESION 2 - FRC ------\n");
  printf("----------------------------\n");

  avail_ifaces = GetAvailAdapters();

  printf("Interfaces disponibles:\n");

  // Creo un vector donde almacenar los nombres de las interfaces
  std::vector<std::string> names;
  // También creo una variable aux que apunte a la interfaz de la lista de interfaces y su indice para numerarla
  pcap_if_t *cur = avail_ifaces;
  int index = 0;

  // Recorro la lista de interfaces numerandolas y guardandolas para luego seleccionarla
  while (cur != NULL)
  {
    printf("[%d] %s\n", index, cur->name);
    names.push_back(std::string(cur->name));
    cur = cur->next;
    index++;
  }

  // Seleccion de la interfaz con algunas excepciones para que salga si se selecciona uno invalido o si no existen interfaces
  int choice = -1;
  if (names.size() == 0)
  {
    printf("No hay interfaces disponibles.\n");
    return 1;
  }

  printf("Seleccione interfaz: ");
  if (scanf("%d", &choice) != 1)
  {
    printf("Entrada no valida.\n");
    return 1;
  }
  if (choice < 0 || choice >= (int)names.size())
  {
    printf("Indice fuera de rango.\n");
    return 1;
  }

  printf("Interfaz Elegida: %s\n", names[choice].c_str());

  if (setDeviceName(&iface, (char *)names[choice].c_str()) != 0)
  {
    printf("Error al setear el nombre del dispositivo.\n");
    return 1;
  }

  if (GetMACAdapter(&iface) != 0)
  {
    printf("Error al obtener la MAC del adaptador.\n");
    return 1;
  }
  // Se muestra la MAC de la interfaz elegida
  unsigned char *mac = iface.MACaddr;
  printf("La MAC es: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Se inserta el caracter
  printf("Pulse los caracteres a enviar:\n");
  if (kbhit)
  {
    car = getch();
  }
  

  // Elegimos el puerto
  setDeviceName(&iface, "lo");
  GetMACAdapter(&iface);
  

  // Abrimos el puerto
  int Puerto = OpenAdapter(&iface);

  if (Puerto != 0)
  {
    printf("Error al abrir el puerto\n");
    getch();
    return (1);
  }
  else
    printf("Puerto abierto correctamente\n");

  __fpurge(stdin);
  // Enviamos un carácter
  while (car != 27)
  {

    if (kbhit)
    {
      car = getch();
      if (car != 27)
      {
        EnviarCaracter(iface, car, mac_src, mac_dst, type);
        printf("Enviado el carácter: %c\n", car);
      } else
      {
        printf("\n");
      }
      
      
    }
    
    
  }

  // Cerramos el puerto:
  CloseAdapter(&iface);
  printf("Puerto cerrado\n");

  return 0;
}
