#! /bin/sh

case "$1" in
  start)
  	echo "Starting simpleserver"
  	start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
  	/usr/bin/aesdchar_load
  	;;
  stop)
    	echo "Starting simpLeserver"
  	start-stop-daemon -K -n aesdsocket
  	/usr/bin/aesdchar_unload
  	;;
  *)
  	echo "Usage: $0 {start|stop}"
  exit 1
esac

exit 0
