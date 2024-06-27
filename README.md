# Server-Client

## Server:

- Dezactivam buffering la afisare si algoritmul Nagle;
- Deschidem doua socket-uri, unul TCP si unul UDP si le dam bind la server;
- Deschidem server-ul (run_server);
- Folosim un poll de file descriptori pentru a detecta input-uri;
(de la tastatura, de tip UDP, de tip TCP, mesaje de la clientii TCP)
- Ascultam si acceptam conexiuni TCP:
    -> daca clientul este deja conectat si online, afisam
    "Client <ID> already connected." si refuzam noua conexiune;
    -> daca clientul se reconecteaza, devine online si afisam
    "New client <ID> connected from <IP>:<PORT>.";
    -> daca clientul este nou, il adaugam in vectorul de clienti, devine online
    si afisam "New client <ID> connected from <IP>:<PORT>.";
- Primim packete tip UDP:
    -> parsam mesajul si il trimitem tuturor clientilor TCP abonati si online;
    -> pentru abonamentele de tip wildcard se verifica potentialul match:
        a. "+": sarim peste nivelul curent (marcat intre '/');
        b. "*": sarim peste nivelele topicului primit pana cand gasim un match
        cu nivelul imediat urmator de "*", sau pana ajungem la sfarsit;
        c. daca toate nivelele s-au potrivit, trimitem mesajul la abonati;
- Primim input de la tastatura:
    -> comanda "exit" inchide serverul si toti clientii conectati;
    -> orice alta comanda este invalida -> "Invalid command!" la stderr;
- Primim pachete (comenzi) de la clientii TCP in functie de tipul mesajului:
    -> 0 (exit): Inchidem conexiunea acelui client;
    -> 1 (subscribe): Memoram topicul la care s-a abonat clientul (daca e nou);
    -> 2 (unsubscribe): Stergem topicul respectiv;
- Inchidem toate conexiunile, inclusiv socketurile TCP si UDP.


## Subscriberi:

- Dezactivam buffering la afisare si algoritmul Nagle;
- Deschidem si conectam un socket la server;
- Trimitem ID-ul clientului la server;
- Folosim un poll de file descriptori pentru a detecta input-uri;
(de la tastatura, mesaje de la server)
- Primim comenzi de la tastatura si le trimitem serverului;
(exit/subscribe/unsubscribe)
- Primim mesaje de la server si le afisam la stdout;
- Inchidem conexiunea cu serverul.


## Common (functii pentru quick-flow):

Pentru un numar mare de mesaje trimise intr-un interval scurt de timp,
am implementat cele doua functii (send_all, recv_all) care ne asigura
ca fiecare mesaj a fost trimis in intregime.
Logica este simpla: se apeleaza in mod repetat "send", respectiv "recv",
trimitandu-se bucati din mesaj pana se trimite intreg continutul.
