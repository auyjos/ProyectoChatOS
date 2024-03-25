# Chat en C/C++ con Sockets

Este proyecto es una implementación de un sistema de chat en C/C++ basado en el trabajo propuesto por Bob Dugan y Erik Véliz en 2006. Su objetivo es reforzar los conocimientos sobre procesos, threads, concurrencia y comunicación entre procesos.

## Como correr
1. Clonar this repository
2. Correr los siguientes comandos en la terminal :
```
g++ server.cpp -lpthread -o server
g++ client.cpp -lpthread -o client
```
3. Para correr el servidor:
```
./server
```

4. Para correr el cliente :
```
./client
```

5. Para abrir múltiples clientes, repetir paso 4.
## Descripción

El proyecto consiste en dos partes principales:

1. **Servidor**: Mantiene una lista de todos los clientes/usuarios conectados al sistema. Solo puede existir un servidor durante una ejecución del sistema de chat. Se ejecuta como un proceso independiente y atiende las conexiones de los clientes con multithreading.

2. **Cliente**: Se conecta y registra con el servidor. Permite la existencia de uno o más clientes durante su ejecución. Cada cliente posee una pantalla para visualizar mensajes recibidos y enviar mensajes a otros usuarios.

## Instalación

Para compilar y ejecutar el servidor, se debe ejecutar el siguiente comando en la terminal:


Donde `<nombre_del_servidor>` es el nombre del programa y `<puerto_del_servidor>` es el número de puerto en el que el servidor está pendiente de conexiones.

## Funcionalidades del Servidor

- **Registro de usuarios**: El cliente envía su nombre de usuario al servidor. El servidor registra el nombre de usuario junto con la dirección IP de origen en la lista de usuarios conectados si no existe previamente.

- **Liberación de usuarios**: Cuando un usuario cierra su cliente de chat, el servidor elimina la información del cliente del listado de usuarios conectados.

- **Listado de usuarios conectados**: El cliente puede solicitar al servidor en cualquier momento el listado de usuarios conectados.

## Recursos Recomendados

Se recomienda leer la documentación de `man 7 ip` en sistemas Linux y sus enlaces relacionados para comprender los conceptos de sockets y comunicación entre procesos.

## Contribuciones

Las contribuciones son bienvenidas. Si desea contribuir al proyecto, por favor abra un issue o envíe una solicitud de extracción.

## Licencia

Este proyecto está bajo la licencia [MIT](LICENSE).
