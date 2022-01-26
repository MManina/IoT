# IoT
Dernier TP fait dans la matière "Internet des objets" à l'école d'ingénieurs Sup Galilée

Ce TP avait pour but de faire communiquer 2 Zolertia RE-mote via le protocole UDP.

Pour pouvoir compiler le fichier, il faut avoir installé la bibliotèque contiki-ng, et renseigner le chemin vers le dossier "contiki-ng" depuis l'emplacement du Makefile dans la variable "CONTIKI".

Pour compiler et injecter le programme dans le RE-mote, utiliser la commande suivante :
make TARGET=zoul PORT=/dev/ttyUSB0 NOM-FICHIER.upload

Pour se connecter au RE-mote, utiliser la commande suivante :
make TARGET=zoul PORT=/dev/ttyUSB0 login

(ttyUSB0 est un exemple de port, à changer selon celui est utilisé)
