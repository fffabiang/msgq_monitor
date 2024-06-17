# msgq_monitor

## Description

Programa en C que monitorea el uso de colas de mensaje del sistema. Compilado en Red Hat 8.6.

Las colas monitoreadas seran aquellas disponibles a través del comando ipcs -q

## Ejecucion

Se requiere de lo siguiente:

1. Archivo binario monmsgq ubicado en $SIXDIR/bin
2. Archivo de configuracion monmsg.files ubicado en $SIXDIR/files

El programa debe ejecutarse de la siguiente manera:

> monmsgq _filename_

Donde _filename_ es la ruta absoluta del archivo de configuracion.

Por ejemplo:
> monmsgq /home/six/files/monmsgq.config

## Archivo de configuracion

Los parámetros del monitor están especificados en el archivo de configuracion monmsgq.config de la siguiente manera:

- LIMIT: Porcentaje máximo de uso que se puede permitir para las colas de mensaje. Valor de 1 a 99.

- MODE: Modo de monitoreo. 
    - A = Todas las colas. 
    - O = Solo colas de un owner. 
    - L = Solo una lista especifica de colas.

- OWNER: Solo en caso el modo sea O. Especifica el nombre del owner de las colas a monitorear. 

- LIST BEGIN: inicio de la lista de claves de cola (message queue keys). Solo valido para modo L.

- LIST END: fin de la lista de claves de cola (message queue keys). Solo valido para modo L.

- mail_recipients: lista de recipientes de correo de alerta
- mail_subject: asunto para el correo de alerta.
- mail_content_header: cabecera del contenido de correo de alerta.

- LOG_LEVEL: Nivel de log para el monitor. Los valores posibles (en orden de menor a mayor restriccion) son:
    - D (debug)
    - I (info)
    - N (notification)
    - E (error)
    
    Se recomienda D en ambiente de desarrollo y N en ambiente de produccion.

- LOG_DIR: ruta donde se escribira o generara el log. De ser una ruta invalida, se generará el log en la misma carpeta del binario monmsgq.





