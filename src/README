Pentru implementarea temei am implementat 3 structuri ajutatoare:
-Client_file_info ce contine numele fisierului, numarul de segmente si segmentele
-Tracker_file_info ce contine numarul total de fisiere, dadtele despre fisiere
si doua matrici care retin informatii despre seeders si peers ai fiecarui fisier
-Owned_files care contine rank-ul clientului, numarul de fisiere detinute,
datele despre acestea, numarul de fisiere cerute si datele despre acestea

In cadrul pasilor de initializare clientii citesc informatiile din fisierele
corespunzatoare si le trimit catre tracker in cadrul functiei peer(). Apoi
asteapta mesajul ca pot porni thread-urile de download si upload.

TRACKERUL

Trackerul primeste initial datele despre fisiere de la toti clientii si le stocheaza
in structura Tracker_file_info. Dupa aceasta trimite un mesaj de confirmare ca a terminat
de primit datele de la toti clientii. Apoi asteapta cereri de la clienti.

Acesta poate primi 3 tipuri de cereri:
-GET <file_name> - clientul cere informatii despre un fisier
-ADD <file_name> <rank> - clientul a terminat de descarcat un fisier, iar trackerul
trebuie sa il marcheze ca seeder si sa il demarcheze ca peer
-FINISHED - clientul a terminat de descarcat toate fisierele, trackerul tine evidenta
numarului de clienti care au terminat de descarcat, iar daca toti au terminat, trimite
un mesaj de terminare catre toti clientii (sa se inchida thread-ul de upload)
-UPDATE <file_name> - clientul cere informatii actualizate despre peers si seeders

CLIENTUL

DOWNLOAD

Se parcurge lista cu fisiere si descarca pe rand fiecare fisier. Initial se cer de la
tracker informatii despre fisier, apoi se descarca fiecare segment de la seeders si peers.
Pentru a optimiza descarcarea am ales sa descarc alterantiv de la seeders si peers.
Segmentele sunt descarcate in ordine si sunt scrise in fisierul de output dupa
ce au fost descarcate toate. Segmentele sunt numarate pe masura ce sunt descarcate.
La fiecare pas caut un prima data un peer si apoi un seeder. O data ce am gasit
un peer care este posibil sa detina segmentul cautat, fac o cerere de descarcare.
Peer-ul va trimite hash-ul segmentului sau un hash vid daca nu detine segmentul
(simulez partea de ACK prin intermediul hash-ului). Daca hash-ul primit nu este
echivalent cu cel asteptat, inseamna ca peer-ul nu detine segmentul si caut un
alt peer. Este posibil sa nu gasesc un peer care detine segmentul.

Urmeaza apoi descarcarea de la un seeder. Seeder-ul va trimite segmentul cerut,
clientul va verifica daca segmentul este cel asteptat. Cautarea unui seeder si
a unui peer incepe de la ultimul peer/seeder gasit si se reseteaza cand se termina
lista de seeders/peers.

Dupa ce s-a terminat de descarcat un fisier, se trimite un mesaj catre tracker ca
s-a terminat de descarcat fisierul si se incepe descarcarea urmatorului fisier. Dupa
descaracarea tuturor fisierelor se trimite un mesaj catre tracker ca s-au terminat
de descarcat toate fisierele.

UPLOAD

Functia de upload poate primi 2 tipuri de mesaje:
-GET_CHUNK <file_name> <chunk_id> - clientul cere un segment, este trimis hash-ul
segmentului sau un hash vid daca nu detine segmentul (pentru a implementa atat partea
de ACK cat si partea corectitudine a segmentelor primite de client)
-STOP - toti clientii au terminat de descarcat, thread-ul de upload se inchide
