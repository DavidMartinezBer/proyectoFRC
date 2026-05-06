//============================================================================
// ----------- PRACTICAS DE FUNDAMENTOS DE REDES DE COMUNICACIONES -----------
// ---------------------------- CURSO 2025/26 --------------------------------
// ----------------------------- PRACTICA 3 ----------------------------------
//============================================================================
#include <stdio.h>
#include <stdio_ext.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include "linkLayer.h"
#include "protocoloDescubrimiento.h"
#include "protocoloParoEspera.h"

using namespace std;

static void MostrarMAC(const unsigned char *mac)
{
  printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int LeerTeclaEspecial()
{
  int c1 = getch();
  if (c1 == 27)
  {
    if (!kbhit())
    {
      return 27;
    }

    int c2 = getch();
    if (c2 == 'O')
    {
      int c3 = getch();
      if (c3 == 'P')
        return 1;
      if (c3 == 'Q')
        return 2;
      if (c3 == 'R')
        return 3;
      if (c3 == 'S')
        return 4;
    }
    return 27;
  }
  return c1;
}

static void MostrarMenuMaestro()
{
  printf("\nSeleccione opcion:\n");
  printf("[F1] - Envio de caracteres interactivo\n");
  printf("[F2] - Envio de un fichero\n");
  printf("[F3] - Protocolo paro y espera - Seleccion\n");   // AÑADIDO
  printf("[F4] - Introduccion errores durante el protocolo\n"); // AÑADIDO
  printf("[ESC] - Salir\n");
}

static void MostrarMenuEsclavo()
{
  printf("\nSeleccione opcion:\n");
  printf("[F1] - Envio de caracteres interactivo\n");
  printf("[F3] - Protocolo paro y espera - Seleccion\n"); // AÑADIDO
  printf("[ESC] - Salir\n");
}

static void ImprimirCadenaRecibida(const unsigned char *buffer, int longitud)
{
  for (int i = 0; i < longitud; ++i)
  {
    putchar((char)buffer[i]);
  }
}

static void ProcesarRecepcionEsclavo(interface_t iface, unsigned char grupo, const unsigned char *mac_maestro, int modoInteractivo)
{
  unsigned char subtipo = 0;
  unsigned char origen[6] = {0};
  unsigned char datos[FRC_MAX_FILE_CHUNK + 8] = {0};
  int longitud = 0;
  static int numTramaFichero = 0;

  int estado = FRCRecibirTramaDatos(iface, grupo, mac_maestro, origen, &subtipo, datos, &longitud);
  if (estado != 0)
  {
    return;
  }
  

  /* comportamiento original por subtipo */
  if (subtipo == FRC_DATA_INTERACTIVE)
  {
    if (longitud > 0)
    {
      printf("\nRecibido el caracter: %c\n", datos[0]);
      if (modoInteractivo)
      {
        printf("[Modo interactivo esclavo activo]\n");
      }
      else
      {
        MostrarMenuEsclavo();
      }
    }
  }
  else if (subtipo == FRC_DATA_FILE)
  {
    ++numTramaFichero;
    printf("Recibido: ");
    ImprimirCadenaRecibida(datos, longitud);
    if (longitud == 0 || datos[longitud - 1] != '\n')
    {
      putchar('\n');
    }
    printf("Nº caracteres: %d\n", longitud);
  }
  else if (subtipo == FRC_DATA_FILE_END)
  {
    printf("\nFIN\n");
    printf("caracteres: 0\n");
    numTramaFichero = 0;
    MostrarMenuEsclavo();
  }
}
static void EjecutarModoInteractivoMaestro(interface_t iface, unsigned char grupo, const unsigned char *mac_esclavo)
{
  printf("\nModo interactivo maestro activo. ESC para volver al menu.\n");

  int salir = 0;
  while (!salir)
  {
    if (kbhit())
    {
      int tecla = LeerTeclaEspecial();
      if (tecla == 27)
      {
        salir = 1;
      }
      else if (tecla >= 32 && tecla <= 126)
      {
        unsigned char c = (unsigned char)tecla;
        if (FRCEnviarTramaDatos(iface, mac_esclavo, grupo, FRC_DATA_INTERACTIVE, &c, 1) == 0)
        {
          printf("Enviado el caracter: %c\n", c);
        }
      }
      else if (tecla == '\n' || tecla == '\r' || tecla == '\t')
      {
        unsigned char c = (unsigned char)tecla;
        if (FRCEnviarTramaDatos(iface, mac_esclavo, grupo, FRC_DATA_INTERACTIVE, &c, 1) == 0)
        {
          printf("Enviado caracter especial de control.\n");
        }
      }
    }

    unsigned char subtipo = 0;
    unsigned char origen[6] = {0};
    unsigned char datos[8] = {0};
    int longitud = 0;
    int estado = FRCRecibirTramaDatos(iface, grupo, mac_esclavo, origen, &subtipo, datos, &longitud);
    if (estado == 0 && subtipo == FRC_DATA_INTERACTIVE && longitud > 0)
    {
      printf("Recibido el caracter: %c\n", datos[0]);
    }
  }
}

static void EjecutarModoInteractivoEsclavo(interface_t iface, unsigned char grupo, const unsigned char *mac_maestro)
{
  printf("\nModo interactivo esclavo activo. ESC para volver al menu.\n");

  int salir = 0;
  while (!salir)
  {
    if (kbhit())
    {
      int tecla = LeerTeclaEspecial();
      if (tecla == 27)
      {
        salir = 1;
      }
      else if (tecla >= 32 && tecla <= 126)
      {
        unsigned char c = (unsigned char)tecla;
        if (FRCEnviarTramaDatos(iface, mac_maestro, grupo, FRC_DATA_INTERACTIVE, &c, 1) == 0)
        {
          printf("Enviado el caracter: %c\n", c);
        }
      }
      else if (tecla == '\n' || tecla == '\r' || tecla == '\t')
      {
        unsigned char c = (unsigned char)tecla;
        if (FRCEnviarTramaDatos(iface, mac_maestro, grupo, FRC_DATA_INTERACTIVE, &c, 1) == 0)
        {
          printf("Enviado caracter especial de control.\n");
        }
      }
    }

    ProcesarRecepcionEsclavo(iface, grupo, mac_maestro, 1);
  }
}

static int EnviarFichero(interface_t iface, unsigned char grupo, const unsigned char *mac_esclavo)
{
  ifstream fichero("envio.txt", ios::in | ios::binary);
  if (!fichero.is_open())
  {
    printf("No se ha podido abrir el fichero envio.txt\n");
    return 1;
  }

  printf("\nEnviando fichero envio.txt...\n");

  char buffer[FRC_MAX_FILE_CHUNK];
  int numTrama = 0;

  while (!fichero.eof())
  {
    fichero.read(buffer, FRC_MAX_FILE_CHUNK);
    streamsize leidos = fichero.gcount();
    if (leidos <= 0)
    {
      break;
    }

    ++numTrama;
    int estado = FRCEnviarTramaDatos(iface, mac_esclavo, grupo, FRC_DATA_FILE,
                                     (unsigned char *)buffer, (int)leidos);
    if (estado != 0)
    {
      printf("Error enviando la trama %d\n", numTrama);
      fichero.close();
      return 2;
    }

    printf("Trama %d enviada (%d caracteres)\n", numTrama, (int)leidos);
  }

  fichero.close();
  FRCEnviarTramaDatos(iface, mac_esclavo, grupo, FRC_DATA_FILE_END, NULL, 0);
  printf("Fichero enviado correctamente.\n");
  return 0;
}

int main()
{
  interface_t iface;
  unsigned char mac_remota[6] = {0};
  pcap_if_t *avail_ifaces = NULL;

  printf("\n----------------------------\n");
  printf("------ SESION 3 - FRC ------\n");
  printf("----------------------------\n");

  avail_ifaces = GetAvailAdapters();
  printf("Interfaces disponibles:\n");

  vector<string> names;
  pcap_if_t *cur = avail_ifaces;
  int index = 0;
  while (cur != NULL)
  {
    printf("[%d] %s\n", index, cur->name);
    names.push_back(string(cur->name));
    cur = cur->next;
    ++index;
  }

  if (names.empty())
  {
    printf("No hay interfaces disponibles.\n");
    return 1;
  }

  int choice = -1;
  printf("Seleccione interfaz: ");
  if (scanf("%d", &choice) != 1 || choice < 0 || choice >= (int)names.size())
  {
    printf("Seleccion de interfaz no valida.\n");
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

  printf("La MAC es: ");
  MostrarMAC(iface.MACaddr);
  printf("\n");

  int grupo = 0;
  printf("Introduzca el numero de grupo: ");
  if (scanf("%d", &grupo) != 1 || grupo < 1 || grupo > 10)
  {
    printf("Numero de grupo no valido.\n");
    return 1;
  }

  if (OpenAdapter(&iface) != 0)
  {
    printf("Error al abrir el puerto\n");
    return 1;
  }
  printf("Puerto abierto correctamente\n");

  int modo = 0;
  printf("Seleccione el modo de la estacion:\n");
  printf("[1] Modo Maestra\n");
  printf("[2] Modo Esclava\n");
  if (scanf("%d", &modo) != 1 || (modo != 1 && modo != 2))
  {
    printf("Modo no valido.\n");
    CloseAdapter(&iface);
    return 1;
  }

  if (modo == 1)
  {
    printf("Esperando que se una la estacion esclava\n");
    if (FRCDescubrirEsclavo(iface, (unsigned char)grupo, mac_remota) != 0)
    {
      printf("No se pudo descubrir la estacion esclava.\n");
      CloseAdapter(&iface);
      return 1;
    }
    printf("Estacion encontrada. La MAC es: ");
    MostrarMAC(mac_remota);
    printf("\n");
  }
  else
  {
    printf("Esperando que se una la estacion maestra\n");
    if (FRCEsperarMaestroYResponder(iface, (unsigned char)grupo, mac_remota) != 0)
    {
      printf("No se pudo completar el descubrimiento con la estacion maestra.\n");
      CloseAdapter(&iface);
      return 1;
    }
    printf("Estacion encontrada. La MAC es: ");
    MostrarMAC(mac_remota);
    printf("\n");
  }

  __fpurge(stdin);

  if (modo == 1)
  {
    int salir = 0;
    while (!salir)
    {
      MostrarMenuMaestro();
      int tecla = LeerTeclaEspecial();
      if (tecla == 27)
      {
        salir = 1;
      }
      else if (tecla == 1)
      {
        EjecutarModoInteractivoMaestro(iface, (unsigned char)grupo, mac_remota);
      }
      else if (tecla == 2)
      {
        EnviarFichero(iface, (unsigned char)grupo, mac_remota);
      }
      else if (tecla == 3) // F3
      {
        printf("Protocolo paro y espera.\n");
        printf("Estas en modo maestro\n");

        enviarArchivo(iface, mac_remota, "EProtoc.txt");
      }
      else if (tecla == 4) // F4
      {
        pulsarF4();
      }
    }
  }
  else
  {
    int salir = 0;
    MostrarMenuEsclavo();
    while (!salir)
    {
      if (kbhit())
      {
        int tecla = LeerTeclaEspecial();
        if (tecla == 27)
        {
          salir = 1;
        }
        else if (tecla == 1)
        {
          EjecutarModoInteractivoEsclavo(iface, (unsigned char)grupo, mac_remota);
          MostrarMenuEsclavo();
        }
        else if (tecla == 3) // F3
        {
          printf("Protocolo paro y espera.\n");
          printf("Estas en modo esclavo\n");

          recibirArchivo(iface, mac_remota, "RProtoc.txt");
          MostrarMenuEsclavo();
        }
      }

      ProcesarRecepcionEsclavo(iface, (unsigned char)grupo, mac_remota, 0);
    }
  }

  CloseAdapter(&iface);
  printf("Puerto cerrado\n");
  return 0;
}
