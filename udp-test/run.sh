if [[ $# -ne 2 ]]; then
    echo "Usage: $0 <udp-or-tcp> <number-of-tests>"
    exit 1
fi

pkill -f udp-server
pkill -f tcp-server

rm *.log

make

if [[ $1 == "udp" ]]; then
    ./udp-server &
elif [[ $1 == "tcp" ]]; then
    ./tcp-server &
elif [[ $1 == "both" ]]; then
    ./udp-server &
    ./tcp-server &
else
    echo "Invalid protocol specified. Use 'udp', 'tcp', or 'both'."
    exit 1
fi

if [[ $1 == "udp" || $1 == "both" ]]; then
    for i in $(seq 1 $2); do
        ./udp-client >> udp-test.log
    done
fi

if [[ $1 == "tcp" || $1 == "both" ]]; then
    for i in $(seq 1 $2); do
        ./tcp-client >> tcp-test.log
    done
fi


pkill -f udp-server
pkill -f tcp-server
