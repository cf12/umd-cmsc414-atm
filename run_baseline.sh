#!/bin/bash 

docker run --name atm -ti --rm -v "$(pwd):/opt" baseline 
