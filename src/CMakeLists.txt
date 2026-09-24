cmake_minimum_required(VERSION 3.16)
project(huffmanArchiver C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()

add_executable(huff
    bitIO.c
    priorityQueue.c
    huffman.c
    compress.c
    decompress.c
    archive.c
    main.c
)

add_executable(testBitIo
    ../tests/testBitIo.c
    bitIO.c
)

add_executable(testPriorityQueue
    ../tests/testPriorityQueue.c
    priorityQueue.c
)

add_executable(testHuffman
    ../tests/testHuffman.c
    huffman.c
    priorityQueue.c
    bitIO.c
)

add_executable(testCompress
    ../tests/testCompress.c
    compress.c
    decompress.c
    huffman.c
    priorityQueue.c
    bitIO.c
)

add_executable(testArchive
    ../tests/testArchive.c
    archive.c
    compress.c
    decompress.c
    huffman.c
    priorityQueue.c
    bitIO.c
)

if(MSVC)
    target_compile_options(huff PRIVATE /W4)
else()
    target_compile_options(huff PRIVATE -Wall -Wextra -Wpedantic)
endif()

foreach(testTarget testBitIo testPriorityQueue testHuffman testCompress testArchive)
    add_test(
        NAME ${testTarget}
        COMMAND ${testTarget}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..
    )
    target_include_directories(${testTarget} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(MSVC)
        target_compile_options(${testTarget} PRIVATE /W4)
    else()
        target_compile_options(${testTarget} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
