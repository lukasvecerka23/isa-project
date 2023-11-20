# ISA - TFTP Klient + Server Projekt 23/24
- Autor - Lukáš Večerka (xvecer30)
- Datum - 20.11.2023

## Popis
Program implementuje klienta a server pro přenos souborů po sítí implementovaného dle protokolu TFTP (Trivial File Transfer Protocol) dle RFC 1350 a dále i rozšiření specifikované v RFC 2347, 2348 a 2349.

### Popis rozšíření
- Blocksize - klient a server se shodnou na velikosti datového bloku pro přenos
- Timeout - klient a server se domluví na nastavení po jaké době se bude paket opakovaně zasílat, v případě že dojde k jeho ztrátě nebo zpoždění
- Transfer size 
    - klient při zápisu na server, může specifikovat jakou velikost má soubor, server mu může odpovědět chybou, protože nebude mít dostatek místa
    - klient při stahování souboru pošle transfer size s hodnotou `0`, server mu následně pošle velikost souboru, v případě že klient nemá dostatek místa na uložení souboru odesílá chybu

## Server
- Poslouchá na portu specifikováném při spuštění a konkurentně obsluhuje klienty.
- Podporovaný mód přenosu - netascii, octet
- Podporované rozšíření - Block size, Timeout, Transfer size

### Příklad spuštění
```bash
./tftp-server [-p port] <root-dir-path>
```
- `p` - port, na kterém server poslouchá pro příchozí RRQ a WRQ pakety
- `root-dir-path` - složka, ve které server spravuje soubory

### Klient
- Zasílá paket RRQ v případě že chce stahovat daný soubor ze serveru, nebo WRQ v případě že chce zapsat na server obsah standardního vstupu
- Podporovaný mód přenosu - netascii, octet
- Podporované rozšíření - Block size, Timeout, Transfer size

### Příklad použítí - upload
```bash
./tftp-client -h <hostname> [-p port] -t <filename-to-store>
```
- `h` - hostname nebo IP adresa serveru
- `p` - port, na kterém běží server
- `t` - název souboru, který bude uložen na serveru

### Příklad použítí - download
```bash
./tftp-client -h <hostname> [-p port] -f <filename-to-download> -t <path-to-store> 
```
- `h` - hostname nebo IP adresa serveru
- `p` - port, na kterém běží server
- `f` - cesta k souboru, na serveru
- `t` - cesta pro uložení souboru na klientovi

## Seznam odevzdaných souborů
### Server
- `src/server/main.cpp`
- `src/server/tftp_server.cpp`
- `include/server/tftp_server.hpp`
### Klient
- `src/client/main.cpp`
- `src/client/tftp_client.cpp`
- `include/client/tftp_client.hpp`
### Klient+Server
- `src/common/packets.cpp`
- `src/common/session.cpp`
- `include/common/packets.hpp`
- `include/common/session.hpp`
- `include/common/logger.hpp`
- `include/common/exceptions.hpp`

### Testy
- `test_tftp.py`

### Makefile
- `Makefile`

### Dokumentace
- `README.md`
- `dokumentace.pdf`