#!/bin/bash

gcc Server.c -o server -c
gcc tokenizer.c -o tokenizer -c
gcc -o aserver server tokenizer
rm server;
rm tokenizer;
