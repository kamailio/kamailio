#!/bin/sh

gcc -D_IPSEC_SPI_LIST_TEST spi_list_tests.c spi_list.c -o spi_list_tests
./spi_list_tests
