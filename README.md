# Tema 2 PCom - Ion Daniel 325CC

### Surse externe utilizate pentru rezolvarea temei:
* laboratorul 7, de unde am preluat scheletul pentru a implementa server.c,
subscriber.c, common.c, common.h, helpers.h si Makefile.

### Implementare Server - server.c

In main, prelucrez portul primit in linia de comanda si initializez 2 socketi
pe care ii voi utiliza pentru a receptiona conexiunile clientilor TCP, 
respectiv UDP. Acesti socketi au numele, intuitiv, tcpfd, respectiv udpfd.
Dupa ce setez optiunile socketilor si dezactivez algoritmul lui Nagle folosind
TCP_NODELAY, dau bind celor 2 socketi si "activez" server-ul in functia
run_server(). In cadrul server-ului, am 2 vectori alocati dinamic, unul ce tine
cont de numarul conexiunilor actuale - poll_fds, si unul ce tine cont de
clientii care s-au inregistrat pe parcursul sesiunii actuale pe server -
clients. Pentru clienti, am realizat o structura specifica in care retin:
statusul curent al utilizatorului - pentru 0, utilizatorul este deconectat
si pentru 1, utilizatorul este conectat, socket-ul prin care utilizatorul
comunica cu server-ul, numarul de topics la care utilizatorul este abonat,
numarul maxim de topics pe care le poate avea actual utilizatorul (dinamic),
id-ul utilizatorului si topic-urile la care utilizatorul este abonat.
Intr-o bucla while(), apelez un poll() ce asteapta 4 tipuri de evenimente:
1. cerere de conexiune pe socketul TCP, ceea ce inseamna ca un client vrea
sa se conecteze la server. Inainte de a-l conecta, cer ID-ul clientului si
verific daca clientul este deconectat/nou, caz in care acesta se conecteaza
la server. In caz contrar, deconectez pe cel ce incearca sa se conecteze la
server cu ID conectat;
2. receptionare mesaj pe socketul UDP, ceea ce inseamna ca am primit un mesaj
de la un client UDP. In cazul acesta, transpun mesajul primit intr-un struct
UDP_message ce are ca si continut continutul payload-ului specificat in enunt,
astfel ca structura contine topicul, tipul payload-ului si continutul efectiv.
Dupa aceasta, transpun formatul UDP in format TCP, retinand informatiile
intr-un struct TCP_message, unde pastrez ip-ul clientului UDP, port-ul, topicul,
tipul payload-ului - de data aceasta in format string, necodificat, si contentul
prelucrat in functie de tipul primit in UDP. Caut dupa aceea acei clienti care
sunt abonati la topicul ce a trimis mesajul, verificand fie cu strcmp()
egalitatea intre topic-uri, fie cu check_wildcards(), unde, cu strtok_r, extrag
bucati din topic si verific daca am wildcard-uri in topicul inregistrat de
client. In functie de wildcard, avem:
- pentru text simplu, verific daca cel primit si cel din client sunt aceleasi;
- pentru *, merg intr-un while prin tokenurile primite pana gasesc urmatorul
token din client in tokenurile primite;
- pentru +, sar tokenul din received.
Daca clientul are topicul si este conectat, trimit mesajul in formatul dorit
catre client;
3. receptionare mesaj de la tastatura - verifica daca am primit exit, si in
acest caz inchid serverul impreuna cu clientii conectati si eliberez memoria
alocata;
4. receptionare date de la clienti - din partea clientilor, se pot primi 3
comenzi:
- exit, ce implica setarea statusului la 0 si a socketului pe -1;
- subscribe, unde extrag topicul din comanda si inserez topicul in topicurile
clientului pe care-l identific folosindu-ma de socketul ce a trimis mesajul;
- unsubscribe, unde scot topicul primit ca argument din topicurile clientului.
Pentru trimiterea datelor intre server si client si viceversa, ma folosesc de
un struct implementat in common.h, chat_packet, ce contine mesajul pe care
vreau sa-l transmit, ca sir de caractere, si lungimea acestui mesaj.

### Implementare Subscriber - subscriber.c

In main, ma conectez la ip-ul si port-ul primit si trimit catre server ID-ul
clientului pentru verificare. Pornesc clientul in run_client si intr-un while()
astept mesaje de la server sau trimit comenzi date in stdin catre server. In
cazul in care primesc mesajul exit de la server, trimit ca un ACK comanda
primita si inchid clientul. In caz contrar, afisez mesajul - acesta fie este
o confirmare legata de abonarea/dezabonarea efectuata, fie un mesaj de la un
topic la care clientul este abonat. In cazul in care trimit comenzi, pentru
exit, trimit comanda catre server pentru a seta clientul ca deconectat si
inchid clientul, iar in cazul in care se trimite subscribe/unsubscribe, trimit
comanda catre server, verificand faptul ca numarul de argumente trimis este
conform comenzii.

### Auxiliare

- common.c - sursa unde definesc recv_all si send_all, 2 functii ce asigura
faptul ca toata informatia din pachetul pe care vreau sa-l trimit este
transmisa;

- common.h - header ce contine structura pachetului transmis intre client si
server;

- helpers.h - header ce contine macro-ul DIE folosit pentru programare
defensiva.
