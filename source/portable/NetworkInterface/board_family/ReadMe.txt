Update NetworkInterface.c and include other files needed by FreeRTOS+TCP here.

Note: The NetworkInterface.c file in this directory is Not! to be used as is. The purpose of the file is to provide a template for writing a network interface.
Each network interface will have to provide concrete implementations of the functions in NetworkInterface.c.
See the following URL for an explanation of the file and its functions: https://freertos.org/Documentation/03-Libraries/02-FreeRTOS-plus/02-FreeRTOS-plus-TCP/10-Porting/03-Embedded_Ethernet_Porting