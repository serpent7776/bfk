#!/bin/sh
echo 'running hello_world_1'
[ "`$bfk helo.bf`" = "`cat helo.txt`" ] && echo "OK" || (echo "FAILED"; exit 1)
echo 'running hellobf'
[ "`$bfk HELLOBF.BF`" = "`cat HELLOBF.txt`" ] && echo "OK" || (echo "FAILED"; exit 1)
#echo 'running 99bottles'
#[ "`$bfk 99bot.bf`" = '.' ] && echo "OK" || (echo "FAILED"; exit 1)
#echo 'running mandelbrot'
#[ "`$bfk mandelbrot.b`" = '.' ] && echo "OK" || (echo "FAILED"; exit 1)
