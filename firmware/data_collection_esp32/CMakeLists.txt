cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(data_collection)

target_add_binary_data(data_collection.elf "main/broker.emqx.io-ca.crt" TEXT)
