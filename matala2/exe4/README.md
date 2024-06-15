chat:
./mync -i TCPS6060
./mync -o TCPClocalhost,5050
./mync -i TCPS6060 -o TCPClocalhost,5050
    ./mync -i TCPS5050
    /mync -o TCPClocalhost,6060

./mync -i UDPS6060
./mync -o UDPClocalhost,5050
./mync -i UDPS6060 -o UDPClocalhost,5050
    ./mync -i UDPS5050
    /mync -o UDPClocalhost,6060

./mync -i TCPS6060 -o UDPClocalhost,5050 
./mync -i UDPS6060 -o TCPClocalhost,5050

program:
./mync -e "./ttt 123456789" -i TCPS6060
./mync -e "./ttt 123456789" -b TCPS6060
./mync -e "./ttt 123456789" -o TCPClocalhost,5050
./mync -e "./ttt 123456789" -i TCPS6060 -o TCPClocalhost,5050

./mync -e "./ttt 123456789" -i UDPS6060
./mync -e "./ttt 123456789" -b UDPS6060
./mync -e "./ttt 123456789" -o UDPClocalhost,5050
./mync -e "./ttt 123456789" -i UDPS6060 -o UDPClocalhost,5050

./mync -e "./ttt 123456789" -i TCPS6060 -o UDPClocalhost,5050
./mync -e "./ttt 123456789" -i UDPS6060 -o TCPClocalhost,5050
./mync -e "./ttt 123456789" -i TCPMUXS6060
./mync -e "./ttt 123456789" -b TCPMUXS6060


